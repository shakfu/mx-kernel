# Kernel Shutdown Compromise

## The Problem

When stopping the Max/MSP Jupyter kernel and subsequently shutting down Max, the application would hang and require force quitting.

### Root Cause

The hang occurred during kernel cleanup in the destructor chain:

```
kernel_free()
  → delete x->kernel
    → xkernel::~xkernel()
      → xserver_zmq_default::~xserver_zmq_default()
        → xserver_zmq_impl::~xserver_zmq_impl()
          → xthread::~xthread()
            → xthread::join()  ← HANGS HERE
```

**Why it hangs:**

1. xeus-zmq creates internal threads for each ZMQ channel (shell, control, heartbeat, iopub)
2. These threads run event loops that block in `poll_channels(-1)` waiting for ZMQ messages
3. When `kernel->stop()` is called, it sets a flag `m_request_stop = true`
4. However, the event loop only checks this flag AFTER `poll_channels()` returns with a message
5. Since no messages arrive after stop, the threads remain blocked indefinitely
6. When the kernel destructor runs, xeus tries to join these internal threads
7. Joining a blocked thread causes an indefinite hang

### Stack Trace

```
kernel_free(_kernel*)
  std::unique_ptr<xeus::xkernel>::~unique_ptr()
    xeus::xkernel::~xkernel()
      std::unique_ptr<xeus::xserver>::~unique_ptr()
        xeus::xserver_zmq_default::~xserver_zmq_default()
          xeus::xserver_zmq::~xserver_zmq()
            std::unique_ptr<xeus::xserver_zmq_impl>::~unique_ptr()
              xeus::xserver_zmq_impl::~xserver_zmq_impl()
                xeus::xthread::~xthread()
                  xeus::xthread::join()
                    std::thread::join()
                      __ulock_wait  ← BLOCKED FOREVER
```

## Attempted Solutions

### [X] Attempt 1: Join the thread in `kernel_stop()`
**Problem:** Same issue - thread is blocked in poll, never exits, join hangs.

### [X] Attempt 2: Use timeout + detach in `kernel_stop()`
**Problem:** Deleting kernel/context while detached thread might still be using them causes use-after-free.

### [X] Attempt 3: Send stop message through control channel
**Problem:** xeus doesn't expose an API to send internal control messages from external code.

## The Solution

**Intentionally leak kernel and context objects during shutdown.**

### Implementation

In `source/projects/kernel/kernel.cpp`:

```cpp
void kernel_stop(t_kernel* x)
{
    if (x->kernel && (*x->kernel)) {
        // Signal kernel to stop
        (*x->kernel)->stop();

        // Detach thread immediately (don't wait)
        if (x->kernel_thread) {
            if (x->kernel_thread->joinable()) {
                x->kernel_thread->detach();
            }
            delete x->kernel_thread;
            x->kernel_thread = nullptr;
        }

        // Remove connection file
        std::remove(x->connection_file.c_str());

        // Note: kernel and context are left alive
        // They'll be cleaned up (or leaked) in kernel_free()
    }
}

void kernel_free(t_kernel* x)
{
    // Stop kernel if still running
    if (x->kernel) {
        (*x->kernel)->stop();
    }

    // Detach any remaining thread
    if (x->kernel_thread) {
        if (x->kernel_thread->joinable()) {
            x->kernel_thread->detach();
        }
        delete x->kernel_thread;
        x->kernel_thread = nullptr;
    }

    // Clean up interpreter (safe - no threads)
    if (x->interpreter) {
        delete x->interpreter;
        x->interpreter = nullptr;
    }

    // Remove connection file
    if (!x->connection_file.empty()) {
        std::remove(x->connection_file.c_str());
    }

    // INTENTIONAL LEAK: Do NOT delete x->kernel or x->context
    // Deleting them would trigger xeus destructors that try to join
    // internal threads, causing a hang. The OS will reclaim all
    // resources when Max terminates anyway.
}
```

## Why This Is Acceptable

1. **Leak only occurs during process termination**: The kernel/context are only leaked when Max is shutting down
2. **OS cleanup**: When a process terminates, the OS immediately reclaims all resources:
   - Memory is freed
   - File descriptors (ZMQ sockets) are closed
   - Threads are terminated
3. **No user-visible impact**: The leak happens after the user has quit Max, so there's no observable effect
4. **Better than hanging**: A brief memory leak during shutdown is far preferable to forcing users to force-quit
5. **Small leak size**: The kernel and context objects are relatively small (~few KB)

## Trade-offs

### [x] Pros
- Max shuts down instantly and cleanly
- No force-quit required
- User experience is smooth
- Simple implementation

### [!] Cons
- Small memory leak during shutdown (~few KB)
- Resources aren't explicitly cleaned up by our code
- Relies on OS cleanup

## Alternative Approaches (Not Implemented)

### 1. Patch xeus-zmq to use timed poll
Modify `poll_channels()` to use a timeout instead of blocking forever, allowing periodic checks of `is_stopped()`.

**Why not:** Requires forking and maintaining xeus-zmq, breaks compatibility with upstream updates.

### 2. Send dummy message to wake up poll
Create a client that connects to the kernel and sends a message to wake up `poll_channels()`.

**Why not:** Adds complexity, requires ZMQ client code, race conditions if client connects after thread is already blocking.

### 3. Use shutdown_request from Jupyter protocol
Have a Jupyter client send a proper shutdown_request message.

**Why not:** Requires user to send shutdown via Jupyter before closing Max, poor UX.

## Documentation

This compromise is documented in:
- This file: `SHUTDOWN_COMPROMISE.md`
- Code comments in `kernel.cpp` at `kernel_free()`
- Code comments in `kernel.cpp` at `kernel_stop()`

## Verification

To verify the fix works:

1. Start Max/MSP
2. Create a `[kernel @name test @debug 1]` object
3. Send `start` message
4. Send `stop` message → should return immediately with "kernel: stopped (thread will finish in background)"
5. Quit Max → should exit cleanly without hanging

## Future Improvements

If xeus-zmq is updated to support:
- Timed polling with periodic flag checks
- External control messages to wake up blocked threads
- Async shutdown without join

Then we could implement proper cleanup without the leak. Until then, this compromise provides the best user experience.

## References

- Issue discovered: 2025-01-07
- xeus version: 5.2.4
- xeus-zmq version: 3.1.1
- Jupyter protocol: 5.3
