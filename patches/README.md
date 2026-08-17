# Local patches against vendored dependencies

The dependencies under `source/projects/kernel/thirdparty` are vendored as
plain checked-in files rather than submodules. That makes a local fix
indistinguishable from upstream source, and means refreshing a dependency
silently reverts it. This directory is the record of what has been changed and
why.

## Current patches

| Patch | Target | Summary |
|-------|--------|---------|
| `xeus-zmq-0001-iopub-welcome-parent-header.patch` | xeus-zmq 3.1.1 | Send `{}` instead of `null` for `parent_header` and `metadata` on the startup `iopub_welcome` message |
| `xeus-zmq-0002-cmake-policy-range.patch` | xeus-zmq 3.1.1 | Declare a `cmake_minimum_required` policy range so CMake 3.31+ stops warning about pre-3.10 compatibility |
| `xeus-zmq-0003-timed-poll-and-idle-callback.patch` | xeus-zmq 3.1.1 | Poll the server loop with a timeout instead of blocking forever, add an idle callback, and make the stop flag atomic |
| `xeus-0002-cmake-policy-range.patch` | xeus 5.2.4 | Same change in xeus |

## Applying

The patches are already applied in the checked-in tree, so an ordinary build
needs no action. After refreshing a vendored dependency, run:

```sh
make patch-thirdparty
```

`patches/apply.sh` is idempotent: an already-applied patch is reported as such
rather than failing.

## Why patch 0001 matters

Without it, connecting with `jupyter console` fails immediately:

```
Exception 'NoneType' object has no attribute 'get'
```

The cause is not obvious from that message -- it took a full investigation to
trace it from a Python traceback in `jupyter_console/ptshell.py` back to an
uninitialised `nl::json` in xeus-zmq's publisher. `source/notes/jupyter_console_issue.md`
records that investigation. Losing the two-line fix means repeating it.

The patched lines carry a comment in the vendored source pointing back here, so
the divergence is visible when reading the file directly.

## Why the 0002 patches matter

`cmake_minimum_required(VERSION 3.8)` makes CMake 3.31 and newer emit:

```
CMake Warning (deprecated) at CMakeLists.txt:10 (cmake_minimum_required):
  Compatibility with CMake < 3.10 will be removed from a future version of CMake.
```

This is not only noise. CMake 4 already removed support for minimums below 3.5
(the root `CMakeLists.txt` sets `CMAKE_POLICY_VERSION_MINIMUM 3.5` to cope), and
the floor keeps rising. When it passes 3.8 the vendored trees stop configuring
at all.

The patches take the second option the warning itself suggests: the
`<min>...<max>` range keeps the declared floor at 3.8, so older CMake still
works, while telling CMake the sources have been checked against policies up to
3.31. The bundled nlohmann/json already uses this form, which is why it never
warned.

Only the two top-level `CMakeLists.txt` files are patched -- the `test/`,
`docs/` and `example/` subdirectories carry the same old minimums but are never
added to our build.

## Why patch 0003 matters

This is the load-bearing one. `poll_channels(-1)` blocks until a message
arrives, so after `stop()` the server loop never re-tests its stop flag, never
reaches `stop_channels()`, and the destructor then blocks forever joining the
publisher and heartbeat threads. Embedded in Max, that is a hang on quit
requiring a force quit.

The project previously worked around it from the outside by deliberately
leaking the kernel, the ZMQ context, and the object's own C++ state. Patch 0003
removes the need: the loop notices the stop request within its poll interval,
shuts the channels down properly, and the thread can be joined and the kernel
destroyed normally.

It also gives an embedder somewhere safe to publish. The IOPub socket belongs
to the server thread and ZMQ sockets are not thread-safe, so output originating
in Max cannot be published from Max's thread. The idle callback runs on the
right thread, which is what makes `print` work while no cell is executing.

The patch additionally makes `xserver_zmq_impl::m_request_stop` atomic. It was
already read and written from two threads; while the loop sat parked in `poll()`
that race was easy to miss, but once the loop reads the flag every tick the
compiler may hoist the load and the stop request can be lost entirely.

`source/projects/kernel/tests/test_server_shutdown.cpp` covers this: it starts a
real kernel on loopback and asserts that `stop()` is observed, the thread joins,
and the destructor returns. Reverting the poll to `-1` makes those tests hang,
which their watchdog turns into a hard failure -- so the tests genuinely pin the
behaviour rather than passing vacuously.

## Upstreaming

None of these are specific to this project:

- **0001** is a genuine spec-compliance bug affecting every kernel built on
  xeus-zmq.
- **0002** is routine maintenance that upstream will need anyway as CMake's
  floor rises.
- **0003** fixes a hang that affects any application embedding an xeus-zmq
  kernel in-process, and adds a small, optional API. This is the one most worth
  discussing upstream, since the API shape should be theirs rather than ours.

Carrying them locally forever is the worse end state -- they should be reported
to https://github.com/jupyter-xeus/xeus-zmq and
https://github.com/jupyter-xeus/xeus, and dropped from here once released.

Not yet reported.
