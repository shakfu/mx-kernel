// Tests for max_interpreter -- the protocol semantics that used to be
// untestable because the interpreter called qelem_set directly. It now goes
// through t_kernel_impl::notify, which a test can supply.

#include "doctest.h"

#include "../interpreter.h"
#include "../types.h"
#include "../version.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "xeus/xinterpreter.hpp"

namespace nl = nlohmann;

namespace {

struct published_message {
    std::string msg_type;
    nl::json content;
    nl::json parent; // header of the request this was attributed to
};

// Drives a max_interpreter and records everything it publishes.
struct harness {
    mx::t_kernel_impl impl;
    mx::max_interpreter interp{&impl};
    std::vector<published_message> published;
    int notify_count = 0;

    harness() {
        impl.set_notifier([this] { ++notify_count; });
        interp.register_publisher(
            [this](xeus::xrequest_context ctx, const std::string& msg_type,
                   nl::json, nl::json content, xeus::buffer_sequence) {
                published.push_back({msg_type, std::move(content), ctx.header()});
            });
    }

    // Run a cell to completion and return the shell reply.
    //
    // Execution is asynchronous now: execute_request returns immediately and
    // the cell is completed from on_idle. This stands in for the server loop's
    // idle tick, and runs on this thread because the pending queue is only
    // ever touched from one thread.
    nl::json execute(const std::string& code, bool silent = false) {
        nl::json reply;
        bool done = false;
        interp.execute_request(
            xeus::xrequest_context{},
            [&](nl::json r) { reply = std::move(r); done = true; },
            code,
            xeus::execute_request_config{silent, true, false},
            nl::json::object());

        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::seconds(60);
        while (!done && std::chrono::steady_clock::now() < deadline) {
            interp.on_idle();
            if (done) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // A real server loop keeps ticking after a cell completes. A cell that
        // finished inline (fire-and-forget) would not otherwise have reached an
        // idle tick, which is where free-standing output gets published.
        interp.on_idle();
        return reply;
    }

    // Start a cell without driving it to completion, for tests that need to
    // observe the kernel while a cell is in flight.
    void begin(const std::string& code, nl::json* reply, bool* done) {
        interp.execute_request(
            xeus::xrequest_context{},
            [reply, done](nl::json r) { *reply = std::move(r); *done = true; },
            code,
            xeus::execute_request_config{false, true, false},
            nl::json::object());
    }

    // Answer the cell that is currently executing, from another thread.
    void reply_from_max(mx::ResultMessage msg) {
        msg.execution_counter = impl.current_execution.load();
        impl.result_queue.push(std::move(msg));
    }

    std::vector<published_message> of_type(const std::string& t) const {
        std::vector<published_message> out;
        for (const auto& p : published) {
            if (p.msg_type == t) out.push_back(p);
        }
        return out;
    }

    bool has_type(const std::string& t) const { return !of_type(t).empty(); }
};

// Run `fn` on a detached-but-joined helper thread while the cell is waiting.
// The interpreter blocks in execute_request, so the reply has to come from
// somewhere else -- exactly as it does in Max.
struct max_side {
    std::thread t;
    template <typename F>
    explicit max_side(F&& fn) : t(std::forward<F>(fn)) {}
    ~max_side() { if (t.joinable()) t.join(); }
};

// A request context that can be told apart in published output.
xeus::xrequest_context labelled(const std::string& label) {
    nl::json header;
    header["msg_id"] = label;
    return xeus::xrequest_context(std::move(header), xeus::xrequest_context::guid_list{});
}

} // namespace

TEST_CASE("execute_request returns without waiting for Max") {
    harness h;
    h.impl.timeout.store(30); // would be a 30s stall if execution blocked

    nl::json reply;
    bool done = false;

    const auto start = std::chrono::steady_clock::now();
    h.begin("nobody will answer this", &reply, &done);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    // This is the head-of-line fix: the server thread goes straight back to
    // polling, so other shell and control requests keep being served.
    CHECK(elapsed < std::chrono::milliseconds(200));
    CHECK(!done);
    CHECK(h.impl.pending_executions.load() == 1);

    // The code still reached Max immediately -- no idle tick needed.
    CHECK(h.impl.outlet_queue.size() == 1);

    // Let it finish so the harness tears down cleanly.
    h.impl.alive.store(false);
    h.interp.on_idle();
    CHECK(done);
}

TEST_CASE("a second cell is accepted while the first is still waiting") {
    harness h;
    h.impl.timeout.store(30);

    nl::json r1, r2;
    bool d1 = false, d2 = false;

    h.begin("first", &r1, &d1);
    h.begin("second", &r2, &d2);

    CHECK(h.impl.pending_executions.load() == 2);
    CHECK(!d1);
    CHECK(!d2);

    // Only the first has been handed to Max: cells run one at a time, which is
    // what the shell channel promises.
    CHECK(h.impl.outlet_queue.size() == 1);

    // Answer the first.
    mx::ResultMessage a;
    a.text = "one";
    a.execution_counter = h.impl.current_execution.load();
    h.impl.result_queue.push(std::move(a));
    h.interp.on_idle();

    CHECK(d1);
    CHECK(!d2);
    CHECK(r1["status"] == "ok");

    // Now the second has been handed to Max.
    CHECK(h.impl.outlet_queue.size() == 2);
    CHECK(h.impl.pending_executions.load() == 1);

    mx::ResultMessage b;
    b.text = "two";
    b.execution_counter = h.impl.current_execution.load();
    h.impl.result_queue.push(std::move(b));
    h.interp.on_idle();

    CHECK(d2);
    CHECK(r2["status"] == "ok");
    CHECK(h.impl.pending_executions.load() == 0);
}

TEST_CASE("a deferred cell publishes under its own request context") {
    harness h;
    h.impl.timeout.store(30);

    nl::json r1, r2;
    bool d1 = false, d2 = false;

    // Cell one starts and is left waiting.
    h.interp.execute_request(labelled("cell-one"),
                             [&](nl::json r) { r1 = std::move(r); d1 = true; },
                             "first", xeus::execute_request_config{false, true, false},
                             nl::json::object());

    // Cell two arrives while cell one is in flight. xeus overwrites its single
    // request context here -- the reason get_request_context is overridden.
    h.interp.execute_request(labelled("cell-two"),
                             [&](nl::json r) { r2 = std::move(r); d2 = true; },
                             "second", xeus::execute_request_config{false, true, false},
                             nl::json::object());

    // Answer cell one, with some streamed output first.
    mx::ResultMessage progress;
    progress.stream_name = "stdout";
    progress.text = "from cell one";
    progress.execution_counter = h.impl.current_execution.load();
    h.impl.result_queue.push(std::move(progress));

    mx::ResultMessage a;
    a.text = "one";
    a.execution_counter = h.impl.current_execution.load();
    h.impl.result_queue.push(std::move(a));

    h.interp.on_idle();
    REQUIRE(d1);

    auto streams = h.of_type("stream");
    REQUIRE(streams.size() == 1);
    auto results = h.of_type("execute_result");
    REQUIRE(results.size() == 1);

    // Both must be attributed to cell one, not to whichever request arrived
    // last. Getting this wrong puts cell one's output on cell two.
    CHECK(streams[0].parent["msg_id"] == "cell-one");
    CHECK(results[0].parent["msg_id"] == "cell-one");
}

TEST_CASE("cells still in flight are answered when the kernel shuts down") {
    harness h;
    h.impl.timeout.store(30);

    nl::json reply;
    bool done = false;
    h.begin("waiting on max", &reply, &done);
    REQUIRE(!done);

    // A client requests shutdown while the cell waits.
    h.interp.shutdown_request();
    h.interp.on_idle();

    // Dropping the cell would leave the client waiting for a reply forever.
    CHECK(done);
    CHECK(reply["status"] == "error");
    CHECK(reply["ename"] == "MaxShutdown");
    CHECK(h.impl.pending_executions.load() == 0);
}

TEST_CASE("execute pushes code to the outlet and wakes the main thread") {
    harness h;
    h.impl.timeout.store(0); // fire and forget, no waiting

    nl::json reply = h.execute("hello world");

    CHECK(reply["status"] == "ok");
    CHECK(h.notify_count == 1);

    auto msg = h.impl.outlet_queue.try_pop();
    REQUIRE(msg.has_value());
    CHECK(msg->selector == "code");
    CHECK(msg->outlet_index == 0);
    REQUIRE(msg->atoms.size() == 2);
    CHECK(std::get<std::string>(msg->atoms[0]) == "execute");
    CHECK(std::get<std::string>(msg->atoms[1]) == "hello world");
}

TEST_CASE("execution_input is published exactly once") {
    harness h;
    h.impl.timeout.store(0);

    h.execute("hello");

    // xeus::execute_request publishes it; the interpreter must not repeat it.
    CHECK(h.of_type("execute_input").size() == 1);
}

TEST_CASE("a result from Max completes the cell") {
    harness h;
    h.impl.timeout.store(5);

    max_side responder([&h] {
        while (h.impl.current_execution.load() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        mx::ResultMessage r;
        r.text = "42";
        h.reply_from_max(std::move(r));
    });

    nl::json reply = h.execute("what is six times seven");

    CHECK(reply["status"] == "ok");

    auto results = h.of_type("execute_result");
    REQUIRE(results.size() == 1);
    CHECK(results[0].content["data"]["text/plain"] == "42");
}

TEST_CASE("a result carrying a mime type is published under that mime type") {
    harness h;
    h.impl.timeout.store(5);

    max_side responder([&h] {
        while (h.impl.current_execution.load() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        mx::ResultMessage r;
        r.text = "{\"freq\":440}";
        r.mime_type = "application/json";
        h.reply_from_max(std::move(r));
    });

    h.execute("dump state");

    auto results = h.of_type("execute_result");
    REQUIRE(results.size() == 1);
    CHECK(results[0].content["data"]["application/json"] == "{\"freq\":440}");
}

TEST_CASE("an error result fails the cell") {
    harness h;
    h.impl.timeout.store(5);

    max_side responder([&h] {
        while (h.impl.current_execution.load() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        mx::ResultMessage r;
        r.error_name = "MaxError";
        r.error_value = "no such object";
        h.reply_from_max(std::move(r));
    });

    nl::json reply = h.execute("bogus");

    CHECK(reply["status"] == "error");
    CHECK(reply["ename"] == "MaxError");
    CHECK(reply["evalue"] == "no such object");
    CHECK(h.has_type("error"));
}

TEST_CASE("stream output does not end the cell") {
    harness h;
    h.impl.timeout.store(5);

    max_side responder([&h] {
        while (h.impl.current_execution.load() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        mx::ResultMessage progress;
        progress.stream_name = "stdout";
        progress.text = "working";
        h.reply_from_max(std::move(progress));

        mx::ResultMessage done;
        done.text = "finished";
        h.reply_from_max(std::move(done));
    });

    nl::json reply = h.execute("long job");

    CHECK(reply["status"] == "ok");

    auto streams = h.of_type("stream");
    REQUIRE(streams.size() == 1);
    CHECK(streams[0].content["text"] == "working\n");

    auto results = h.of_type("execute_result");
    REQUIRE(results.size() == 1);
    CHECK(results[0].content["data"]["text/plain"] == "finished");
}

TEST_CASE("stream output is newline terminated") {
    harness h;
    h.impl.timeout.store(5);

    max_side responder([&h] {
        while (h.impl.current_execution.load() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        for (const char* line : {"working on it", "still going"}) {
            mx::ResultMessage progress;
            progress.stream_name = "stdout";
            progress.text = line;
            h.reply_from_max(std::move(progress));
        }
        mx::ResultMessage done;
        done.text = "finished";
        h.reply_from_max(std::move(done));
    });

    h.execute("long job");

    auto streams = h.of_type("stream");
    REQUIRE(streams.size() == 2);

    // Jupyter concatenates stream text verbatim. Without the newline the two
    // lines run together and the next Out[n] collides with them.
    CHECK(streams[0].content["text"] == "working on it\n");
    CHECK(streams[1].content["text"] == "still going\n");
}

TEST_CASE("stream output already ending in a newline is not doubled") {
    harness h;
    h.impl.timeout.store(0);

    mx::ResultMessage note;
    note.stream_name = "stdout";
    note.text = "already terminated\n";
    h.impl.async_queue.push(std::move(note));

    h.execute("anything");

    auto streams = h.of_type("stream");
    REQUIRE(streams.size() == 1);
    CHECK(streams[0].content["text"] == "already terminated\n");
}

TEST_CASE("no result within the timeout reports an error, not success") {
    harness h;
    h.impl.timeout.store(1);

    const auto start = std::chrono::steady_clock::now();
    nl::json reply = h.execute("nobody is listening");
    const auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK(reply["status"] == "error");
    CHECK(reply["ename"] == "MaxTimeout");
    CHECK(elapsed >= std::chrono::milliseconds(900));
}

TEST_CASE("timeout of zero returns immediately without waiting") {
    harness h;
    h.impl.timeout.store(0);

    const auto start = std::chrono::steady_clock::now();
    nl::json reply = h.execute("fire and forget");
    const auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK(reply["status"] == "ok");
    CHECK(elapsed < std::chrono::milliseconds(500));
}

TEST_CASE("a result left over from an earlier cell cannot answer the next one") {
    harness h;
    h.impl.timeout.store(1);

    // Max replied twice to cell 1; the extra reply is still queued.
    mx::ResultMessage stale;
    stale.text = "stale answer";
    stale.execution_counter = 1;
    h.impl.result_queue.push(std::move(stale));

    nl::json reply = h.execute("cell two");

    // Cell two must not be answered by cell one's leftover.
    CHECK(reply["status"] == "error");
    CHECK(reply["ename"] == "MaxTimeout");

    auto results = h.of_type("execute_result");
    CHECK(results.empty());
}

TEST_CASE("a result stamped for another cell is discarded, not delivered") {
    harness h;
    h.impl.timeout.store(1);

    max_side responder([&h] {
        while (h.impl.current_execution.load() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // Deliberately wrong counter -- a late reply to a previous cell.
        mx::ResultMessage r;
        r.text = "belongs to someone else";
        r.execution_counter = h.impl.current_execution.load() + 99;
        h.impl.result_queue.push(std::move(r));
    });

    nl::json reply = h.execute("cell");

    CHECK(reply["status"] == "error");
    CHECK(reply["ename"] == "MaxTimeout");
    CHECK(h.of_type("execute_result").empty());
}

TEST_CASE("async output queued outside a cell is flushed on an idle tick") {
    harness h;
    h.impl.timeout.store(0);

    mx::ResultMessage note;
    note.stream_name = "stdout";
    note.text = "max said something";
    h.impl.async_queue.push(std::move(note));

    h.execute("anything");

    auto streams = h.of_type("stream");
    REQUIRE(streams.size() == 1);
    CHECK(streams[0].content["name"] == "stdout");
    CHECK(streams[0].content["text"] == "max said something\n");
    CHECK(h.impl.async_queue.empty());
}

TEST_CASE("current_execution is set during a cell and cleared afterwards") {
    harness h;
    h.impl.timeout.store(5);

    int seen_during = -1;

    max_side responder([&h, &seen_during] {
        while (h.impl.current_execution.load() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        seen_during = h.impl.current_execution.load();
        mx::ResultMessage r;
        r.text = "ok";
        h.reply_from_max(std::move(r));
    });

    h.execute("cell");

    CHECK(seen_during == 1);
    CHECK(h.impl.current_execution.load() == 0);
}

TEST_CASE("current_execution is cleared even when the cell errors") {
    harness h;
    h.impl.timeout.store(5);

    max_side responder([&h] {
        while (h.impl.current_execution.load() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        mx::ResultMessage r;
        r.error_name = "MaxError";
        r.error_value = "boom";
        h.reply_from_max(std::move(r));
    });

    nl::json reply = h.execute("cell");

    CHECK(reply["status"] == "error");
    CHECK(h.impl.current_execution.load() == 0);
}

TEST_CASE("teardown releases a cell that is waiting") {
    harness h;
    h.impl.timeout.store(30); // would otherwise block the test for 30s

    max_side teardown([&h] {
        while (h.impl.current_execution.load() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        h.impl.alive.store(false);
    });

    const auto start = std::chrono::steady_clock::now();
    nl::json reply = h.execute("waiting forever");
    const auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK(reply["status"] == "error");
    CHECK(elapsed < std::chrono::seconds(5));
}

TEST_CASE("shutdown_request does not permanently disarm the interpreter") {
    harness h;
    h.impl.timeout.store(30);

    h.interp.shutdown_request();
    CHECK(h.impl.shutdown_requested.load());

    // alive must be untouched: clearing it here is what used to make every
    // later cell fall straight through to the no-response path.
    CHECK(h.impl.alive.load());

    // The patch is told about it via the status outlet.
    auto msg = h.impl.outlet_queue.try_pop();
    REQUIRE(msg.has_value());
    CHECK(msg->selector == "shutdown");
    CHECK(msg->outlet_index == 1);

    // Clearing the flag, as kernel_start does, restores normal waiting.
    h.impl.shutdown_requested.store(false);
    h.impl.timeout.store(1);

    nl::json reply = h.execute("after shutdown");
    CHECK(reply["ename"] == "MaxTimeout"); // waited, rather than skipping
}

TEST_CASE("a locally initiated stop does not report a client shutdown") {
    harness h;

    // This is what kernel_shutdown does before calling xkernel::stop(), which
    // in turn calls shutdown_request() on the interpreter.
    h.impl.shutdown_requested.store(true);
    h.interp.shutdown_request();

    // The patch asked for this stop; telling it "shutdown" would imply a
    // client did.
    CHECK(h.impl.outlet_queue.empty());
    CHECK(h.impl.shutdown_requested.load());
}

TEST_CASE("silent cells publish nothing") {
    harness h;
    h.impl.timeout.store(0);

    nl::json reply = h.execute("quiet", /*silent=*/true);

    CHECK(reply["status"] == "ok");
    CHECK(h.published.empty());

    // The code still reaches Max.
    CHECK(h.impl.outlet_queue.size() == 1);
}

TEST_CASE("kernel_info reports the max language") {
    harness h;

    nl::json info = h.interp.kernel_info_request();

    CHECK(info["implementation"] == "max_kernel");
    CHECK(info["protocol_version"] == "5.3");
    CHECK(info["language_info"]["name"] == "max");
    // Without this clients warn that no lexer exists for language "max".
    CHECK(info["language_info"]["pygments_lexer"] == "text");
    CHECK(info["implementation_version"] == MX_KERNEL_VERSION);
}

TEST_CASE("a cleared notifier is not called") {
    harness h;
    h.impl.timeout.store(0);

    h.impl.clear_notifier();
    h.execute("hello");

    CHECK(h.notify_count == 0);
    // The message is still queued; only the wake-up is suppressed.
    CHECK(h.impl.outlet_queue.size() == 1);
}
