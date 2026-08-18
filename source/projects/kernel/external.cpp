// external.cpp - Max/MSP external interface for the Jupyter kernel
//
// All Max SDK interactions live here. The interpreter and connection modules
// are Max-free and independently testable.

#include "ext.h"
#include "ext_obex.h"
#include "ext_dictobj.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus-zmq/xserver_zmq.hpp"
#include "xeus-zmq/xzmq_context.hpp"
#include "nlohmann/json.hpp"

#include "connection.h"
#include "interpreter.h"
#include "types.h"
#include "version.h"

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
void kernel_print(t_kernel* x, t_symbol* s, long argc, t_atom* argv);
void kernel_dict(t_kernel* x, t_symbol* s);
void kernel_install(t_kernel* x);
void kernel_outlet_drain(t_kernel* x);

static void kernel_thread_func(t_kernel* x);
static bool kernel_shutdown(t_kernel* x, std::chrono::milliseconds limit);

static t_class* kernel_class = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Concatenate a Max argument list into a single space-separated string.
// Atoms that are neither symbol, int nor float are reported and skipped.
static std::string atoms_to_string(t_object* owner, long argc, t_atom* argv,
                                   long start = 0) {
    std::stringstream ss;
    bool first = true;

    for (long i = start; i < argc; i++) {
        const short type = atom_gettype(argv + i);
        if (type != A_SYM && type != A_LONG && type != A_FLOAT) {
            object_warn(owner, "ignoring unsupported atom at index %ld", i);
            continue;
        }

        if (!first) ss << " ";
        first = false;

        if (type == A_SYM) {
            ss << atom_getsym(argv + i)->s_name;
        } else if (type == A_LONG) {
            ss << atom_getlong(argv + i);
        } else {
            ss << atom_getfloat(argv + i);
        }
    }

    return ss.str();
}

// Queue a message for a cell, or as free-standing output when no cell is
// waiting. Results that arrive outside an execution used to be delivered as
// the answer to whichever cell ran next; routing them to the async queue keeps
// the text without letting it masquerade as a result.
static void queue_for_jupyter(t_kernel* x, mx::ResultMessage result) {
    auto* impl = x->impl;
    const int pending = impl->current_execution.load();

    if (pending != 0) {
        result.execution_counter = pending;
        impl->result_queue.push(std::move(result));
        return;
    }

    if (x->debug) {
        object_warn((t_object*)x,
                    "no cell is waiting; queuing as output for the next cell");
    }

    if (result.is_error()) {
        result.stream_name = "stderr";
        result.text = result.error_name + ": " + result.error_value;
        result.error_name.clear();
        result.error_value.clear();
    } else if (result.stream_name.empty()) {
        result.stream_name = "stdout";
    }

    impl->async_queue.push(std::move(result));
}

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
    class_addmethod(c, (method)kernel_print,   "print",   A_GIMME, 0);
    class_addmethod(c, (method)kernel_dict,    "dict",    A_SYM,   0);
    class_addmethod(c, (method)kernel_install, "install", 0);

    CLASS_ATTR_SYM(c, "name", 0, t_kernel, name);
    CLASS_ATTR_LABEL(c, "name", 0, "Unique Name");
    CLASS_ATTR_BASIC(c, "name", 0);

    CLASS_ATTR_LONG(c, "debug", 0, t_kernel, debug);
    CLASS_ATTR_LABEL(c, "debug", 0, "Debug Mode");
    CLASS_ATTR_STYLE(c, "debug", 0, "onoff");

    CLASS_ATTR_LONG(c, "timeout", 0, t_kernel, timeout);
    CLASS_ATTR_LABEL(c, "timeout", 0, "Result Timeout (seconds, 0 = do not wait)");
    CLASS_ATTR_BASIC(c, "timeout", 0);

    class_register(CLASS_BOX, c);
    kernel_class = c;

    post("kernel: Max/MSP Jupyter Kernel v" MX_KERNEL_VERSION);
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

    // Allocate the C++ impl on the heap (away from Max's C allocator).
    // The interpreter is created in kernel_start, not here: starting the kernel
    // moves it into the xkernel, so a single instance cannot survive a restart.
    try {
        auto impl = std::make_unique<mx::t_kernel_impl>();
        impl->timeout.store(x->timeout);

        x->outlet_qelem = qelem_new(x, (method)kernel_outlet_drain);
        if (!x->outlet_qelem) {
            object_error((t_object*)x, "failed to allocate qelem");
            return x;
        }

        void* qelem = x->outlet_qelem;
        impl->set_notifier([qelem]() { qelem_set((t_qelem*)qelem); });

        x->impl = impl.release();

        if (x->debug) {
            object_post((t_object*)x, "created with name '%s'",
                        x->name->s_name);
        }
    } catch (const std::exception& e) {
        object_error((t_object*)x, "failed to create: %s", e.what());
    }

    return x;
}

void kernel_free(t_kernel* x) {
    auto* impl = x->impl;
    if (!impl) {
        if (x->outlet_qelem) {
            qelem_free(x->outlet_qelem);
            x->outlet_qelem = nullptr;
        }
        return;
    }

    // Tell the kernel thread to abandon any wait in progress.
    impl->alive.store(false);

    // Stop the server and join its thread. This completes because the server
    // loop polls with a timeout; see source/notes/shutdown_compromise.md for
    // what used to happen instead.
    kernel_shutdown(x, std::chrono::milliseconds(2000));

    // Drop the notifier before freeing the qelem. After this returns, no
    // thread can reach qelem_set, so freeing the qelem is safe -- this matters
    // in the fallback case where a thread is still running.
    impl->clear_notifier();

    if (x->outlet_qelem) {
        qelem_free(x->outlet_qelem);
        x->outlet_qelem = nullptr;
    }

    if (!impl->connection_file.empty()) {
        std::remove(impl->connection_file.c_str());
    }

    if (impl->leaked) {
        // A kernel thread outlived its deadline and still holds pointers into
        // impl, so nothing here may be freed. Only reachable if the timed-poll
        // patch is missing or the loop is wedged.
        x->impl = nullptr;
        return;
    }

    delete impl;
    x->impl = nullptr;
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
            // Always report: a failure here is a silent crash otherwise.
            object_error((t_object*)x, "thread error: %s", e.what());
        }
    }
    // Publish last: the main thread waits on this to know a join will not block.
    if (impl) {
        impl->thread_finished.store(true);
    }
}

// Stop the server loop and take down the kernel.
//
// The timed-poll patch means the loop notices the stop request within its poll
// interval and returns, so the thread can be joined and the kernel destroyed
// normally. If that does not happen within `limit` -- which would mean the
// patch is missing or the loop is wedged -- fall back to the historical
// behaviour of detaching and leaking, rather than freezing Max's main thread.
//
// Returns true if the kernel was shut down cleanly.
static bool kernel_shutdown(t_kernel* x, std::chrono::milliseconds limit) {
    auto* impl = x->impl;
    if (!impl || !impl->kernel) {
        return true;
    }

    // Release any cell that is mid-wait, so the server thread can return to
    // its loop instead of sitting out the full result timeout.
    impl->shutdown_requested.store(true);

    // Give the server loop a moment to answer cells that are still in flight.
    // Stopping first would leave those clients waiting on a reply that can
    // never arrive.
    if (impl->pending_executions.load() > 0) {
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::milliseconds(500);
        while (impl->pending_executions.load() > 0
               && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    try {
        impl->kernel->stop();
    } catch (...) {}

    bool finished = impl->thread_finished.load();
    if (!finished && impl->kernel_thread) {
        const auto deadline = std::chrono::steady_clock::now() + limit;
        while (std::chrono::steady_clock::now() < deadline) {
            if (impl->thread_finished.load()) {
                finished = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    if (impl->kernel_thread) {
        if (finished) {
            if (impl->kernel_thread->joinable()) {
                impl->kernel_thread->join();
            }
        } else if (impl->kernel_thread->joinable()) {
            impl->kernel_thread->detach();
        }
        delete impl->kernel_thread;
        impl->kernel_thread = nullptr;
    }

    if (!finished) {
        // The thread is still running and still holds pointers into impl, so
        // nothing it can reach may be destroyed.
        object_warn((t_object*)x,
                    "server thread did not stop in time; leaking it "
                    "(is the xeus-zmq timed-poll patch applied?)");
        impl->leaked = true;
        impl->interpreter_view = nullptr;
        impl->kernel.release();
        impl->context.release();
        return false;
    }

    // Safe now: the only thread that touched these has been joined.
    impl->interpreter_view = nullptr;
    impl->kernel.reset();
    impl->context.reset();
    impl->thread_finished.store(false);
    return true;
}

void kernel_start(t_kernel* x) {
    auto* impl = x->impl;
    if (!impl) {
        object_error((t_object*)x, "not initialized");
        return;
    }

    if (impl->kernel) {
        object_warn((t_object*)x, "already running");
        return;
    }

    try {
        // Re-arm state so a restart behaves like a fresh start.
        impl->timeout.store(x->timeout);
        impl->shutdown_requested.store(false);
        impl->alive.store(true);
        impl->thread_finished.store(false);
        impl->current_execution.store(0);
        impl->result_queue.clear();
        impl->async_queue.clear();

        // A fresh interpreter per start: the previous one was moved into the
        // previous xkernel and is no longer ours to use.
        impl->interpreter = std::make_unique<mx::max_interpreter>(impl);

        // Create configuration and ZMQ context
        xconfiguration config = mx::create_kernel_configuration();
        impl->context = std::unique_ptr<xcontext>(make_zmq_context());

        if (x->debug) {
            object_post((t_object*)x, "creating ZMQ server...");
        }

        std::string username = get_user_name();

        // Transfer interpreter ownership into the kernel, keeping a non-owning
        // view so the idle callback can reach it.
        // xkernel expects unique_ptr<xinterpreter>, so upcast from max_interpreter.
        impl->interpreter_view = impl->interpreter.get();
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

        // Drive the server loop's idle tick. The poll timeout bounds how long
        // `stop` takes to be noticed; the callback is what lets Max-initiated
        // output reach a client while no cell is running, since IOPub may only
        // be published from the thread that owns the socket.
        auto* server = dynamic_cast<xserver_zmq*>(&impl->kernel->get_server());
        if (server) {
            server->set_poll_timeout(50);
            server->set_idle_callback([impl]() {
                if (impl->interpreter_view) {
                    impl->interpreter_view->on_idle();
                }
            });
        } else {
            object_warn((t_object*)x,
                        "unexpected server type; idle output disabled");
        }

        // Generate kernel name
        std::string kernel_name = mx::sanitize_kernel_name(x->name->s_name);
        if (kernel_name.empty()) {
            kernel_name = "max-" + std::to_string((uintptr_t)x);
        }

        // Write connection file
        impl->connection_file = mx::write_connection_file(config, kernel_name);

        object_post((t_object*)x, "connection file: %s",
                    impl->connection_file.c_str());
        object_post((t_object*)x,
                    "shell=%s control=%s iopub=%s stdin=%s hb=%s",
                    config.m_shell_port.c_str(),
                    config.m_control_port.c_str(),
                    config.m_iopub_port.c_str(),
                    config.m_stdin_port.c_str(),
                    config.m_hb_port.c_str());

        // Launch kernel thread
        impl->kernel_thread = new std::thread(kernel_thread_func, x);

        object_post((t_object*)x, "started successfully");
        object_post((t_object*)x, "connect with: jupyter console --existing %s",
                    impl->connection_file.c_str());

        if (x->outlet_right) {
            t_atom atoms[2];
            atom_setsym(&atoms[0], gensym("connection_file"));
            atom_setsym(&atoms[1], gensym(impl->connection_file.c_str()));
            outlet_anything(x->outlet_right, gensym("started"), 2, atoms);
        }

    } catch (const std::exception& e) {
        object_error((t_object*)x, "start error: %s", e.what());

        // Cleanup on failure. The kernel thread has not been launched yet at any
        // point where this can throw, so the kernel can be destroyed normally --
        // its publisher and heartbeat threads only exist once start() runs.
        impl->interpreter_view = nullptr;
        impl->kernel.reset();
        impl->context.reset();
        impl->interpreter.reset();
    }
}

void kernel_stop(t_kernel* x) {
    auto* impl = x->impl;
    if (!impl || !impl->kernel) {
        object_warn((t_object*)x, "not running");
        return;
    }

    try {
        const bool clean = kernel_shutdown(x, std::chrono::milliseconds(2000));

        if (!impl->connection_file.empty()) {
            std::remove(impl->connection_file.c_str());
            impl->connection_file.clear();
        }

        impl->current_execution.store(0);

        if (clean) {
            object_post((t_object*)x, "stopped");
        } else {
            object_post((t_object*)x,
                        "stopped (thread will finish in background)");
        }

        if (x->outlet_right) {
            outlet_anything(x->outlet_right, gensym("stopped"), 0, nullptr);
        }

    } catch (const std::exception& e) {
        object_error((t_object*)x, "stop error: %s", e.what());
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

// eval simply echoes its arguments out the right outlet. It does not reach a
// Jupyter client -- use `print` for that.
void kernel_eval(t_kernel* x, t_symbol* s, long argc, t_atom* argv) {
    if (argc < 1) {
        object_error((t_object*)x, "eval requires at least one argument");
        return;
    }

    if (x->debug) {
        const std::string code = atoms_to_string((t_object*)x, argc, argv);
        object_post((t_object*)x, "eval '%s'", code.c_str());
    }

    if (x->outlet_right) {
        outlet_anything(x->outlet_right, gensym("eval"), argc, argv);
    }
}

void kernel_info(t_kernel* x) {
    auto* impl = x->impl;
    if (!impl) {
        object_error((t_object*)x, "not initialized");
        return;
    }

    // kernel_info_request is only available when the interpreter is registered
    // and the kernel is running. Use the static info instead.
    try {
        nl::json info;
        info["implementation"] = "max_kernel";
        info["implementation_version"] = MX_KERNEL_VERSION;
        info["language"] = "max";
        info["running"] = (impl->kernel != nullptr);

        object_post((t_object*)x, "=== Kernel Info ===");
        object_post((t_object*)x, "Implementation: max_kernel");
        object_post((t_object*)x, "Version: %s", MX_KERNEL_VERSION);
        object_post((t_object*)x, "Language: max");
        object_post((t_object*)x, "Running: %s", impl->kernel ? "yes" : "no");

        if (x->outlet_right) {
            t_atom atoms[2];
            atom_setsym(&atoms[0], gensym("info"));
            atom_setsym(&atoms[1], gensym(info.dump().c_str()));
            outlet_anything(x->outlet_right, gensym("kernel"), 2, atoms);
        }

    } catch (const std::exception& e) {
        object_error((t_object*)x, "info error: %s", e.what());
    }
}

// ---------------------------------------------------------------------------
// kernel_result -- reply from a Max patch to the cell that is executing
// ---------------------------------------------------------------------------
void kernel_result(t_kernel* x, t_symbol* s, long argc, t_atom* argv) {
    auto* impl = x->impl;
    if (!impl) {
        object_error((t_object*)x, "not initialized");
        return;
    }

    if (argc < 1) {
        object_error((t_object*)x, "result requires at least one argument");
        return;
    }

    mx::ResultMessage result;

    // "result error <ename> <evalue...>" reports a failure for the cell.
    if (argc >= 3 && atom_gettype(argv) == A_SYM &&
        std::string(atom_getsym(argv)->s_name) == "error") {

        if (atom_gettype(argv + 1) == A_SYM) {
            result.error_name = atom_getsym(argv + 1)->s_name;
        } else {
            result.error_name = "MaxError";
        }
        result.error_value = atoms_to_string((t_object*)x, argc, argv, 2);
    } else {
        result.text = atoms_to_string((t_object*)x, argc, argv);
    }

    queue_for_jupyter(x, std::move(result));

    if (x->debug) {
        object_post((t_object*)x, "result queued");
    }
}

// ---------------------------------------------------------------------------
// kernel_print -- stream output from Max to the connected client
// ---------------------------------------------------------------------------
void kernel_print(t_kernel* x, t_symbol* s, long argc, t_atom* argv) {
    auto* impl = x->impl;
    if (!impl) {
        object_error((t_object*)x, "not initialized");
        return;
    }

    if (argc < 1) {
        object_error((t_object*)x, "print requires at least one argument");
        return;
    }

    // Optional leading "stderr" / "stdout" selects the stream.
    long start = 0;
    std::string stream = "stdout";
    if (atom_gettype(argv) == A_SYM) {
        const std::string first = atom_getsym(argv)->s_name;
        if (argc >= 2 && (first == "stdout" || first == "stderr")) {
            stream = first;
            start = 1;
        }
    }

    mx::ResultMessage out;
    out.stream_name = stream;
    out.text = atoms_to_string((t_object*)x, argc, argv, start);

    const int pending = impl->current_execution.load();
    if (pending != 0) {
        out.execution_counter = pending;
        impl->result_queue.push(std::move(out));
    } else {
        impl->async_queue.push(std::move(out));
    }
}

// ---------------------------------------------------------------------------
// kernel_dict -- serialize a Max dict to JSON and send it as a result
// ---------------------------------------------------------------------------

// Convert one Max atom to its JSON equivalent.
static nl::json atom_to_json(t_atom* a);

// Walk a t_dictionary and build the equivalent JSON object. Max's
// dictobj_dictionarytoatoms produces Max dictionary *text*, which is not JSON,
// so building it explicitly is what makes the application/json mime type
// truthful rather than merely plausible.
static nl::json dictionary_to_json(t_dictionary* d) {
    nl::json obj = nl::json::object();
    if (!d) return obj;

    long numkeys = 0;
    t_symbol** keys = nullptr;
    if (dictionary_getkeys(d, &numkeys, &keys) != MAX_ERR_NONE || !keys) {
        return obj;
    }

    for (long i = 0; i < numkeys; i++) {
        t_symbol* key = keys[i];
        const char* name = key->s_name;

        if (dictionary_entryisdictionary(d, key)) {
            t_object* sub = nullptr;
            if (dictionary_getobject(d, key, &sub) == MAX_ERR_NONE && sub) {
                obj[name] = dictionary_to_json((t_dictionary*)sub);
            }
            continue;
        }

        if (dictionary_entryisatomarray(d, key)) {
            t_object* arr = nullptr;
            if (dictionary_getobject(d, key, &arr) == MAX_ERR_NONE && arr) {
                long ac = 0;
                t_atom* av = nullptr;
                atomarray_getatoms((t_atomarray*)arr, &ac, &av);
                nl::json items = nl::json::array();
                for (long j = 0; j < ac; j++) {
                    items.push_back(atom_to_json(av + j));
                }
                obj[name] = std::move(items);
            }
            continue;
        }

        t_atom value;
        if (dictionary_getatom(d, key, &value) == MAX_ERR_NONE) {
            obj[name] = atom_to_json(&value);
        }
    }

    dictionary_freekeys(d, numkeys, keys);
    return obj;
}

static nl::json atom_to_json(t_atom* a) {
    switch (atom_gettype(a)) {
    case A_LONG:
        return nl::json(static_cast<long long>(atom_getlong(a)));
    case A_FLOAT:
        return nl::json(atom_getfloat(a));
    case A_SYM:
        return nl::json(std::string(atom_getsym(a)->s_name));
    case A_OBJ: {
        t_object* o = (t_object*)atom_getobj(a);
        if (o && object_classname(o) == gensym("dictionary")) {
            return dictionary_to_json((t_dictionary*)o);
        }
        if (o && object_classname(o) == gensym("atomarray")) {
            long ac = 0;
            t_atom* av = nullptr;
            atomarray_getatoms((t_atomarray*)o, &ac, &av);
            nl::json items = nl::json::array();
            for (long j = 0; j < ac; j++) {
                items.push_back(atom_to_json(av + j));
            }
            return items;
        }
        return nl::json(nullptr);
    }
    default:
        return nl::json(nullptr);
    }
}

void kernel_dict(t_kernel* x, t_symbol* s) {
    auto* impl = x->impl;
    if (!impl) {
        object_error((t_object*)x, "not initialized");
        return;
    }

    t_dictionary* dict = dictobj_findregistered_retain(s);
    if (!dict) {
        object_error((t_object*)x, "dictionary '%s' not found", s->s_name);
        return;
    }

    mx::ResultMessage result;
    try {
        result.text = dictionary_to_json(dict).dump(2);
        result.mime_type = "application/json";
    } catch (const std::exception& e) {
        dictobj_release(dict);
        object_error((t_object*)x, "failed to serialize dictionary '%s': %s",
                     s->s_name, e.what());
        return;
    }

    dictobj_release(dict);

    queue_for_jupyter(x, std::move(result));

    if (x->debug) {
        object_post((t_object*)x, "dict '%s' queued as JSON", s->s_name);
    }
}

// ---------------------------------------------------------------------------
// kernel_install -- install Jupyter kernelspec
// ---------------------------------------------------------------------------
void kernel_install(t_kernel* x) {
    const char* home = std::getenv("HOME");
    if (!home) {
        object_error((t_object*)x, "HOME not set");
        return;
    }

    const std::string dir =
        std::string(home) + "/.local/share/jupyter/kernels/mx-kernel";

    if (!mx::make_directories(dir)) {
        object_error((t_object*)x, "failed to create kernelspec directory: %s",
                     dir.c_str());
        return;
    }

    // This kernelspec exists so the kernel is discoverable by name. It cannot
    // launch a kernel -- a Max patch has to be running and started first --
    // which the display name makes explicit.
    nl::json spec;
    spec["argv"] = nl::json::array({"echo", "Start the kernel from a Max patch, "
                                            "then connect with --existing"});
    spec["display_name"] = "Max/MSP (connect to a running patch)";
    spec["language"] = "max";

    const std::string filepath = dir + "/kernel.json";
    std::ofstream file(filepath);
    if (!file.is_open()) {
        object_error((t_object*)x, "failed to write kernelspec");
        return;
    }

    file << spec.dump(2);
    file.close();

    if (file.fail()) {
        object_error((t_object*)x, "failed to write kernelspec");
        return;
    }

    object_post((t_object*)x, "kernelspec installed at %s",
                filepath.c_str());

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
        snprintf(s, 256, "Messages: start, stop, info, eval, result, print, dict, install");
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
