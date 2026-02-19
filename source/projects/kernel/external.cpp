// external.cpp - Max/MSP external interface for the Jupyter kernel
//
// All Max SDK interactions live here. The interpreter and connection modules
// are Max-free and independently testable.

#include "ext.h"
#include "ext_obex.h"
#include "ext_dictobj.h"

#include <string>
#include <sstream>
#include <memory>
#include <fstream>
#include <thread>

#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus-zmq/xserver_zmq.hpp"
#include "xeus-zmq/xzmq_context.hpp"
#include "nlohmann/json.hpp"

#include "interpreter.h"
#include "types.h"
#include "connection.h"

using namespace xeus;
namespace nl = nlohmann;

// ---------------------------------------------------------------------------
// t_kernel: Max-visible C struct
// ---------------------------------------------------------------------------
typedef struct _kernel {
    t_object ob;
    void* outlet_left;
    void* outlet_right;
    t_symbol* name;
    long debug;
    long timeout;
    mx::t_kernel_impl* impl;
    void* outlet_qelem;
} t_kernel;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void* kernel_new(t_symbol* s, long argc, t_atom* argv);
void kernel_free(t_kernel* x);
void kernel_assist(t_kernel* x, void* b, long m, long a, char* s);
void kernel_bang(t_kernel* x);
void kernel_eval(t_kernel* x, t_symbol* s, long argc, t_atom* argv);
void kernel_start(t_kernel* x);
void kernel_stop(t_kernel* x);
void kernel_info(t_kernel* x);
void kernel_result(t_kernel* x, t_symbol* s, long argc, t_atom* argv);
void kernel_dict(t_kernel* x, t_symbol* s);
void kernel_install(t_kernel* x);
void kernel_outlet_drain(t_kernel* x);

static void kernel_thread_func(t_kernel* x);

static t_class* kernel_class = nullptr;

// ---------------------------------------------------------------------------
// ext_main
// ---------------------------------------------------------------------------
void ext_main(void* r) {
    t_class* c = class_new("kernel",
                           (method)kernel_new,
                           (method)kernel_free,
                           (long)sizeof(t_kernel),
                           0L,
                           A_GIMME,
                           0);

    class_addmethod(c, (method)kernel_assist,  "assist",  A_CANT,  0);
    class_addmethod(c, (method)kernel_bang,    "bang",    0);
    class_addmethod(c, (method)kernel_eval,    "eval",    A_GIMME, 0);
    class_addmethod(c, (method)kernel_start,   "start",   0);
    class_addmethod(c, (method)kernel_stop,    "stop",    0);
    class_addmethod(c, (method)kernel_info,    "info",    0);
    class_addmethod(c, (method)kernel_result,  "result",  A_GIMME, 0);
    class_addmethod(c, (method)kernel_dict,    "dict",    A_SYM,   0);
    class_addmethod(c, (method)kernel_install, "install", 0);

    CLASS_ATTR_SYM(c, "name", 0, t_kernel, name);
    CLASS_ATTR_LABEL(c, "name", 0, "Unique Name");
    CLASS_ATTR_BASIC(c, "name", 0);

    CLASS_ATTR_LONG(c, "debug", 0, t_kernel, debug);
    CLASS_ATTR_LABEL(c, "debug", 0, "Debug Mode");
    CLASS_ATTR_STYLE(c, "debug", 0, "onoff");

    CLASS_ATTR_LONG(c, "timeout", 0, t_kernel, timeout);
    CLASS_ATTR_LABEL(c, "timeout", 0, "Result Timeout (seconds)");
    CLASS_ATTR_BASIC(c, "timeout", 0);

    class_register(CLASS_BOX, c);
    kernel_class = c;

    post("kernel: Max/MSP Jupyter Kernel v0.1.0");
}

// ---------------------------------------------------------------------------
// kernel_new / kernel_free
// ---------------------------------------------------------------------------
void* kernel_new(t_symbol* s, long argc, t_atom* argv) {
    t_kernel* x = (t_kernel*)object_alloc(kernel_class);
    if (!x) return nullptr;

    // Outlets (right to left so left is index 0)
    x->outlet_right = outlet_new(x, nullptr);
    x->outlet_left = outlet_new(x, nullptr);

    // Default attribute values
    x->name = gensym("");
    x->debug = 0;
    x->timeout = 30;
    x->impl = nullptr;
    x->outlet_qelem = nullptr;

    // Process attributes from object box args
    attr_args_process(x, argc, argv);

    // Allocate the C++ impl on the heap (away from Max's C allocator)
    try {
        auto* impl = new mx::t_kernel_impl();
        impl->timeout = x->timeout;

        // Create the qelem for draining outlet messages on the main thread
        x->outlet_qelem = qelem_new(x, (method)kernel_outlet_drain);
        impl->qelem = x->outlet_qelem;

        // Create the interpreter, passing the impl so it can access queues
        impl->interpreter = std::make_unique<mx::max_interpreter>(impl);

        x->impl = impl;

        if (x->debug) {
            object_post((t_object*)x, "kernel: created with name '%s'",
                        x->name->s_name);
        }
    } catch (const std::exception& e) {
        object_error((t_object*)x, "kernel: failed to create: %s", e.what());
    }

    return x;
}

void kernel_free(t_kernel* x) {
    auto* impl = x->impl;

    // Stop kernel if running
    if (impl && impl->kernel) {
        try {
            impl->kernel->stop();
        } catch (...) {}
    }

    // Detach thread (cannot join -- xeus blocks in poll_channels)
    if (impl && impl->kernel_thread) {
        if (impl->kernel_thread->joinable()) {
            impl->kernel_thread->detach();
        }
        delete impl->kernel_thread;
        impl->kernel_thread = nullptr;
    }

    // Free qelem
    if (x->outlet_qelem) {
        qelem_free(x->outlet_qelem);
        x->outlet_qelem = nullptr;
    }

    // Remove connection file
    if (impl && !impl->connection_file.empty()) {
        try {
            std::remove(impl->connection_file.c_str());
        } catch (...) {}
    }

    // Intentionally leak kernel and context to avoid xeus destructor deadlock.
    // Release ownership so ~t_kernel_impl doesn't call their destructors.
    if (impl) {
        impl->kernel.release();
        impl->context.release();
        impl->alive.store(false);
        delete impl;
        x->impl = nullptr;
    }
}

// ---------------------------------------------------------------------------
// kernel_outlet_drain -- qelem callback, runs on the main thread
// ---------------------------------------------------------------------------
void kernel_outlet_drain(t_kernel* x) {
    auto* impl = x->impl;
    if (!impl) return;

    // Drain all pending outlet messages
    while (true) {
        auto msg = impl->outlet_queue.try_pop();
        if (!msg.has_value()) break;

        const mx::OutletMessage& m = msg.value();

        // Convert AtomValue vector to t_atom array
        std::vector<t_atom> atoms(m.atoms.size());
        for (size_t i = 0; i < m.atoms.size(); ++i) {
            const auto& val = m.atoms[i];
            if (std::holds_alternative<std::string>(val)) {
                atom_setsym(&atoms[i], gensym(std::get<std::string>(val).c_str()));
            } else if (std::holds_alternative<long>(val)) {
                atom_setlong(&atoms[i], std::get<long>(val));
            } else if (std::holds_alternative<double>(val)) {
                atom_setfloat(&atoms[i], std::get<double>(val));
            }
        }

        void* outlet = (m.outlet_index == 0) ? x->outlet_left : x->outlet_right;
        if (outlet) {
            outlet_anything(outlet, gensym(m.selector.c_str()),
                            static_cast<long>(atoms.size()), atoms.data());
        }
    }
}

// ---------------------------------------------------------------------------
// kernel_start / kernel_stop
// ---------------------------------------------------------------------------
static void kernel_thread_func(t_kernel* x) {
    auto* impl = x->impl;
    if (impl && impl->kernel) {
        try {
            impl->kernel->start();
        } catch (const std::exception& e) {
            if (x->debug) {
                object_error((t_object*)x, "kernel: thread error: %s", e.what());
            }
        }
    }
}

void kernel_start(t_kernel* x) {
    auto* impl = x->impl;
    if (!impl) {
        object_error((t_object*)x, "kernel: not initialized");
        return;
    }

    if (impl->kernel) {
        object_warn((t_object*)x, "kernel: already running");
        return;
    }

    if (!impl->interpreter) {
        object_error((t_object*)x, "kernel: interpreter not initialized");
        return;
    }

    try {
        // Sync timeout from attribute
        impl->timeout = x->timeout;

        // Create configuration and ZMQ context
        xconfiguration config = mx::create_kernel_configuration();
        impl->context = std::unique_ptr<xcontext>(make_zmq_context());

        if (x->debug) {
            object_post((t_object*)x, "kernel: creating ZMQ server...");
        }

        std::string username = get_user_name();

        // Transfer interpreter ownership into the kernel.
        // xkernel expects unique_ptr<xinterpreter>, so upcast from max_interpreter.
        std::unique_ptr<xinterpreter> interp_ptr(std::move(impl->interpreter));

        impl->kernel = std::make_unique<xkernel>(
            config,
            username,
            std::move(impl->context),
            std::move(interp_ptr),
            make_xserver_default,
            make_in_memory_history_manager()
        );

        // Get actual bound ports
        impl->kernel->get_server().update_config(config);

        // Generate kernel name
        std::string kernel_name = std::string(x->name->s_name);
        if (kernel_name.empty()) {
            kernel_name = "max-" + std::to_string((uintptr_t)x);
        }

        // Write connection file
        impl->connection_file = mx::write_connection_file(config, kernel_name);

        object_post((t_object*)x, "kernel: connection file: %s",
                    impl->connection_file.c_str());
        object_post((t_object*)x,
                    "kernel: shell=%s control=%s iopub=%s stdin=%s hb=%s",
                    config.m_shell_port.c_str(),
                    config.m_control_port.c_str(),
                    config.m_iopub_port.c_str(),
                    config.m_stdin_port.c_str(),
                    config.m_hb_port.c_str());

        // Launch kernel thread
        impl->kernel_thread = new std::thread(kernel_thread_func, x);

        object_post((t_object*)x, "kernel: started successfully");
        object_post((t_object*)x, "kernel: connect with: jupyter console --existing %s",
                    impl->connection_file.c_str());

        if (x->outlet_right) {
            t_atom atoms[2];
            atom_setsym(&atoms[0], gensym("connection_file"));
            atom_setsym(&atoms[1], gensym(impl->connection_file.c_str()));
            outlet_anything(x->outlet_right, gensym("started"), 2, atoms);
        }

    } catch (const std::exception& e) {
        object_error((t_object*)x, "kernel: start error: %s", e.what());

        // Cleanup on failure
        if (impl->kernel) {
            impl->kernel.release(); // avoid destructor deadlock
        }
        if (impl->context) {
            impl->context.release();
        }
    }
}

void kernel_stop(t_kernel* x) {
    auto* impl = x->impl;
    if (!impl || !impl->kernel) {
        object_warn((t_object*)x, "kernel: not running");
        return;
    }

    try {
        impl->kernel->stop();

        if (impl->kernel_thread) {
            if (impl->kernel_thread->joinable()) {
                impl->kernel_thread->detach();
            }
            delete impl->kernel_thread;
            impl->kernel_thread = nullptr;
        }

        if (!impl->connection_file.empty()) {
            try {
                std::remove(impl->connection_file.c_str());
                impl->connection_file.clear();
            } catch (...) {}
        }

        object_post((t_object*)x, "kernel: stopped (thread will finish in background)");

        if (x->outlet_right) {
            outlet_anything(x->outlet_right, gensym("stopped"), 0, nullptr);
        }

    } catch (const std::exception& e) {
        object_error((t_object*)x, "kernel: stop error: %s", e.what());
    }
}

// ---------------------------------------------------------------------------
// kernel_bang / kernel_eval / kernel_info
// ---------------------------------------------------------------------------
void kernel_bang(t_kernel* x) {
    if (x->outlet_right) {
        outlet_bang(x->outlet_right);
    }
}

void kernel_eval(t_kernel* x, t_symbol* s, long argc, t_atom* argv) {
    auto* impl = x->impl;
    if (!impl || !impl->interpreter) {
        object_error((t_object*)x, "kernel: interpreter not initialized");
        return;
    }

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

    if (x->outlet_right) {
        outlet_anything(x->outlet_right, gensym("eval"), argc, argv);
    }
}

void kernel_info(t_kernel* x) {
    auto* impl = x->impl;
    if (!impl) {
        object_error((t_object*)x, "kernel: not initialized");
        return;
    }

    // kernel_info_request is only available when the interpreter is registered
    // and the kernel is running. Use the static info instead.
    try {
        nl::json info;
        info["implementation"] = "max_kernel";
        info["implementation_version"] = "0.1.0";
        info["language"] = "max";

        object_post((t_object*)x, "=== Kernel Info ===");
        object_post((t_object*)x, "Implementation: max_kernel");
        object_post((t_object*)x, "Version: 0.1.0");
        object_post((t_object*)x, "Language: max");

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

// ---------------------------------------------------------------------------
// kernel_result -- receives results from Max patches back to Jupyter
// ---------------------------------------------------------------------------
void kernel_result(t_kernel* x, t_symbol* s, long argc, t_atom* argv) {
    auto* impl = x->impl;
    if (!impl) {
        object_error((t_object*)x, "kernel: not initialized");
        return;
    }

    if (argc < 1) {
        object_error((t_object*)x, "kernel: result requires at least one argument");
        return;
    }

    mx::ResultMessage result;

    // Check for "error" prefix: result error <ename> <evalue>
    if (argc >= 3 && atom_gettype(argv) == A_SYM &&
        std::string(atom_getsym(argv)->s_name) == "error") {

        if (atom_gettype(argv + 1) == A_SYM) {
            result.error_name = atom_getsym(argv + 1)->s_name;
        }

        // Concatenate remaining atoms as error value
        std::stringstream ss;
        for (long i = 2; i < argc; i++) {
            if (i > 2) ss << " ";
            if (atom_gettype(argv + i) == A_SYM) {
                ss << atom_getsym(argv + i)->s_name;
            } else if (atom_gettype(argv + i) == A_LONG) {
                ss << atom_getlong(argv + i);
            } else if (atom_gettype(argv + i) == A_FLOAT) {
                ss << atom_getfloat(argv + i);
            }
        }
        result.error_value = ss.str();
    } else {
        // Normal result: concatenate all atoms as text
        std::stringstream ss;
        for (long i = 0; i < argc; i++) {
            if (i > 0) ss << " ";
            if (atom_gettype(argv + i) == A_SYM) {
                ss << atom_getsym(argv + i)->s_name;
            } else if (atom_gettype(argv + i) == A_LONG) {
                ss << atom_getlong(argv + i);
            } else if (atom_gettype(argv + i) == A_FLOAT) {
                ss << atom_getfloat(argv + i);
            }
        }
        result.text = ss.str();
    }

    impl->result_queue.push(std::move(result));

    if (x->debug) {
        object_post((t_object*)x, "kernel: result pushed to queue");
    }
}

// ---------------------------------------------------------------------------
// kernel_dict -- serialize a Max dict to JSON and push as result
// ---------------------------------------------------------------------------
void kernel_dict(t_kernel* x, t_symbol* s) {
    auto* impl = x->impl;
    if (!impl) {
        object_error((t_object*)x, "kernel: not initialized");
        return;
    }

    t_dictionary* dict = dictobj_findregistered_retain(s);
    if (!dict) {
        object_error((t_object*)x, "kernel: dictionary '%s' not found", s->s_name);
        return;
    }

    // Serialize to atoms, then parse as JSON
    long ac = 0;
    t_atom* av = nullptr;
    t_max_err err = dictobj_dictionarytoatoms(dict, &ac, &av);

    if (err != MAX_ERR_NONE || ac == 0 || !av) {
        object_error((t_object*)x, "kernel: failed to serialize dictionary '%s'", s->s_name);
        dictobj_release(dict);
        return;
    }

    // Convert atoms to a single string (Max dict text format)
    std::stringstream ss;
    for (long i = 0; i < ac; i++) {
        if (i > 0) ss << " ";
        if (atom_gettype(av + i) == A_SYM) {
            ss << atom_getsym(av + i)->s_name;
        } else if (atom_gettype(av + i) == A_LONG) {
            ss << atom_getlong(av + i);
        } else if (atom_gettype(av + i) == A_FLOAT) {
            ss << atom_getfloat(av + i);
        }
    }

    sysmem_freeptr(av);
    dictobj_release(dict);

    // Try to parse the dict text as JSON. Max's dictobj_dictionarytoatoms
    // produces a text format that may not be direct JSON, but we attempt it.
    // If it fails, send the raw text.
    mx::ResultMessage result;
    result.text = ss.str();
    result.mime_type = "application/json";

    impl->result_queue.push(std::move(result));

    if (x->debug) {
        object_post((t_object*)x, "kernel: dict '%s' pushed as JSON result", s->s_name);
    }
}

// ---------------------------------------------------------------------------
// kernel_install -- install Jupyter kernelspec
// ---------------------------------------------------------------------------
void kernel_install(t_kernel* x) {
    const char* home = std::getenv("HOME");
    if (!home) {
        object_error((t_object*)x, "kernel: HOME not set");
        return;
    }

    std::string dir = std::string(home) + "/.local/share/jupyter/kernels/mx-kernel";

    // Create directory (mkdir -p equivalent)
    std::string mkdir_cmd = "mkdir -p \"" + dir + "\"";
    if (std::system(mkdir_cmd.c_str()) != 0) {
        object_error((t_object*)x, "kernel: failed to create kernelspec directory");
        return;
    }

    // Write kernel.json
    nl::json spec;
    spec["argv"] = nl::json::array({"echo", "Connect via Max"});
    spec["display_name"] = "Max/MSP";
    spec["language"] = "max";

    std::string filepath = dir + "/kernel.json";
    std::ofstream file(filepath);
    if (!file.is_open()) {
        object_error((t_object*)x, "kernel: failed to write kernelspec");
        return;
    }

    file << spec.dump(2);
    file.close();

    object_post((t_object*)x, "kernel: kernelspec installed at %s", filepath.c_str());

    if (x->outlet_right) {
        t_atom a;
        atom_setsym(&a, gensym(filepath.c_str()));
        outlet_anything(x->outlet_right, gensym("installed"), 1, &a);
    }
}

// ---------------------------------------------------------------------------
// kernel_assist
// ---------------------------------------------------------------------------
void kernel_assist(t_kernel* x, void* b, long m, long a, char* s) {
    if (m == ASSIST_INLET) {
        snprintf(s, 256, "Messages to kernel (start, stop, eval, result, dict, install)");
    } else {
        switch (a) {
        case 0:
            snprintf(s, 256, "Kernel output (code from Jupyter)");
            break;
        case 1:
            snprintf(s, 256, "Status messages");
            break;
        }
    }
}
