#pragma once

#include "xeus/xinterpreter.hpp"
#include "message_queue.h"

namespace nl = nlohmann;

namespace mx {

struct t_kernel_impl; // forward

class max_interpreter : public xeus::xinterpreter {
public:
    // The interpreter borrows pointers to the impl's queues and notifier.
    // The impl must outlive the interpreter -- see the lifetime note in types.h.
    max_interpreter(t_kernel_impl* impl);
    virtual ~max_interpreter() = default;

    // Called from the server thread when the poll times out. Publishing must
    // happen on that thread: the IOPub socket belongs to it.
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

    // Publish anything Max queued outside of a cell as stream output.
    void flush_async_output();

    t_kernel_impl* m_impl;
};

} // namespace mx
