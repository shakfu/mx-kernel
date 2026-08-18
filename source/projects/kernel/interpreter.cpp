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

void max_interpreter::flush_async_output() {
    while (auto out = m_impl->async_queue.try_pop()) {
        const std::string name = out->stream_name.empty() ? std::string("stdout")
                                                          : out->stream_name;
        publish_stream(name, as_line(out->text));
    }
}

void max_interpreter::set_request_context(xeus::xrequest_context context) {
    m_dispatch_context = std::move(context);
}

const xeus::xrequest_context& max_interpreter::get_request_context() const noexcept {
    return m_has_active ? m_active_context : m_dispatch_context;
}

void max_interpreter::execute_request_impl(send_reply_callback cb,
                                           int execution_counter,
                                           const std::string& code,
                                           xeus::execute_request_config config,
                                           nl::json user_expressions) {
    // Queue the cell and return without replying. xeus registers
    // execute_request as non-blocking precisely so a kernel can do this; the
    // reply callback carries its own context, and publishes the trailing idle
    // status itself when we eventually call it.
    pending_execution p;
    p.cb = std::move(cb);
    p.context = m_dispatch_context;
    p.counter = execution_counter;
    p.code = code;
    p.silent = config.silent;
    p.timeout_s = m_impl->timeout.load();

    m_pending.push_back(std::move(p));
    m_impl->pending_executions.store(static_cast<int>(m_pending.size()));

    // Hand it to Max straight away rather than waiting for the next idle tick.
    pump();

    // Note: xeus::xinterpreter::execute_request has already published the
    // execution_input for this cell.
}

void max_interpreter::start_front() {
    pending_execution& p = m_pending.front();

    // Drop replies left over from an earlier cell before this one can see
    // them, then declare this cell the one results belong to.
    m_impl->result_queue.clear();
    m_impl->current_execution.store(p.counter);

    OutletMessage msg;
    msg.selector = "code";
    msg.atoms.push_back(std::string("execute"));
    msg.atoms.push_back(p.code);
    msg.outlet_index = 0; // left outlet
    msg.execution_counter = p.counter;

    m_impl->outlet_queue.push(std::move(msg));
    m_impl->notify_main_thread();

    p.deadline = std::chrono::steady_clock::now()
               + std::chrono::seconds(p.timeout_s > 0 ? p.timeout_s : 0);
    p.started = true;
}

void max_interpreter::complete_front(nl::json reply) {
    send_reply_callback cb = std::move(m_pending.front().cb);
    m_pending.pop_front();
    m_impl->current_execution.store(0);
    m_impl->pending_executions.store(static_cast<int>(m_pending.size()));
    cb(std::move(reply));
}

namespace {

nl::json ok_reply(int counter) {
    nl::json reply;
    reply["status"] = "ok";
    reply["execution_count"] = counter;
    reply["user_expressions"] = nl::json::object();
    reply["payload"] = nl::json::array();
    return reply;
}

nl::json error_reply(const std::string& ename, const std::string& evalue) {
    nl::json reply;
    reply["status"] = "error";
    reply["ename"] = ename;
    reply["evalue"] = evalue;
    reply["traceback"] = nl::json::array();
    return reply;
}

} // namespace

bool max_interpreter::service_front() {
    pending_execution& p = m_pending.front();

    if (!p.started) {
        start_front();
    }

    // Fire and forget: the cell is done as soon as Max has the code.
    if (p.timeout_s <= 0) {
        const int counter = p.counter;
        complete_front(ok_reply(counter));
        return true;
    }

    // Teardown or a client shutdown: answer rather than leave the client
    // waiting on a reply that will never come.
    if (!m_impl->alive.load() || m_impl->shutdown_requested.load()) {
        const std::string evalue = "kernel is shutting down";
        if (!p.silent) {
            publish_execution_error("MaxShutdown", evalue, {});
        }
        complete_front(error_reply("MaxShutdown", evalue));
        return true;
    }

    while (auto result = m_impl->result_queue.try_pop()) {
        const ResultMessage& r = result.value();

        // A reply stamped for a different cell is stale; drop it.
        if (r.execution_counter != p.counter) {
            continue;
        }

        if (r.is_error()) {
            publish_execution_error(r.error_name, r.error_value, {});
            nl::json reply = error_reply(r.error_name, r.error_value);
            complete_front(std::move(reply));
            return true;
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
        publish_execution_result(p.counter, std::move(data), nl::json::object());
        const int counter = p.counter;
        complete_front(ok_reply(counter));
        return true;
    }

    if (std::chrono::steady_clock::now() < p.deadline) {
        return false; // still waiting
    }

    // No result within the deadline. A timeout is not a success -- report it
    // as an error so programmatic clients can tell the difference.
    const std::string ename = "MaxTimeout";
    const std::string evalue =
        "no result from Max within " + std::to_string(p.timeout_s) + "s: " + p.code;

    if (!p.silent) {
        publish_execution_error(ename, evalue, {});
    }
    complete_front(error_reply(ename, evalue));
    return true;
}

void max_interpreter::pump() {
    while (!m_pending.empty()) {
        // Publish under the context of the cell being serviced, so its output
        // is attributed to it and not to whichever request arrived last.
        m_active_context = m_pending.front().context;
        m_has_active = true;
        const bool completed = service_front();
        m_has_active = false;

        if (!completed) {
            break;
        }
    }
}

void max_interpreter::on_idle() {
    if (!m_pending.empty()) {
        // Attribute free-standing output to the cell that is running.
        m_active_context = m_pending.front().context;
        m_has_active = true;
        flush_async_output();
        m_has_active = false;
    } else {
        flush_async_output();
    }

    pump();
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
    // Clients resolve a syntax highlighter from pygments_lexer, falling back to
    // `name`. There is no "max" lexer, so without this jupyter-console warns
    // "No lexer found for language 'max'" on every connect. Max messages have
    // no highlighter to offer, so name the plain-text one deliberately.
    language_info["pygments_lexer"] = "text";
    language_info["codemirror_mode"] = "null";

    reply["language_info"] = language_info;
    reply["banner"] = "Max/MSP Jupyter Kernel v" MX_KERNEL_VERSION;
    reply["help_links"] = nl::json::array();

    return reply;
}

void max_interpreter::shutdown_request_impl() {
    // Only flag the request. Clearing `alive` here would leave the object
    // permanently unable to execute, since nothing ever set it back.
    //
    // xkernel::stop() calls this as well, so a stop initiated from Max also
    // arrives here. The external sets the flag before calling stop(), so a
    // previous value of true means this is our own shutdown rather than a
    // client's -- and the patch does not need telling about a stop it asked
    // for itself.
    const bool already_stopping = m_impl->shutdown_requested.exchange(true);
    if (already_stopping) {
        return;
    }

    // Let the patch know a client asked to shut down.
    OutletMessage msg;
    msg.selector = "shutdown";
    msg.outlet_index = 1; // right outlet (status)
    m_impl->outlet_queue.push(std::move(msg));
    m_impl->notify_main_thread();
}

} // namespace mx
