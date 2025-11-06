# kernel

This project embeds a Jupyter kernel inside a Max external using the xeus C++ implementation of the Jupyter kernel protocol. This enables remote terminal interaction with Max, allowing users to send messages to the external object via a Jupyter interface as if they were Max messages.

## Overview

The `kernel` external provides bidirectional integration between Max/MSP and the Jupyter protocol, enabling:

- Remote control of Max objects from Jupyter clients
- Code execution through the Jupyter kernel protocol
- Kernel introspection and metadata queries
- Full lifecycle management (start/stop/info)

## Dependencies

This external links to the following libraries:

- [xeus](https://github.com/jupyter-xeus/xeus) - C++ implementation of the Jupyter kernel protocol
- [nlohmann/json](https://github.com/nlohmann/json) - JSON library for Modern C++ (header-only)

## Building

The external can be built via:

```sh
make kernel
```

This will:
1. Build the xeus library (both shared and static variants)
2. Compile the kernel external
3. Output `externals/kernel.mxo` (macOS) or `externals/kernel.mxe64` (Windows)

## Features

### Methods

- **bang** - Outputs a bang from the right outlet (status outlet)
- **eval [code...]** - Evaluates code through the Jupyter interpreter
  - Example: `eval print("hello")`
  - Arguments are concatenated and sent to the kernel
- **start** - Starts the Jupyter kernel (placeholder for full kernel initialization)
- **stop** - Stops the running kernel
- **info** - Outputs kernel information (implementation, version, language info)

### Attributes

- **name** (symbol) - Unique object identifier for the kernel instance
- **debug** (int, 0/1) - Enable/disable debug output to the Max console

### Outlets

- **Left outlet** - Kernel output and execution results
- **Right outlet** - Status messages and kernel lifecycle events

## Usage

### Basic Example

```
[kernel @debug 1]
|
[print]
```

Send messages to the kernel:
```
eval hello world        → Evaluates "hello world"
info                    → Outputs kernel metadata
start                   → Starts kernel
stop                    → Stops kernel
bang                    → Outputs status bang
```

### Jupyter Protocol Integration

The kernel implements the following Jupyter protocol handlers:

- **execute_request** - Executes code and returns results
- **complete_request** - Code completion (returns empty matches)
- **inspect_request** - Code introspection (returns not found)
- **is_complete_request** - Checks if code is complete
- **kernel_info_request** - Returns kernel metadata:
  - Implementation: `max_kernel`
  - Version: `0.1.0`
  - Language: `max` (Max/MSP)
  - Protocol version: `5.3`
- **shutdown_request** - Handles kernel shutdown

## Architecture

The implementation consists of:

1. **max_interpreter** - Custom interpreter class inheriting from `xeus::xinterpreter`
   - Implements all required virtual methods for Jupyter protocol
   - Routes execution requests to Max outlets
   - Provides kernel metadata and introspection

2. **t_kernel** - Max external structure
   - Manages kernel lifecycle and state
   - Handles Max message routing
   - Owns interpreter and kernel instances

3. **Message Flow**:
   ```
   Max Patch → kernel external → xeus interpreter → Jupyter protocol
                     ↓
                  Outlets → Max objects
   ```

## Current Limitations

- Full kernel startup with connection files is not yet implemented
- The `start` method is a placeholder (does not create connection files or bind to ports)
- Kernel execution currently routes through Max outlets rather than Jupyter clients
- No actual network communication with Jupyter frontends yet

## Future Enhancements

Potential improvements include:

- Full Jupyter kernel server implementation with ZMQ transport
- Connection file generation for Jupyter clients
- Support for rich media outputs (images, HTML, etc.)
- Interactive input handling
- Kernel interrupt and restart capabilities
- Integration with Jupyter Lab/Notebook interfaces

## Notes

- The external uses xeus v5.2.4 (binary version 13.2.0)
- Links to `libxeus-static.a` for static linking
- Requires C++17 or later
- Currently macOS only (arm64), Windows support pending

