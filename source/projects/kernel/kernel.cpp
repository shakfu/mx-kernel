// kernel.cpp - Max external embedding a Jupyter kernel using xeus

#include "ext.h"
#include "ext_obex.h"

#include <string>
#include <sstream>
#include <memory>
#include <fstream>
#include <random>
#include <thread>

#include "xeus/xinterpreter.hpp"
#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus-zmq/xserver_zmq.hpp"
#include "xeus-zmq/xzmq_context.hpp"
#include "nlohmann/json.hpp"

using namespace xeus;
namespace nl = nlohmann;

// Forward declarations
typedef struct _kernel t_kernel;
class max_interpreter;

// Max external structure
typedef struct _kernel {
    t_object ob;                          // object header (must be first)
    void* outlet_left;                    // left outlet for output
    void* outlet_right;                   // right outlet for status
    t_symbol* name;                       // unique object name
    long debug;                           // debug flag
    std::unique_ptr<max_interpreter>* interpreter;  // pointer to interpreter
    std::unique_ptr<xkernel>* kernel;     // pointer to kernel
    std::unique_ptr<xcontext>* context;   // ZMQ context
    std::string connection_file;          // path to connection file
    std::thread* kernel_thread;           // kernel execution thread
} t_kernel;

// Custom interpreter for Max integration
class max_interpreter : public xinterpreter {
public:
    max_interpreter(t_kernel* x) : m_max_obj(x), m_execution_count(0) {
        register_interpreter(this);
    }

    virtual ~max_interpreter() = default;

private:
    void configure_impl() override {
        // Configuration logic
        if (m_max_obj && m_max_obj->debug) {
            object_post((t_object*)m_max_obj, "kernel: interpreter configured");
        }
    }

    void execute_request_impl(send_reply_callback cb,
                              int execution_counter,
                              const std::string& code,
                              execute_request_config config,
                              nl::json user_expressions) override {

        m_execution_count = execution_counter;

        if (m_max_obj && m_max_obj->debug) {
            object_post((t_object*)m_max_obj, "kernel: executing code: %s", code.c_str());
        }

        // Send to Max outlet
        if (m_max_obj && m_max_obj->outlet_left) {
            t_atom atoms[2];
            atom_setsym(&atoms[0], gensym("execute"));
            atom_setsym(&atoms[1], gensym(code.c_str()));
            outlet_anything(m_max_obj->outlet_left, gensym("code"), 2, atoms);
        }

        // Publish execution input (this is what shows In[n]: in Jupyter)
        if (!config.silent) {
            publish_execution_input(code, execution_counter);
        }

        // Publish execution result as stream output
        if (!config.silent) {
            std::string output = "Executed in Max: " + code;
            publish_stream("stdout", output);
        }

        // Create successful reply
        nl::json reply;
        reply["status"] = "ok";
        reply["execution_count"] = execution_counter;
        reply["user_expressions"] = nl::json::object();
        reply["payload"] = nl::json::array();

        cb(reply);
    }

    nl::json complete_request_impl(const std::string& code,
                                   int cursor_pos) override {
        nl::json reply;
        reply["matches"] = nl::json::array();
        reply["cursor_start"] = cursor_pos;
        reply["cursor_end"] = cursor_pos;
        reply["status"] = "ok";
        return reply;
    }

    nl::json inspect_request_impl(const std::string& code,
                                  int cursor_pos,
                                  int detail_level) override {
        nl::json reply;
        reply["status"] = "ok";
        reply["found"] = false;
        reply["data"] = nl::json::object();
        reply["metadata"] = nl::json::object();
        return reply;
    }

    nl::json is_complete_request_impl(const std::string& code) override {
        nl::json reply;
        reply["status"] = "complete";
        return reply;
    }

    nl::json kernel_info_request_impl() override {
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

    void shutdown_request_impl() override {
        if (m_max_obj && m_max_obj->debug) {
            object_post((t_object*)m_max_obj, "kernel: shutdown requested");
        }
    }

    t_kernel* m_max_obj;
    int m_execution_count;
};

// Helper functions
std::string generate_random_key();
int find_free_port();
xconfiguration create_kernel_configuration();
std::string write_connection_file(const xconfiguration& config, const std::string& kernel_name);
void kernel_thread_func(t_kernel* x);

// Method prototypes
void* kernel_new(t_symbol* s, long argc, t_atom* argv);
void kernel_free(t_kernel* x);
void kernel_assist(t_kernel* x, void* b, long m, long a, char* s);
void kernel_bang(t_kernel* x);
void kernel_eval(t_kernel* x, t_symbol* s, long argc, t_atom* argv);
void kernel_start(t_kernel* x);
void kernel_stop(t_kernel* x);
void kernel_info(t_kernel* x);

// Global class pointer
static t_class* kernel_class = NULL;

// Helper function implementations
std::string generate_random_key() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    const char* hex_chars = "0123456789abcdef";
    std::string key;
    key.reserve(64);

    for (int i = 0; i < 64; ++i) {
        key += hex_chars[dis(gen)];
    }

    return key;
}

int find_free_port() {
    // Simple approach: let the OS assign a random port
    // For a more sophisticated approach, you could check specific port ranges
    return 0;  // 0 means let ZMQ auto-assign
}

xconfiguration create_kernel_configuration() {
    xconfiguration config;
    config.m_transport = "tcp";
    config.m_ip = "127.0.0.1";
    config.m_control_port = "0";  // Auto-assign
    config.m_shell_port = "0";
    config.m_stdin_port = "0";
    config.m_iopub_port = "0";
    config.m_hb_port = "0";
    config.m_signature_scheme = "hmac-sha256";
    config.m_key = generate_random_key();

    return config;
}

std::string write_connection_file(const xconfiguration& config, const std::string& kernel_name) {
    // Get runtime directory or temp directory
    const char* runtime_dir = std::getenv("JUPYTER_RUNTIME_DIR");
    std::string base_dir;

    if (runtime_dir) {
        base_dir = runtime_dir;
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            base_dir = std::string(home) + "/.local/share/jupyter/runtime";
        } else {
            base_dir = "/tmp";
        }
    }

    // Create filename
    std::string filename = base_dir + "/kernel-" + kernel_name + ".json";

    // Create JSON connection info
    nl::json connection_info;
    connection_info["transport"] = config.m_transport;
    connection_info["ip"] = config.m_ip;

    // Convert port strings to integers
    try {
        connection_info["control_port"] = std::stoi(config.m_control_port);
        connection_info["shell_port"] = std::stoi(config.m_shell_port);
        connection_info["stdin_port"] = std::stoi(config.m_stdin_port);
        connection_info["iopub_port"] = std::stoi(config.m_iopub_port);
        connection_info["hb_port"] = std::stoi(config.m_hb_port);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to parse ports: ") + e.what() +
                                 " (shell=" + config.m_shell_port +
                                 ", control=" + config.m_control_port + ")");
    }

    connection_info["signature_scheme"] = config.m_signature_scheme;
    connection_info["key"] = config.m_key;

    // Write to file
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open connection file for writing: " + filename);
    }

    file << connection_info.dump(2);
    file.close();

    if (file.fail()) {
        throw std::runtime_error("Failed to write connection file: " + filename);
    }

    return filename;
}

void kernel_thread_func(t_kernel* x) {
    if (x->kernel && (*x->kernel)) {
        try {
            (*x->kernel)->start();
        } catch (const std::exception& e) {
            if (x->debug) {
                object_error((t_object*)x, "kernel: thread error: %s", e.what());
            }
        }
    }
}

// Main entry point
void ext_main(void* r)
{
    t_class* c;

    c = class_new("kernel",
                  (method)kernel_new,
                  (method)kernel_free,
                  (long)sizeof(t_kernel),
                  0L,
                  A_GIMME,
                  0);

    class_addmethod(c, (method)kernel_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)kernel_bang,   "bang",   0);
    class_addmethod(c, (method)kernel_eval,   "eval",   A_GIMME, 0);
    class_addmethod(c, (method)kernel_start,  "start",  0);
    class_addmethod(c, (method)kernel_stop,   "stop",   0);
    class_addmethod(c, (method)kernel_info,   "info",   0);

    CLASS_ATTR_SYM(c, "name", 0, t_kernel, name);
    CLASS_ATTR_LABEL(c, "name", 0, "Unique Name");
    CLASS_ATTR_BASIC(c, "name", 0);

    CLASS_ATTR_LONG(c, "debug", 0, t_kernel, debug);
    CLASS_ATTR_LABEL(c, "debug", 0, "Debug Mode");
    CLASS_ATTR_STYLE(c, "debug", 0, "onoff");

    class_register(CLASS_BOX, c);
    kernel_class = c;

    post("kernel: Max/MSP Jupyter Kernel v0.1.0");
}

void kernel_assist(t_kernel* x, void* b, long m, long a, char* s)
{
    if (m == ASSIST_INLET) {
        snprintf(s, 256, "Messages to kernel");
    }
    else {
        switch (a) {
        case 0:
            snprintf(s, 256, "Kernel output");
            break;
        case 1:
            snprintf(s, 256, "Status messages");
            break;
        }
    }
}

void kernel_free(t_kernel* x)
{
    // Stop kernel if running
    if (x->kernel) {
        try {
            (*x->kernel)->stop();
        } catch (...) {
            // Ignore exceptions during cleanup
        }
    }

    // Handle thread cleanup
    if (x->kernel_thread) {
        if (x->kernel_thread->joinable()) {
            // Thread is still running - detach it rather than blocking
            x->kernel_thread->detach();
        }
        delete x->kernel_thread;
        x->kernel_thread = nullptr;
    }

    // IMPORTANT: If the kernel was stopped but the thread was detached,
    // we CANNOT safely delete the kernel/context because xeus has its own
    // internal threads that will block in their destructors trying to join.
    // Since those internal threads are blocked waiting for ZMQ messages,
    // they'll never exit cleanly.
    //
    // Solution: Intentionally leak kernel and context on shutdown.
    // The OS will clean up all resources (memory, sockets, threads) when
    // the process terminates anyway.

    // Only clean up if we're certain the kernel never started
    // (interpreter is safe to delete - it doesn't have threads)
    if (x->interpreter) {
        delete x->interpreter;
        x->interpreter = nullptr;
    }

    // Remove connection file if it exists
    if (!x->connection_file.empty()) {
        try {
            std::remove(x->connection_file.c_str());
        } catch (...) {
            // Ignore errors
        }
    }

    // Note: x->kernel and x->context are intentionally not deleted here
    // to avoid blocking in xeus internal thread joins during shutdown.
    // This is a known limitation when the kernel is stopped while blocked
    // waiting for messages. The OS will reclaim all resources on process exit.
}

void* kernel_new(t_symbol* s, long argc, t_atom* argv)
{
    t_kernel* x = NULL;

    if ((x = (t_kernel*)object_alloc(kernel_class))) {
        // Create outlets (right to left)
        x->outlet_right = outlet_new(x, NULL);
        x->outlet_left = outlet_new(x, NULL);

        // Initialize attributes
        x->name = gensym("");
        x->debug = 0;
        x->interpreter = nullptr;
        x->kernel = nullptr;
        x->context = nullptr;
        x->kernel_thread = nullptr;

        // Process attributes
        attr_args_process(x, argc, argv);

        // Create interpreter
        try {
            x->interpreter = new std::unique_ptr<max_interpreter>(
                new max_interpreter(x)
            );

            if (x->debug) {
                object_post((t_object*)x, "kernel: created with name '%s'",
                           x->name->s_name);
            }
        } catch (const std::exception& e) {
            object_error((t_object*)x, "kernel: failed to create interpreter: %s",
                        e.what());
        }
    }
    return (x);
}

void kernel_bang(t_kernel* x)
{
    if (x->outlet_right) {
        outlet_bang(x->outlet_right);
    }
}

void kernel_eval(t_kernel* x, t_symbol* s, long argc, t_atom* argv)
{
    if (!x->interpreter || !(*x->interpreter)) {
        object_error((t_object*)x, "kernel: interpreter not initialized");
        return;
    }

    // Concatenate atoms into a string
    std::stringstream ss;
    for (long i = 0; i < argc; i++) {
        if (atom_gettype(argv + i) == A_SYM) {
            ss << atom_getsym(argv + i)->s_name;
        } else if (atom_gettype(argv + i) == A_LONG) {
            ss << atom_getlong(argv + i);
        } else if (atom_gettype(argv + i) == A_FLOAT) {
            ss << atom_getfloat(argv + i);
        }
        if (i < argc - 1) ss << " ";
    }

    std::string code = ss.str();

    if (x->debug) {
        object_post((t_object*)x, "kernel: eval '%s'", code.c_str());
    }

    // Execute the code through the interpreter
    try {
        auto callback = [x](nl::json reply) {
            if (x->debug) {
                object_post((t_object*)x, "kernel: execution complete");
            }
        };

        execute_request_config config;
        config.silent = false;
        config.store_history = true;
        config.allow_stdin = false;

        // Note: This is a simplified version. Full implementation would need
        // proper request context handling
        if (x->outlet_right) {
            outlet_anything(x->outlet_right, gensym("eval"), argc, argv);
        }

    } catch (const std::exception& e) {
        object_error((t_object*)x, "kernel: eval error: %s", e.what());
    }
}

void kernel_start(t_kernel* x)
{
    if (x->kernel && (*x->kernel)) {
        object_warn((t_object*)x, "kernel: already running");
        return;
    }

    if (!x->interpreter || !(*x->interpreter)) {
        object_error((t_object*)x, "kernel: interpreter not initialized");
        return;
    }

    try {
        // Create kernel configuration
        xconfiguration config = create_kernel_configuration();

        // Create ZMQ context
        x->context = new std::unique_ptr<xcontext>(make_zmq_context());

        if (x->debug) {
            object_post((t_object*)x, "kernel: creating ZMQ server...");
        }

        // Create the kernel with ZMQ server
        std::string username = get_user_name();

        // Transfer ownership of interpreter (we need to move it)
        auto interpreter_ptr = std::move(*x->interpreter);

        // Create the kernel
        x->kernel = new std::unique_ptr<xkernel>(
            new xkernel(
                config,
                username,
                std::move(*x->context),
                std::move(interpreter_ptr),
                make_xserver_default,
                make_in_memory_history_manager()
            )
        );

        // Update config with actual ports after server binding
        (*x->kernel)->get_server().update_config(config);

        // Generate kernel name from object name or create unique one
        std::string kernel_name = std::string(x->name->s_name);
        if (kernel_name.empty()) {
            kernel_name = "max-" + std::to_string((uintptr_t)x);
        }

        // Write connection file
        x->connection_file = write_connection_file(config, kernel_name);

        object_post((t_object*)x, "kernel: connection file: %s", x->connection_file.c_str());
        object_post((t_object*)x, "kernel: shell_port=%s control_port=%s iopub_port=%s stdin_port=%s hb_port=%s",
                   config.m_shell_port.c_str(),
                   config.m_control_port.c_str(),
                   config.m_iopub_port.c_str(),
                   config.m_stdin_port.c_str(),
                   config.m_hb_port.c_str());

        // Start kernel in separate thread
        x->kernel_thread = new std::thread(kernel_thread_func, x);

        object_post((t_object*)x, "kernel: started successfully");
        object_post((t_object*)x, "kernel: connect with: jupyter console --existing %s",
                   x->connection_file.c_str());

        if (x->outlet_right) {
            t_atom atoms[2];
            atom_setsym(&atoms[0], gensym("connection_file"));
            atom_setsym(&atoms[1], gensym(x->connection_file.c_str()));
            outlet_anything(x->outlet_right, gensym("started"), 2, atoms);
        }

    } catch (const std::exception& e) {
        object_error((t_object*)x, "kernel: start error: %s", e.what());

        // Cleanup on error
        if (x->kernel) {
            delete x->kernel;
            x->kernel = nullptr;
        }
        if (x->context) {
            delete x->context;
            x->context = nullptr;
        }
    }
}

void kernel_stop(t_kernel* x)
{
    if (x->kernel && (*x->kernel)) {
        try {
            // Stop the kernel (signals the event loop to exit)
            (*x->kernel)->stop();

            // The issue: kernel->stop() sets a flag, but the thread is blocked in poll_channels()
            // waiting for messages. It won't check the flag until a message arrives.
            //
            // Solution: Detach the thread immediately and leave kernel/context cleanup to kernel_free().
            // This way, the user gets immediate response, and the thread will eventually exit on its own
            // (when it receives the next message or when the process exits).

            if (x->kernel_thread) {
                if (x->kernel_thread->joinable()) {
                    x->kernel_thread->detach();
                }
                delete x->kernel_thread;
                x->kernel_thread = nullptr;
            }

            // Remove connection file (safe to do immediately)
            if (!x->connection_file.empty()) {
                try {
                    std::remove(x->connection_file.c_str());
                    x->connection_file.clear();
                } catch (...) {
                    // Ignore errors
                }
            }

            object_post((t_object*)x, "kernel: stopped (thread will finish in background)");

            if (x->outlet_right) {
                outlet_anything(x->outlet_right, gensym("stopped"), 0, NULL);
            }

            // Note: We leave kernel and context alive for now. They'll be cleaned up in kernel_free()
            // after a grace period. This prevents use-after-free if the detached thread is still running.

        } catch (const std::exception& e) {
            object_error((t_object*)x, "kernel: stop error: %s", e.what());
        }
    } else {
        object_warn((t_object*)x, "kernel: not running");
    }
}

void kernel_info(t_kernel* x)
{
    if (!x->interpreter || !(*x->interpreter)) {
        object_error((t_object*)x, "kernel: interpreter not initialized");
        return;
    }

    try {
        nl::json info = (*x->interpreter)->kernel_info_request();

        object_post((t_object*)x, "=== Kernel Info ===");
        object_post((t_object*)x, "Implementation: %s",
                   info["implementation"].get<std::string>().c_str());
        object_post((t_object*)x, "Version: %s",
                   info["implementation_version"].get<std::string>().c_str());
        object_post((t_object*)x, "Language: %s",
                   info["language_info"]["name"].get<std::string>().c_str());

        if (x->outlet_right) {
            t_atom atoms[2];
            atom_setsym(&atoms[0], gensym("info"));
            atom_setsym(&atoms[1], gensym(info.dump().c_str()));
            outlet_anything(x->outlet_right, gensym("kernel"), 2, atoms);
        }

    } catch (const std::exception& e) {
        object_error((t_object*)x, "kernel: info error: %s", e.what());
    }
}
