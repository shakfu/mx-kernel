#include "interpreter.h"
#include "types.h"
#include "version.h"

#include <algorithm>
#include <chrono>

namespace mx {

namespace {

// Longest single wait before re-checking the shutdown flags. Bounds how long
// teardown can be delayed by a cell that is waiting for a result.
constexpr std::chrono::milliseconds k_wait_slice{100};

// Jupyter stream output is raw text: the client inserts no line breaks of its
// own. One Max `print` message is one line, so terminate it here -- otherwise
// consecutive messages run together, and the next Out[n] collides with the
// last of them. Text that already ends in a newline is left alone.
std::string as_line(const std::string& text) {
    if (!text.empty() && text.back() == '\n') {
        return text;
    }
    return text + "\n";
}

// Restores current_execution on every exit path from execute_request_impl.
struct execution_scope {
    t_kernel_impl* impl;
    explicit execution_scope(t_kernel_impl* i, int counter) : impl(i) {
        impl->current_execution.store(counter);
    }
    ~execution_scope() { impl->current_execution.store(0); }
};

} // namespace

max_interpreter::max_interpreter(t_kernel_impl* impl)
    : m_impl(impl)
{
    xeus::register_interpreter(this);
}

void max_interpreter::configure_impl() {
    // Nothing to configure
}

void max_interpreter::on_idle() {
    flush_async_output();
}

void max_interpreter::flush_async_output() {
    while (auto out = m_impl->async_queue.try_pop()) {
        const std::string name = out->stream_name.empty() ? std::string("stdout")
                                                          : out->stream_name;
        publish_stream(name, as_line(out->text));
    }
}

void max_interpreter::execute_request_impl(send_reply_callback cb,
                                           int execution_counter,
                                           const std::string& code,
                                           xeus::execute_request_config config,
                                           nl::json user_expressions) {
    // Discard replies left over from earlier cells. Without this a duplicated
    // or late result is delivered as the answer to this cell, and every
    // subsequent cell stays off by one.
    m_impl->result_queue.clear();

    // Publish anything Max queued while no cell was running.
    if (!config.silent) {
        flush_async_output();
    }

    // From here until we return, results arriving from Max belong to this cell.
    execution_scope scope(m_impl, execution_counter);

    // 1. Hand the code to the Max patch via the left outlet.
    OutletMessage msg;
    msg.selector = "code";
    msg.atoms.push_back(std::string("execute"));
    msg.atoms.push_back(code);
    msg.outlet_index = 0; // left outlet
    msg.execution_counter = execution_counter;

    m_impl->outlet_queue.push(std::move(msg));
    m_impl->notify_main_thread();

    // Note: xeus::xinterpreter::execute_request has already published the
    // execution_input for this cell. Publishing it again here would emit a
    // duplicate In[n] on IOPub.

    // 2. Fire-and-forget mode: do not wait for the patch to answer.
    const long timeout_s = m_impl->timeout.load();
    if (timeout_s <= 0) {
        nl::json reply;
        reply["status"] = "ok";
        reply["execution_count"] = execution_counter;
        reply["user_expressions"] = nl::json::object();
        reply["payload"] = nl::json::array();
        cb(reply);
        return;
    }

    // 3. Wait for a terminal result, publishing intermediate output as it lands.
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::seconds(timeout_s);
    bool got_result = false;

    while (!got_result) {
        if (!m_impl->alive.load() || m_impl->shutdown_requested.load()) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            break;
        }

        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        auto result = m_impl->result_queue.wait_pop(std::min(remaining, k_wait_slice));

        // Nothing yet -- drain async output so long-running cells still stream.
        if (!result.has_value()) {
            if (!config.silent) {
                flush_async_output();
            }
            continue;
        }

        const ResultMessage& r = result.value();

        // A reply stamped for a different cell is stale; drop it.
        if (r.execution_counter != execution_counter) {
            continue;
        }

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

        // Intermediate output: publish it and keep waiting for the result.
        if (r.is_stream()) {
            publish_stream(r.stream_name, as_line(r.text));
            continue;
        }

        nl::json data;
        if (!r.mime_type.empty()) {
            data[r.mime_type] = r.text;
        } else {
            data["text/plain"] = r.text;
        }
        publish_execution_result(execution_counter, std::move(data), nl::json::object());
        got_result = true;
    }

    if (got_result) {
        nl::json reply;
        reply["status"] = "ok";
        reply["execution_count"] = execution_counter;
        reply["user_expressions"] = nl::json::object();
        reply["payload"] = nl::json::array();
        cb(reply);
        return;
    }

    // 5. No result within the deadline. A timeout is not a success -- report it
    //    as an error so programmatic clients can tell the difference.
    const std::string ename = "MaxTimeout";
    const std::string evalue =
        "no result from Max within " + std::to_string(timeout_s) + "s: " + code;

    if (!config.silent) {
        publish_execution_error(ename, evalue, {});
    }

    nl::json reply;
    reply["status"] = "error";
    reply["ename"] = ename;
    reply["evalue"] = evalue;
    reply["traceback"] = nl::json::array();
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
    reply["implementation_version"] = MX_KERNEL_VERSION;

    nl::json language_info;
    language_info["name"] = "max";
    language_info["version"] = "8.0";
    language_info["mimetype"] = "text/x-maxmsp";
    language_info["file_extension"] = ".maxpat";

    reply["language_info"] = language_info;
    reply["banner"] = "Max/MSP Jupyter Kernel v" MX_KERNEL_VERSION;
    reply["help_links"] = nl::json::array();

    return reply;
}

void max_interpreter::shutdown_request_impl() {
    // Only flag the request. Clearing `alive` here would leave the object
    // permanently unable to execute, since nothing ever set it back.
    m_impl->shutdown_requested.store(true);

    // Let the patch know the client asked to shut down.
    OutletMessage msg;
    msg.selector = "shutdown";
    msg.outlet_index = 1; // right outlet (status)
    m_impl->outlet_queue.push(std::move(msg));
    m_impl->notify_main_thread();
}

} // namespace mx
