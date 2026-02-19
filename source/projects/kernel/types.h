#pragma once

#include <atomic>
#include <memory>
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
struct t_kernel_impl {
    std::unique_ptr<max_interpreter> interpreter;
    std::unique_ptr<xeus::xkernel> kernel;
    std::unique_ptr<xeus::xcontext> context;
    std::thread* kernel_thread = nullptr;
    std::string connection_file;

    ThreadSafeQueue<OutletMessage> outlet_queue;
    ThreadSafeQueue<ResultMessage> result_queue;

    std::atomic<bool> alive{true};
    long timeout = 30; // seconds, configurable via @timeout attribute

    // Opaque pointer to qelem (owned by the Max external, stored here for
    // the interpreter to call qelem_set without needing Max headers).
    void* qelem = nullptr;
};

} // namespace mx
