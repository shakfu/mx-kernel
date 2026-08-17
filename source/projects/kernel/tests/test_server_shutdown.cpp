// Integration tests for the timed-poll patch against xeus-zmq.
//
// These start a real xkernel with a real ZMQ server bound to loopback, so they
// exercise the exact shutdown path that used to hang Max. No Max SDK involved.
//
// Every test that could hang runs under a watchdog: if the operation does not
// finish in time the process reports the hang and exits non-zero, rather than
// blocking the suite (and CI) forever. A hang here is precisely the regression
// these tests exist to catch, so it must fail loudly rather than hang quietly.

#include "doctest.h"

#include "../interpreter.h"
#include "../connection.h"
#include "../types.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus/xhistory_manager.hpp"
#include "xeus/xsystem.hpp"
#include "xeus-zmq/xserver_zmq.hpp"
#include "xeus-zmq/xzmq_context.hpp"

using namespace std::chrono_literals;

namespace {

// Hard-fails the process if the guarded section overruns. Needed because a
// genuine regression here deadlocks: there is no thread left to report it.
class watchdog {
public:
    watchdog(std::chrono::seconds limit, std::string what)
        : m_what(std::move(what)) {
        m_thread = std::thread([this, limit] {
            const auto deadline = std::chrono::steady_clock::now() + limit;
            while (!m_done.load()) {
                if (std::chrono::steady_clock::now() > deadline) {
                    std::fprintf(stderr,
                                 "\n*** WATCHDOG: '%s' did not complete in %llds.\n"
                                 "*** The server loop or destructor is hung -- this is\n"
                                 "*** the shutdown deadlock the timed-poll patch prevents.\n",
                                 m_what.c_str(),
                                 static_cast<long long>(limit.count()));
                    std::fflush(stderr);
                    std::_Exit(70);
                }
                std::this_thread::sleep_for(20ms);
            }
        });
    }

    ~watchdog() {
        m_done.store(true);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

private:
    std::atomic<bool> m_done{false};
    std::string m_what;
    std::thread m_thread;
};

// A running kernel, assembled the same way external.cpp assembles one.
struct running_kernel {
    mx::t_kernel_impl impl;
    std::unique_ptr<xeus::xkernel> kernel;
    std::thread thread;
    std::atomic<int> idle_ticks{0};

    running_kernel() {
        xeus::xconfiguration config = mx::create_kernel_configuration();
        auto context = std::unique_ptr<xeus::xcontext>(xeus::make_zmq_context());

        std::unique_ptr<xeus::xinterpreter> interp(new mx::max_interpreter(&impl));

        kernel = std::make_unique<xeus::xkernel>(
            config,
            xeus::get_user_name(),
            std::move(context),
            std::move(interp),
            xeus::make_xserver_default,
            xeus::make_in_memory_history_manager());

        auto* server = dynamic_cast<xeus::xserver_zmq*>(&kernel->get_server());
        REQUIRE(server != nullptr);

        server->set_poll_timeout(20);
        server->set_idle_callback([this] { idle_ticks.fetch_add(1); });
    }

    void start() {
        thread = std::thread([this] { kernel->start(); });
    }

    // Wait until the poll loop is demonstrably running.
    bool wait_until_serving(std::chrono::milliseconds limit = 5000ms) {
        const auto deadline = std::chrono::steady_clock::now() + limit;
        while (std::chrono::steady_clock::now() < deadline) {
            if (idle_ticks.load() > 0) return true;
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }

    ~running_kernel() {
        if (thread.joinable()) {
            thread.join();
        }
    }
};

} // namespace

TEST_CASE("the server loop polls rather than blocking forever") {
    watchdog guard(30s, "idle polling");

    running_kernel rk;
    rk.start();

    // With poll_channels(-1) no idle tick could ever fire: the loop parks
    // until a client sends something.
    REQUIRE(rk.wait_until_serving());
    CHECK(rk.idle_ticks.load() > 0);

    const int before = rk.idle_ticks.load();
    std::this_thread::sleep_for(200ms);
    CHECK(rk.idle_ticks.load() > before); // still ticking, not a one-off

    rk.kernel->stop();
    rk.thread.join();
}

TEST_CASE("stop() is observed promptly and the loop exits") {
    watchdog guard(30s, "stop and join");

    running_kernel rk;
    rk.start();
    REQUIRE(rk.wait_until_serving());

    const auto start = std::chrono::steady_clock::now();
    rk.kernel->stop();
    rk.thread.join();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    // No message is ever sent to this kernel, so under the old blocking poll
    // the join would never return.
    CHECK(elapsed < 5s);
}

TEST_CASE("the kernel destructor completes after stop -- no leak required") {
    watchdog guard(30s, "kernel destruction");

    auto rk = std::make_unique<running_kernel>();
    rk->start();
    REQUIRE(rk->wait_until_serving());

    rk->kernel->stop();
    rk->thread.join();

    // This is the assertion that matters. Destroying the xkernel destroys the
    // server, which joins the publisher and heartbeat threads. Those threads
    // only exit because the loop reached stop_channels() -- which it only
    // reaches because the poll now times out. This destructor hanging is the
    // bug that made Max need a force quit.
    const auto start = std::chrono::steady_clock::now();
    rk->kernel.reset();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK(elapsed < 5s);

    rk.reset();
}

TEST_CASE("a kernel can be started and destroyed repeatedly") {
    watchdog guard(60s, "restart cycles");

    for (int i = 0; i < 3; ++i) {
        running_kernel rk;
        rk.start();
        REQUIRE(rk.wait_until_serving());
        rk.kernel->stop();
        rk.thread.join();
        rk.kernel.reset();
    }

    CHECK(true); // reaching here without hanging is the assertion
}

TEST_CASE("the idle callback can publish queued output on the server thread") {
    watchdog guard(30s, "idle output flush");

    running_kernel rk;

    // Replace the counting callback with one that drains the async queue, as
    // external.cpp does. Publishing must happen on this thread: the IOPub
    // socket belongs to it.
    std::atomic<int> flushed{0};
    auto* server = dynamic_cast<xeus::xserver_zmq*>(&rk.kernel->get_server());
    REQUIRE(server != nullptr);
    server->set_idle_callback([&rk, &flushed] {
        rk.idle_ticks.fetch_add(1);
        while (auto out = rk.impl.async_queue.try_pop()) {
            flushed.fetch_add(1);
        }
    });

    rk.start();
    REQUIRE(rk.wait_until_serving());

    // Queue output the way `print` does when no cell is running.
    mx::ResultMessage note;
    note.stream_name = "stdout";
    note.text = "from max, with no cell running";
    rk.impl.async_queue.push(std::move(note));

    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (flushed.load() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }

    CHECK(flushed.load() == 1);
    CHECK(rk.impl.async_queue.empty());

    rk.kernel->stop();
    rk.thread.join();
}

TEST_CASE("a negative poll timeout restores blocking behaviour") {
    // Guards the escape hatch: an embedder that wants the old semantics can
    // still ask for them. Nothing is asserted about stop() here, because with
    // a blocking poll stop() is precisely what does not work.
    watchdog guard(30s, "blocking poll configuration");

    running_kernel rk;
    auto* server = dynamic_cast<xeus::xserver_zmq*>(&rk.kernel->get_server());
    REQUIRE(server != nullptr);

    server->set_poll_timeout(-1);
    CHECK(server->get_poll_timeout() == -1);

    // Put it back so the kernel can actually be shut down.
    server->set_poll_timeout(20);
    CHECK(server->get_poll_timeout() == 20);

    rk.start();
    REQUIRE(rk.wait_until_serving());
    rk.kernel->stop();
    rk.thread.join();
}
