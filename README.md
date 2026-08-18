# mx-kernel

A Jupyter kernel embedded in a Max external.

Start a `[kernel]` object in a Max patch, connect a Jupyter client to it, and
cells typed in Jupyter are delivered to your patch; the patch answers, and the
answer becomes the cell's output. Communication is bidirectional and uses the
standard Jupyter wire protocol (5.3) over ZMQ, so any Jupyter client works.

The object is a bridge rather than an evaluator: it does not interpret Max code
itself. What a cell means is whatever your patch does with the text.

```
Jupyter client  <--ZMQ-->  xeus-zmq  <-->  max_interpreter  <-->  Max outlets
```

## Requirements

- macOS (Apple silicon or Intel), Max 8 or 9
- CMake 3.19+, a C++17 compiler
- Homebrew: `brew install zeromq openssl@3 cppzmq nlohmann-json`
- [uv](https://docs.astral.sh/uv/) for the Jupyter client used by `make connect`

Windows is not supported yet. The platform-specific code (CSPRNG, directory
creation) has Windows branches, but the build has never been run there.

## Build

```sh
git submodule update --init --recursive
make build
```

This produces `externals/kernel.mxo`. Dependent dylibs are copied into the
bundle and re-pointed at `@rpath`, so the external loads on machines that do
not have Homebrew.

`make link` symlinks the repository into your Max `Packages` directory so Max
can find the external.

## Use

```
[kernel @name mykernel @debug 1]
```

Send it `start`, then from a terminal:

```sh
make connect NAME=mykernel
```

Cells arrive at the left outlet as `code execute <text>`; your patch replies
with `result <text>`. The full message set, the wire contract, and a worked
round-trip example are in
[source/projects/kernel/README.md](source/projects/kernel/README.md).

Three patches ship with it:

| Patch | Purpose |
|-------|---------|
| `help/kernel.maxhelp` | Minimal echo loop -- proves the round trip |
| `help/kernel-calc.maxpat` | A patch that actually evaluates: `1+1` returns `2` |
| `help/kernel-test.maxpat` | Test rig for streaming, errors, timeouts, dicts |

## Make targets

| Target | Effect |
|--------|--------|
| `make build` | Incremental build of the external |
| `make rebuild` | Clean build |
| `make test` | Build and run the unit tests |
| `make clean` | Remove `build`, `build-test`, and `externals` |
| `make link` | Symlink into the Max `Packages` directory |
| `make setup` | `update-submodules` + `link` |
| `make connect [NAME=...]` | Attach `jupyter console` to a running kernel |
| `make install-kernelspec` | Register the kernel for discovery by name |
| `make patch-thirdparty` | Re-apply local patches to vendored dependencies |

## Tests

```sh
make test
```

The suite covers the Max-free modules: connection file handling, the
thread-safe queues, and the interpreter's protocol semantics (result matching,
timeouts, streaming, shutdown). It also starts a real ZMQ-backed kernel on
loopback to verify that `stop()` is observed, the server thread joins, and the
kernel destructor returns -- the shutdown path that used to hang Max. Those
tests run under a watchdog, so a regression fails the suite instead of hanging
it.

`external.cpp` is the only translation unit that needs the Max SDK and is
excluded; behaviour that lives there still has to be checked by hand in Max --
`source/projects/kernel/README.md` describes the round trip to exercise.

## Project layout

```
source/projects/kernel/
  external.cpp      Max SDK interface -- the only Max-dependent file
  interpreter.cpp   Jupyter protocol semantics (xeus::xinterpreter)
  connection.cpp    Connection file, key generation, path handling
  types.h/.cpp      t_kernel_impl -- the pimpl shared across threads
  message_queue.h   Thread-safe queues and message structs
  version.h         Single source of the version string
  tests/            doctest unit tests
  thirdparty/       Vendored xeus, xeus-zmq, nlohmann/json, doctest
javascript/         calc.js -- the calculator example, with its own tests
patches/            Local patches carried against the vendored trees
script/             Build helpers (dylib bundling)
source/notes/       Investigation records (historical)
```

## Asynchronous output

`print` sends stream output from Max to a connected client at any time --
during a cell, or while the kernel is idle.

Jupyter's IOPub socket belongs to the kernel's server thread and ZMQ sockets are
not thread-safe, so Max's main thread cannot publish directly. Instead it
queues, and the server thread publishes on its next idle poll tick. The vendored
xeus-zmq is patched to poll with a timeout and invoke a callback on each idle
tick, which is what makes that possible; see
[patches/README.md](patches/README.md).

The same patch fixes the shutdown hang at its root, so the kernel is now stopped
and destroyed normally rather than deliberately leaked.

## Vendored dependencies

`xeus`, `xeus-zmq`, `nlohmann/json`, and `doctest` are vendored as checked-in
source under `source/projects/kernel/thirdparty`. Four local patches are carried
against them -- one is load-bearing (the timed poll that makes clean shutdown
possible), the rest are a protocol fix and CMake maintenance. See
[patches/README.md](patches/README.md). Refreshing a vendored dependency
silently reverts them, so run `make patch-thirdparty` afterwards.

## Changelog

[CHANGELOG.md](CHANGELOG.md) records what changed in each version.

## Status

Working proof of concept. The round trip, restart, result matching, timeouts,
streaming, asynchronous output and clean shutdown all work, and execution is
asynchronous so a waiting cell no longer blocks the shell or control channels.
The main gaps are no completion or inspection content, no interrupt handler,
and no Windows build.

## License

GPL-3.0 (`LICENSE`).

Note that the external links the Max SDK, whose license is not GPL-compatible;
distributing built binaries under GPL-3 is therefore questionable. Most Max
externals use MIT or BSD, and the vendored xeus stack is BSD-3. This should be
settled before any release.
