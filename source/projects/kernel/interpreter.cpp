#include "interpreter.h"
#include "types.h"

#include <chrono>
#include <thread>

// qelem_set is a Max SDK function. We call it via a function pointer to avoid
// pulling in Max headers. The external.cpp sets up the qelem and stores the
// pointer in t_kernel_impl::qelem. We declare the C function here.
extern "C" {
    void qelem_set(void*);
}

namespace mx {

max_interpreter::max_interpreter(t_kernel_impl* impl)
    : m_impl(impl)
{
    xeus::register_interpreter(this);
}

void max_interpreter::configure_impl() {
    // Nothing to configure
}

void max_interpreter::execute_request_impl(send_reply_callback cb,
                                           int execution_counter,
                                           const std::string& code,
                                           xeus::execute_request_config config,
                                           nl::json user_expressions) {
    // 1. Push an OutletMessage for the main thread to deliver via outlet_anything
    OutletMessage msg;
    msg.selector = "code";
    msg.atoms.push_back(std::string("execute"));
    msg.atoms.push_back(code);
    msg.outlet_index = 0; // left outlet
    msg.execution_counter = execution_counter;

    m_impl->outlet_queue.push(std::move(msg));

    // Wake the main thread's qelem to drain the outlet queue
    if (m_impl->qelem) {
        qelem_set(m_impl->qelem);
    }

    // 2. Publish execution input (In[n]:) -- this goes through xeus IOPub, thread-safe
    if (!config.silent) {
        publish_execution_input(code, execution_counter);
    }

    // 3. Poll result_queue with bounded wait
    long timeout_s = m_impl->timeout;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);

    bool got_result = false;
    while (std::chrono::steady_clock::now() < deadline && m_impl->alive.load()) {
        auto result = m_impl->result_queue.try_pop();
        if (result.has_value()) {
            const ResultMessage& r = result.value();

            if (r.is_error()) {
                publish_execution_error(r.error_name, r.error_value, {});
                nl::json reply;
                reply["status"] = "error";
                reply["ename"] = r.error_name;
                reply["evalue"] = r.error_value;
                reply["traceback"] = nl::json::array();
                cb(reply);
                return;
            }

            if (!r.stream_name.empty()) {
                publish_stream(r.stream_name, r.text);
            } else {
                // Execution result
                nl::json data;
                if (!r.mime_type.empty()) {
                    data[r.mime_type] = r.text;
                } else {
                    data["text/plain"] = r.text;
                }
                publish_execution_result(execution_counter, std::move(data), nl::json::object());
            }

            got_result = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 4. Timeout or no result
    if (!got_result && !config.silent) {
        std::string output = "Sent to Max (no response): " + code;
        publish_stream("stdout", output);
    }

    // 5. Reply ok
    nl::json reply;
    reply["status"] = "ok";
    reply["execution_count"] = execution_counter;
    reply["user_expressions"] = nl::json::object();
    reply["payload"] = nl::json::array();
    cb(reply);
}

nl::json max_interpreter::complete_request_impl(const std::string& code,
                                                int cursor_pos) {
    nl::json reply;
    reply["matches"] = nl::json::array();
    reply["cursor_start"] = cursor_pos;
    reply["cursor_end"] = cursor_pos;
    reply["status"] = "ok";
    return reply;
}

nl::json max_interpreter::inspect_request_impl(const std::string& code,
                                               int cursor_pos,
                                               int detail_level) {
    nl::json reply;
    reply["status"] = "ok";
    reply["found"] = false;
    reply["data"] = nl::json::object();
    reply["metadata"] = nl::json::object();
    return reply;
}

nl::json max_interpreter::is_complete_request_impl(const std::string& code) {
    nl::json reply;
    reply["status"] = "complete";
    return reply;
}

nl::json max_interpreter::kernel_info_request_impl() {
    nl::json reply;
    reply["protocol_version"] = "5.3";
    reply["implementation"] = "max_kernel";
    reply["implementation_version"] = "0.1.0";

    nl::json language_info;
    language_info["name"] = "max";
    language_info["version"] = "8.0";
    language_info["mimetype"] = "text/x-maxmsp";
    language_info["file_extension"] = ".maxpat";

    reply["language_info"] = language_info;
    reply["banner"] = "Max/MSP Jupyter Kernel v0.1.0";
    reply["help_links"] = nl::json::array();

    return reply;
}

void max_interpreter::shutdown_request_impl() {
    m_impl->alive.store(false);
}

} // namespace mx
