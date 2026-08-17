# kernel

A Jupyter kernel embedded in a Max external. A Jupyter client (console,
notebook, or Lab) connects to a running Max patch over ZMQ and drives it: cells
typed in Jupyter arrive at the object's left outlet, and the patch answers with
a `result` message that becomes the cell's output.

The object is a bridge, not an evaluator. It does not interpret Max code
itself -- what a cell *means* is whatever your patch does with the text.

## Quick start

Create the object, connect both outlets, and start it:

```
[kernel @name mykernel @debug 1]
|          |
[print out] [print status]
```

Send it `start`. The Max console shows the connection file path:

```
kernel: connection file: ~/.local/share/jupyter/runtime/kernel-mykernel.json
kernel: shell=52431 control=52432 iopub=52433 stdin=52434 hb=52435
kernel: started successfully
```

Then connect from a terminal:

```sh
make connect NAME=mykernel
```

or directly:

```sh
uv run jupyter console --existing ~/.local/share/jupyter/runtime/kernel-mykernel.json
```

## The wire contract

This is the whole protocol between the object and your patch. Everything else
is detail.

**Kernel to patch** (left outlet):

| Message | When |
|---------|------|
| `code execute <text>` | A Jupyter cell was run. `<text>` is the cell's contents as a single symbol. |

**Patch to kernel** (inlet):

| Message | Effect |
|---------|--------|
| `result <text...>` | Completes the running cell. The text becomes `Out[n]`. |
| `result error <ename> <evalue...>` | Fails the running cell with that error name and message. |
| `print <text...>` | Streams one line to the client without completing the cell. A newline is appended. |
| `print stderr <text...>` | Same, on stderr. `print stdout ...` is also accepted. |
| `dict <dict-name>` | Sends a named Max dictionary as JSON, completing the cell. |

**Kernel to patch** (right outlet, status):

| Message | When |
|---------|------|
| `started connection_file <path>` | `start` succeeded. |
| `stopped` | `stop` succeeded. |
| `shutdown` | A Jupyter client requested shutdown. Not emitted for a `stop` sent from the patch, which reaches the same code path inside xeus. |
| `kernel info <json>` | Reply to `info`. |
| `installed <path>` | Reply to `install`. |
| `eval <args...>` | Echo of an `eval` message. |

### Closing the loop

Nothing answers a cell unless you wire it. The minimal echo patch:

```
[kernel @name mykernel]
 |    ^
 |    |
[route code]
 |
[route execute]
 |
[prepend result]
 |
(back to the kernel inlet)
```

With that in place:

```
In [1]: hello
Out[1]: 'hello'
```

Without it, every cell waits for `@timeout` seconds and then fails with
`MaxTimeout`. That is the intended signal: a cell that nobody answered did not
succeed.

## Messages

- **bang** -- output a bang from the right outlet.
- **start** -- allocate ports, write the connection file, and begin serving.
  Safe to call again after `stop`.
- **stop** -- stop serving and delete the connection file. The object can be
  restarted with `start`.
- **info** -- report implementation, version, language, and whether the kernel
  is currently running, to the Max console and the right outlet.
- **eval `<args...>`** -- echo the arguments out the right outlet with an `eval`
  selector. This is a patch-side convenience and does **not** reach a Jupyter
  client; use `print` for that.
- **result `<text...>`** -- answer the cell that is currently executing.
- **result error `<ename> <evalue...>`** -- fail the cell that is currently
  executing.
- **print `[stdout|stderr] <text...>`** -- send stream output to the client.
  One message is one line: a newline is appended unless the text already ends
  in one.
- **dict `<dict-name>`** -- serialise a registered Max dictionary to JSON and
  send it as the cell's result, tagged `application/json`. Nested dictionaries
  and atomarrays are converted recursively.
- **install** -- write a Jupyter kernelspec to
  `~/.local/share/jupyter/kernels/mx-kernel`. This only makes the kernel
  discoverable by name; it cannot launch one, because a kernel only exists
  while a Max patch is running. Start from Max and connect with `--existing`.

## Attributes

- **name** (symbol) -- names the connection file
  (`kernel-<name>.json`). Characters outside `A-Za-z0-9._-` are stripped so the
  name cannot escape the runtime directory. Defaults to `max-<address>`.
- **debug** (0/1) -- verbose logging to the Max console.
- **timeout** (int, seconds, default 30) -- how long a cell waits for a
  `result` before failing with `MaxTimeout`. Set `@timeout 0` for
  fire-and-forget: cells return `ok` as soon as the code reaches the outlet,
  without waiting for any answer.

## How results are matched to cells

A `result` is an answer to whichever cell is executing at the moment it
arrives. The object stamps it with that cell's execution counter, and the
interpreter discards anything stamped for a different cell. It also clears any
leftover replies when a new cell starts.

This matters because the failure it prevents is silent. Previously an extra or
late `result` was delivered as the answer to the *next* cell, and every
subsequent cell stayed off by one -- wrong output attributed to the wrong
input, with no error anywhere.

A `result` that arrives when no cell is running is not discarded: it is
converted to stream output and shown with the next cell, so nothing is lost but
nothing masquerades as a result either.

## Output outside a cell

`print` works at any time, whether or not a cell is running.

During a cell it is delivered as the waiting thread drains the queue, so a
long-running patch can stream progress and then answer:

```
print working on it
print still going
result done
```

produces two stream lines followed by `Out[n]: done`.

While the kernel is idle, output is published from the server loop's idle tick.
Jupyter's IOPub socket is owned by that thread and ZMQ sockets are not
thread-safe, so Max's main thread queues the text and the server thread
publishes it on its next poll -- within the poll interval (50ms), not at the
next cell.

This depends on the local timed-poll patch against xeus-zmq
(`patches/xeus-zmq-0003-*`). Without it the server thread blocks indefinitely
between requests, there is no idle tick, and queued output waits for the next
cell.

## Manual test walkthrough

`external.cpp` cannot be unit tested, so the round trip is checked by hand.
This is the procedure; `help/kernel.maxhelp` has it already wired.

**1. Start the kernel.** Create `[kernel @name test @debug 1 @timeout 30]` and
send `start`. The Max console should report a connection file and five ports.

**2. Connect.** Run `make connect NAME=test`. You should get an `In [1]:`
prompt with no exception on connect. (An exception here means the vendored
xeus-zmq patch has been lost -- see `patches/README.md`.)

**3. Jupyter to Max.** Type anything. The left outlet should emit
`code execute "hello world"`. With nothing wired back, the cell fails after 30
seconds with `MaxTimeout`, which is the correct signal for an unanswered cell.

**4. Max to Jupyter.** Wire the loop shown above, then re-run the cell. It
should return immediately as `Out[1]`. Send an error instead to check the
other path:

```
result error MaxError "something went wrong"
```

**5. Streaming.** Send two `print` messages and then a `result` while a cell is
waiting. Both lines should appear before the result, and the cell should
complete only on the `result`.

**6. Result matching.** Send a `result` while no cell is running. It should
appear promptly as stream output in the client -- not as the next cell's
`Out[n]`. Then answer one cell twice; the second answer must not become the
next cell's output.

**7. Dictionaries.** Populate a `[dict mydict]`, then send `dict mydict`. The
cell output should be valid JSON.

**8. Restart.** Send `stop`, then `start` again. The kernel should come back up
with a fresh connection file, and a newly attached client should work. (This
is the path that used to be broken: `stop` left the kernel pointer set, so the
next `start` reported "already running".)

**9. Shutdown.** With a client attached, send a shutdown request from Jupyter.
The right outlet should emit `shutdown`, and the object should still execute
cells after a subsequent `start`.

**10. Teardown.** Delete the object while a client is connected, then quit Max.
Max should exit without hanging and without a crash report. If the Max console
warns that the server thread did not stop in time, the xeus-zmq timed-poll
patch is missing -- run `make patch-thirdparty` and rebuild.

**11. Idle output.** With a client attached and no cell running, send
`print hello`. It should appear in the client within a fraction of a second,
without needing to run a cell.

## Threading

- The kernel runs on its own thread. `start` returns immediately.
- Kernel thread to Max: messages go through a queue and a `qelem`, so
  `outlet_anything` is only ever called on Max's main thread.
- Max to kernel thread: a second queue, drained by the cell that is waiting.
- The object's C++ state lives behind a pimpl (`t_kernel_impl`) allocated with
  `operator new`, so Max's C allocator never touches non-trivial C++ members.

## Shutdown

`stop` asks the server loop to exit, waits for its thread, and destroys the
kernel and ZMQ context. Deleting the object does the same before freeing
everything else.

This works because the vendored xeus-zmq is patched to poll with a timeout
(`patches/xeus-zmq-0003-*`). Upstream, the loop blocks in `poll_channels(-1)`
until a message arrives, so after `stop()` it never re-tests its stop flag and
never shuts its channels down; destroying the kernel then blocks forever
joining the publisher and heartbeat threads, which hangs Max on quit. The
project previously coped by leaking the kernel, the context, and the object's
own C++ state. That leak is gone.

Two safeguards remain:

- The wait for the thread has a 2 second deadline. If it expires -- which would
  mean the patch is missing or the loop is wedged -- the object falls back to
  the old behaviour, detaching the thread and leaking everything it can reach,
  and warns in the Max console. Freezing Max's main thread is never the right
  answer.
- The `qelem` notifier is cleared under a mutex before the `qelem` is freed, so
  even in that fallback case the kernel thread cannot signal freed memory.

`source/notes/shutdown_compromise.md` records the original diagnosis.
`tests/test_server_shutdown.cpp` pins the behaviour: it starts a real kernel and
asserts that stop, join and destruction all complete.

## Protocol support

Implemented: `execute_request`, `complete_request` (empty matches),
`inspect_request` (not found), `is_complete_request`, `kernel_info_request`,
`shutdown_request`. Protocol version 5.3, via xeus 5.2.4 and xeus-zmq 3.1.1.

Not implemented: completion and inspection with real content, `interrupt_request`,
stdin / `input_request`, comms and widgets, rich media beyond a single mime
type per result.

## Building

From the repository root:

```sh
make build     # incremental
make rebuild   # clean build
make test      # unit tests
```

Output: `externals/kernel.mxo`. Dependent dylibs (libzmq, libcrypto, libsodium)
are copied into the bundle and re-pointed at `@rpath`, so the external loads on
machines without Homebrew.
