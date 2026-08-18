#pragma once

#include "xeus/xinterpreter.hpp"
#include "xeus/xrequest_context.hpp"
#include "message_queue.h"

#include <chrono>
#include <deque>
#include <string>

namespace nl = nlohmann;

namespace mx {

struct t_kernel_impl; // forward

// Jupyter interpreter that hands code to a Max patch and waits for the patch
// to answer.
//
// Execution is asynchronous: execute_request_impl hands the code to Max and
// returns immediately without replying, so the server loop goes straight back
// to polling. Cells are completed later from on_idle(). This is what xeus
// intends -- execute_request is registered as a non-blocking handler and the
// reply callback carries its own request context -- and it is what keeps the
// control channel answerable while a cell is waiting on Max.
//
// Threading: execute_request_impl and on_idle both run on the server thread,
// so the pending queue needs no lock. Only the message queues in t_kernel_impl
// are touched by Max's thread, and those are synchronised.
class max_interpreter : public xeus::xinterpreter {
public:
    // The interpreter borrows pointers to the impl's queues and notifier.
    // The impl must outlive the interpreter -- see the lifetime note in types.h.
    max_interpreter(t_kernel_impl* impl);
    virtual ~max_interpreter() = default;

    // Called from the server thread when the poll times out. Drives pending
    // cells to completion and publishes queued output. Publishing must happen
    // on that thread: the IOPub socket belongs to it.
    void on_idle();

private:
    void configure_impl() override;

    void execute_request_impl(send_reply_callback cb,
                              int execution_counter,
                              const std::string& code,
                              xeus::execute_request_config config,
                              nl::json user_expressions) override;

    nl::json complete_request_impl(const std::string& code,
                                   int cursor_pos) override;

    nl::json inspect_request_impl(const std::string& code,
                                  int cursor_pos,
                                  int detail_level) override;

    nl::json is_complete_request_impl(const std::string& code) override;

    nl::json kernel_info_request_impl() override;

    void shutdown_request_impl() override;

    // xeus keeps one request context and every publish_* reads it. Deferring a
    // reply means a later request can overwrite it before the earlier cell has
    // published its output, which would attribute that output to the wrong
    // cell. Overriding these lets a deferred cell publish under its own
    // context while dispatch keeps using the current one.
    void set_request_context(xeus::xrequest_context context) override;
    const xeus::xrequest_context& get_request_context() const noexcept override;

    // A cell that has been handed to Max and is waiting for a reply.
    struct pending_execution {
        send_reply_callback cb;
        xeus::xrequest_context context;
        int counter = 0;
        std::string code;
        bool silent = false;
        long timeout_s = 0;
        std::chrono::steady_clock::time_point deadline;
        bool started = false;
    };

    // Advance the queue as far as it can go without blocking.
    void pump();
    // Returns true if the front cell completed and was removed.
    bool service_front();
    void start_front();
    void complete_front(nl::json reply);

    // Publish anything Max queued outside of a cell as stream output.
    void flush_async_output();

    std::deque<pending_execution> m_pending;

    // Context of the request xeus is currently dispatching.
    xeus::xrequest_context m_dispatch_context;
    // Context of the cell being serviced, when that is not the dispatched one.
    xeus::xrequest_context m_active_context;
    bool m_has_active = false;

    t_kernel_impl* m_impl;
};

} // namespace mx
