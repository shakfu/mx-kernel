# Changelog

All notable changes to mx-kernel are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
The version reported to Max and to Jupyter clients lives in
`source/projects/kernel/version.h`.

## [0.2.0] - 2026-08-18

The project became usable by someone other than its author. Most of this
release comes from acting on `REVIEW.md`: the object no longer works only once
per instantiation, the built external loads on machines without Homebrew, and
the shutdown hang is fixed at its cause rather than worked around.

### Added

- `print` message, sending stream output from Max to a connected client. During
  a cell it is delivered as it arrives; while the kernel is idle it is
  published on the server loop's next poll tick.
- Asynchronous execution. A cell waiting on Max no longer parks the server
  thread, so the shell and control channels stay answerable -- the Jupyter
  protocol requires `shutdown_request` and `interrupt_request` to work *during*
  execution, and previously they did not.
- Result correlation. Replies are stamped with the executing cell's counter and
  mismatches are discarded, so a late or duplicated `result` cannot be
  delivered as another cell's output.
- `@timeout 0` for fire-and-forget cells, which return as soon as the code
  reaches the outlet.
- Dependency bundling. `script/bundle_dylibs.sh` copies libzmq, libcrypto and
  libsodium into `kernel.mxo` and rewrites their install names to `@rpath`,
  as part of `make build`.
- A patch mechanism for the vendored dependencies: `patches/` with an
  idempotent `patches/apply.sh`, run via `make patch-thirdparty`.
- Calculator example -- `javascript/calc.js` and `help/kernel-calc.maxpat` --
  showing a patch that evaluates rather than echoes. The parser is ES5 with no
  `eval`, so a cell cannot execute arbitrary code, and it has its own tests.
- Test rig patch, `help/kernel-test.maxpat`, covering streaming, errors,
  timeouts, dictionaries and restart.
- Unit tests for the interpreter, and integration tests that start a real
  ZMQ-backed kernel and assert that stop, join and destruction all complete.
  6 test cases / 109 assertions grew to 42 / 261, plus 48 JavaScript cases.
- CI (`.github/workflows/test.yml`), including a check that the built external
  contains no absolute Homebrew paths.

### Changed

- Kernel and Jupyter version strings come from one header (`version.h`).
- A timeout replies `status: error` with `ename: MaxTimeout`. It previously
  replied `ok` with a note on stdout, so programmatic clients saw a successful
  execution with no result.
- Stream output is newline terminated. Jupyter concatenates stream text
  verbatim, so consecutive messages previously ran together and the next
  `Out[n]` collided with them.
- `dict` builds real JSON by walking the dictionary, including nested
  dictionaries and atomarrays. It previously sent Max's dictionary text format
  under the `application/json` mime type.
- `kernel_info` declares `pygments_lexer`, so clients no longer warn that no
  lexer exists for language `max`.
- The kernelspec written by `install` is named "Max/MSP (connect to a running
  patch)", making explicit that it cannot launch a kernel.
- Connection files are created with `0600` permissions via `open(2)`, so the
  HMAC key is never briefly world readable.
- Kernel names are restricted to `[A-Za-z0-9._-]`, so a name cannot escape the
  runtime directory.
- Random key generation uses `arc4random_buf`, `BCryptGenRandom` or
  `/dev/urandom` rather than `std::random_device`, which is permitted to be
  deterministic.
- `std::system("mkdir -p ...")` replaced with direct directory creation,
  removing a shell invocation with an interpolated `HOME`.
- `external.cpp` split out a shared `atoms_to_string` helper that warns on
  unhandled atom types instead of silently dropping them.
- Documentation rewritten: the object README now documents the wire contract
  and every message, `how-to-test.md` was folded into it as a walkthrough, and
  `source/notes/` is dated and framed as historical.

### Fixed

- **Use-after-free on object deletion.** The design deliberately leaked the
  kernel, but freed the `t_kernel_impl` its interpreter still pointed at and
  the `qelem` it still signalled. Deleting a `[kernel]` object while a client
  was connected could write into freed memory.
- **The object worked only once per instantiation.** `eval` was permanently
  broken after `start`; `start` after `stop` reported "already running"
  forever; and a Jupyter shutdown request left every later cell falling
  straight through to the no-response path.
- **The built external was not distributable.** It linked libzmq and libcrypto
  by absolute Homebrew path, so it failed to load on any machine without
  Homebrew at the same prefix -- including every Intel Mac.
- **Shutdown no longer leaks.** The kernel, context and impl were deliberately
  leaked to avoid a destructor that hung Max. With the timed-poll patch below,
  the server thread is joined and everything is destroyed normally; a
  deadline-bounded fallback to the old behaviour remains if the join stalls.
- Stream output no longer ends a cell, so a patch can report progress and then
  answer.
- `execute_input` is published once per cell. xeus publishes it and the
  interpreter was publishing it again.
- Deferred cells publish under their own request context. xeus keeps a single
  context that a newly dispatched request overwrites, which would have
  attributed a running cell's output to a newer one.
- The `shutdown` status is no longer emitted for a `stop` sent from the patch;
  `xkernel::stop()` reaches the same handler as a client request.
- Console messages are no longer double-prefixed (`kernel: kernel: ...`), since
  Max already prefixes the object name.
- `make clean` removes `externals` -- it was misspelled `exterals`, so stale
  builds survived every clean. `.phony` corrected to `.PHONY`, the undefined
  `section` macro defined, `build` made incremental, and `connect` takes
  `NAME=`.

### Vendored dependency patches

Local changes carried against the vendored trees, documented in
`patches/README.md` and applied in the checked-in source:

- **xeus-zmq: timed poll and idle callback.** `poll_channels(-1)` blocked
  until a message arrived, so after `stop()` the server loop never re-tested
  its stop flag, never shut its channels down, and the destructor hung joining
  the publisher and heartbeat threads. Polling with a timeout fixes the hang at
  its cause and gives an embedder a hook to publish from the thread that owns
  the sockets. Also makes the stop flag atomic: it is read and written from two
  threads, and once the loop reads it every tick the compiler may hoist the
  load and lose a stop request entirely.
- **xeus-zmq: `iopub_welcome` parent header.** Sends `{}` rather than `null`
  for `parent_header` and `metadata`, as the messaging specification requires.
  Without it jupyter-console raises `'NoneType' object has no attribute 'get'`
  on connect. (Carried since 0.1.0; documented as a patch in this release.)
- **xeus and xeus-zmq: CMake policy range.** Declares
  `cmake_minimum_required(VERSION 3.8...3.31)` so CMake 3.31+ stops warning
  about pre-3.10 compatibility, which is on a path to removal.

None of these are specific to this project and all should be reported upstream.

### Known limitations

- Cells run one at a time, and xeus publishes `execute_input` at dispatch, so a
  client that queues several cells sees all their `In[n]` numbers at once with
  outputs filling in afterwards.
- No completion or inspection content, and no `interrupt_request` handler.
- The external is arm64 only. The bundled Homebrew dylibs are too, so a
  universal build needs more than enabling `C74_BUILD_FAT`.
- Windows is unsupported. The platform-specific code has Windows branches, but
  the build has never been run there.
- The project is GPL-3 while linking the proprietary Max SDK. This should be
  settled before any release.

## [0.1.0] - 2025-11-07

First working proof of concept: a Jupyter kernel embedded in a Max external,
with bidirectional messaging between a Jupyter client and a Max patch over ZMQ.

### Added

- `kernel` external built on xeus 5.2.4 and xeus-zmq 3.1.1, implementing
  Jupyter wire protocol 5.3 over all five channels.
- Connection file generation, so clients attach with `jupyter console
  --existing`.
- `start`, `stop`, `info`, `eval`, `bang`, `result` and `dict` messages, and
  `name`, `debug` and `timeout` attributes.
- Kernel runs on its own thread, with queues and a `qelem` marshalling messages
  back to Max's main thread.

### Fixed

- jupyter-console raising `'NoneType' object has no attribute 'get'` on
  connect, traced to xeus-zmq sending `null` instead of `{}` for
  `parent_header`.

### Known issues

- Max hangs on quit unless the kernel and ZMQ context are deliberately leaked;
  see `source/notes/shutdown_compromise.md`.
