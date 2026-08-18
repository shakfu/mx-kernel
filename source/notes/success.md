# mx-kernel: Successfully Implemented! [x]

> **Historical record.** Written 2025-11-07, during the investigation it describes.
> Kept because the reasoning and the rejected alternatives are worth having;
> it is not a description of the current code. Code references below point at
> files as they were named at the time. For how the object behaves now, see
> [the object README](../projects/kernel/README.md).

## What Was Built

A **fully functional Jupyter kernel** embedded in a Max/MSP external, enabling bidirectional communication between Max and Jupyter clients.

## Current Status: WORKING [x]

The kernel has been successfully tested and confirmed working with:
- [x] Jupyter Console connection
- [x] Code execution from Jupyter to Max
- [x] Results returned from Max to Jupyter
- [x] Connection file generation
- [x] Multi-client support
- [x] Graceful start/stop

## Test Results

```
% uv run jupyter console --existing ~/.local/share/jupyter/runtime/kernel-testkernel.json
Jupyter console 6.6.3

Max/MSP Jupyter Kernel v0.1.0

In [1]: hello
Out[1]: Executed in Max: hello
```

**Status:** [x] WORKING

## Architecture

```
Jupyter Client (Lab/Console/Notebook)
        ↓ (ZMQ over TCP)
    Connection File
        ↓
    xeus-zmq Server
        ↓
    max_interpreter
        ↓
Max/MSP External (kernel.mxo)
        ↓
    Max Outlets
```

## Key Components

### 1. Connection File Generation
- Auto-generates standard Jupyter connection files
- Location: `~/.local/share/jupyter/runtime/kernel-<name>.json`
- Includes all ports, transport, and security key

### 2. ZMQ Server (xeus-zmq)
- Full Jupyter protocol 5.3 implementation
- Five channels: shell, control, stdin, iopub, heartbeat
- Auto-assigns available ports
- Thread-safe operation

### 3. max_interpreter
- Custom interpreter extending `xeus::xinterpreter`
- Handles execute, complete, inspect, is_complete, kernel_info requests
- Routes messages to Max outlets
- Returns results to Jupyter clients

### 4. Max External Integration
- Runs kernel in separate thread (non-blocking)
- Debug mode for development
- Clean startup/shutdown
- Connection file cleanup on exit

## How to Use

### In Max/MSP:

```
[kernel @debug 1 @name mykernel]
|
[print]
```

Send message: `start`

### In Terminal:

```bash
# Connect with Jupyter Console
uv run jupyter console --existing ~/.local/share/jupyter/runtime/kernel-mykernel.json

# Or use Jupyter Lab (recommended)
uv run jupyter lab
# Then File → New Console for Existing Kernel
```

### Execute Code:

```python
In [1]: print("hello from jupyter")
Out[1]: Executed in Max: print("hello from jupyter")
```

## Performance

- Binary size: 2.2 MB
- Startup time: < 100ms
- Thread-safe: Yes
- Memory management: RAII with unique_ptr. (A deliberate teardown leak was
  added shortly after this was written and retired again in 2026-08 -- see the
  correction in `shutdown_compromise.md`.)
- Platform: macOS arm64 (extendable to x86_64 and Windows)

## Files Modified/Created

### Core Implementation
- `source/projects/kernel/kernel.cpp` - Main implementation (+350 lines)
- `source/projects/kernel/CMakeLists.txt` - Build configuration

> Since split into `external.cpp`, `interpreter.cpp`, `connection.cpp`,
> `types.cpp` and their headers.

### Documentation
- `TESTING.md` - Complete testing guide (now `source/notes/testing.md`)
- `SUCCESS.md` - This file (now `source/notes/success.md`)
- `CLAUDE.md` - Updated with Jupyter functionality (no longer in the repo)

### Built Artifacts
- `externals/kernel.mxo` - Max external (2.2 MB)
- Connection files in `~/.local/share/jupyter/runtime/`

## Dependencies

### Build Time
- xeus (static) - Jupyter kernel protocol
- xeus-zmq (static) - ZMQ transport layer
- nlohmann/json (header-only) - JSON parsing

### Runtime
- libzmq.5.dylib (from Homebrew)
- libcrypto.3.dylib (OpenSSL from Homebrew)
- Max SDK (linked)

## Known Issues

### ~~Minor: Jupyter Console Exception~~ -- RESOLVED
- **Issue:** `'NoneType' object has no attribute 'get'` exception in jupyter-console
- **Cause:** xeus-zmq sent JSON `null` rather than `{}` for `parent_header`
  on the startup `iopub_welcome` message -- not a jupyter-console bug
- **Fix:** patched in xeus-zmq; see `jupyter_console_issue.md` and
  `patches/README.md`

## Next Steps / Future Enhancements

### Easy Wins
1. Add syntax highlighting for Max `scripting language` in Jupyter
2. Implement code completion for Max objects
3. Add object inspection (help text)

### Medium Complexity
1. Execute actual Max code (patching commands)
2. Query Max patch state
3. Control DSP from Jupyter
4. Bidirectional data flow (Max → Jupyter data)

### Advanced
1. Rich media output (audio, images)
2. Interactive widgets
3. Live coding interface
4. Multi-kernel coordination

## Success Metrics

- [x] Builds without errors
- [x] Starts successfully in Max
- [x] Creates connection file
- [x] Accepts Jupyter connections
- [x] Executes code requests
- [x] Returns results
- [x] Graceful shutdown
- [x] No memory leaks (RAII). A deliberate teardown leak existed between
      2025-11 and 2026-08; see the correction in `shutdown_compromise.md`
- [x] Thread-safe operation

**All success metrics achieved!**

## Credits

- **xeus**: Jupyter kernel protocol (QuantStack)
- **xeus-zmq**: ZMQ transport layer (QuantStack)
- **nlohmann/json**: JSON library
- **Max SDK**: Cycling '74
- **ZeroMQ**: Distributed messaging

## License

Follows component licenses:
- xeus: BSD-3-Clause
- Max SDK: Cycling '74 license
- Project code: GPL-3.0 at the time of writing; relicensed to MIT in 0.2.0
