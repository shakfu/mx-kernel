#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "message_queue.h"

// Forward declarations for xeus types (avoid pulling in heavy headers)
namespace xeus {
class xkernel;
class xcontext;
} // namespace xeus

namespace mx {

// Forward declaration
class max_interpreter;

// Pimpl struct holding all C++ objects. Allocated with operator new, so
// Max's C allocator (object_alloc/sysmem_freeptr) never touches these.
//
// Lifetime note: the kernel thread is joined before the kernel is destroyed,
// so this struct is freed normally. That depends on the local timed-poll patch
// against xeus-zmq (patches/xeus-zmq-0003-*): without it the server loop never
// returns and the join would never complete.
//
// If the join does not finish within its deadline, shutdown falls back to
// detaching the thread and setting `leaked`. In that case the kernel, context
// and this struct are all leaked together, because the still-running thread
// holds pointers into them. The notify callback is cleared under its own mutex
// in either case, so the kernel thread can never reach a freed qelem.
struct t_kernel_impl {
    // Declared here and defined in types.cpp, where xkernel and xcontext are
    // complete. Without it every translation unit that instantiates this
    // struct would need the full xeus headers just to destroy it.
    t_kernel_impl();
    ~t_kernel_impl();

    std::unique_ptr<max_interpreter> interpreter;

    // Non-owning view of the interpreter after ownership moves into the
    // kernel. Valid for as long as the kernel is alive; cleared when it is
    // destroyed, which only happens after the server thread has been joined.
    max_interpreter* interpreter_view = nullptr;
    std::unique_ptr<xeus::xkernel> kernel;
    std::unique_ptr<xeus::xcontext> context;
    std::thread* kernel_thread = nullptr;
    std::string connection_file;

    // Kernel thread -> main thread (drained by the qelem callback).
    ThreadSafeQueue<OutletMessage> outlet_queue;
    // Main thread -> kernel thread, replying to the cell currently executing.
    ThreadSafeQueue<ResultMessage> result_queue;
    // Main thread -> kernel thread, output not tied to any cell.
    ThreadSafeQueue<ResultMessage> async_queue;

    // False once the Max object is being torn down. The kernel thread checks
    // this to abandon any wait in progress.
    std::atomic<bool> alive{true};

    // Set when a Jupyter client sends a shutdown_request. Cleared on start, so
    // a shutdown does not permanently disarm the object.
    std::atomic<bool> shutdown_requested{false};

    // Execution counter of the cell currently waiting for a result, or 0 when
    // no cell is waiting. Read by the main thread to stamp incoming results.
    std::atomic<int> current_execution{0};

    // Number of cells queued or in flight. Maintained by the interpreter on
    // the server thread; read by the main thread during shutdown, which waits
    // briefly for in-flight cells to be answered rather than dropping them.
    std::atomic<int> pending_executions{0};

    // Seconds to wait for a result before giving up. 0 or less means
    // fire-and-forget: the cell returns as soon as the code reaches the outlet.
    std::atomic<long> timeout{30};

    // Set by the kernel thread just before it returns, so the main thread can
    // wait for it with a deadline (std::thread has no timed join).
    std::atomic<bool> thread_finished{false};

    // Set only if a shutdown had to fall back to detaching the kernel thread.
    // In that case the kernel, context and this struct are leaked rather than
    // freed, because the detached thread may still reference them.
    bool leaked = false;

    // Wakes the main thread to drain outlet_queue. Set by the external to a
    // closure over qelem_set; cleared under m_notify_mutex during teardown so
    // the kernel thread cannot touch a freed qelem.
    void set_notifier(std::function<void()> fn) {
        std::lock_guard<std::mutex> lock(notify_mutex);
        notify = std::move(fn);
    }

    void clear_notifier() {
        std::lock_guard<std::mutex> lock(notify_mutex);
        notify = nullptr;
    }

    void notify_main_thread() {
        std::lock_guard<std::mutex> lock(notify_mutex);
        if (notify) {
            notify();
        }
    }

    std::mutex notify_mutex;
    std::function<void()> notify;
};

} // namespace mx
