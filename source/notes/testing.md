# Testing the mx-kernel Jupyter Kernel

This guide explains how to test the Max/MSP Jupyter kernel with Jupyter clients.

## Prerequisites

- Max/MSP 8 or 9 installed
- Jupyter installed (via `uv add jupyter jupyterlab`)
- The kernel external built successfully (`make build`)

## Quick Start

### 1. Start Max/MSP and Create a Test Patch

1. Open Max/MSP
2. Create a new patch
3. Create a `[kernel @debug 1 @name testkernel]` object
4. Connect `[print]` objects to both outlets to see output

### 2. Start the Kernel

In Max, send the following message to the kernel object:

```
start
```

You should see output like:
```
kernel: connection file: /Users/username/.local/share/jupyter/runtime/kernel-testkernel.json
kernel: shell_port=xxxxx control_port=xxxxx iopub_port=xxxxx stdin_port=xxxxx hb_port=xxxxx
kernel: started successfully
kernel: connect with: jupyter console --existing /path/to/kernel-testkernel.json
```

### 3. Connect with Jupyter Console

Copy the connection file path from the Max console and connect:

```bash
uv run jupyter console --existing /Users/username/.local/share/jupyter/runtime/kernel-testkernel.json
```

Or use the shorthand if the filename is displayed:

```bash
uv run jupyter console --existing kernel-testkernel.json
```

### 4. Test the Connection

In the Jupyter console, try executing some code:

```python
In [1]: print("hello from jupyter")
```

This will be sent to the Max kernel, which will:
- Execute the code through the interpreter
- Send output back to Jupyter
- Also output to Max outlets

### 5. Stop the Kernel

In Max, send:

```
stop
```

This will gracefully shut down the kernel and clean up the connection file.

## Testing Methods

### Method 1: Jupyter Console (Recommended for Testing)

Jupyter Console provides a terminal-based interface:

```bash
uv run jupyter console --existing <connection-file>
```

Advantages:
- Lightweight
- Easy to see immediate feedback
- Good for debugging

### Method 2: Jupyter Lab (Recommended)

For a full IDE experience without console exceptions:

```bash
# Start Jupyter Lab
uv run jupyter lab

# In the Launcher, select "Console"
# Or use File → New → Console
# Then connect to the existing kernel via the connection file
```

To connect to an existing kernel in Jupyter Lab:
1. Open Jupyter Lab
2. File → New Console for Existing Kernel
3. Browse to the connection file shown in Max console
4. Or paste the connection info

### Method 3: Jupyter Notebook

```bash
uv run jupyter notebook

# Create new notebook and select the kernel
```

## Understanding the Kernel Behavior

### What the Kernel Does

1. **Code Execution**: When you execute code in Jupyter:
   - Code is sent to the kernel via ZMQ
   - The `max_interpreter::execute_request_impl()` method processes it
   - Output is sent to both Jupyter (via publish) and Max outlets

2. **Kernel Info**: Request kernel information:
   ```python
   # The kernel identifies as "max_kernel" version 0.1.0
   # Language: max (Max/MSP)
   ```

3. **Code Completion**: The kernel provides basic completion (currently returns empty matches)

### Current Features

- [x] Full Jupyter protocol implementation
- [x] ZMQ communication
- [x] Connection file generation
- [x] Multi-client support
- [x] Kernel info/inspection
- [x] Execute requests
- [x] Graceful shutdown

### Limitations

- Code completion returns empty (can be extended)
- Code inspection returns not found (can be extended)
- Execution currently echoes to Max but could be extended to actually evaluate Max code

## Known Issues - RESOLVED! [x]

### ~~Jupyter Console Parent Header Exception~~ - FIXED

**Previous Issue:** jupyter-console would crash with `'NoneType' object has no attribute 'get'` when connecting to the kernel.

**Root Cause:** xeus-zmq was sending JSON `null` for `parent_header` instead of empty object `{}` in startup messages. This was because `nl::json` default-constructs to `null` instead of `{}`.

**Fix:** Patched `xeus-zmq/src/server/xpublisher.cpp` to explicitly initialize `parent_header` and `metadata` as empty objects:

```cpp
data.m_parent_header = nl::json::object();
data.m_metadata = nl::json::object();
```

**Status:** [x] Fixed and working! Jupyter console now connects without any exceptions.

## Troubleshooting

### Connection File Not Found

If Jupyter can't find the connection file, ensure:

1. The runtime directory exists:
   ```bash
   mkdir -p ~/.local/share/jupyter/runtime
   ```

2. Check the `JUPYTER_RUNTIME_DIR` environment variable:
   ```bash
   echo $JUPYTER_RUNTIME_DIR
   ```

3. Use the full path provided by Max console

### Kernel Won't Start

Check Max console for errors. Common issues:

- **Port already in use**: Stop any other kernels and try again
- **Interpreter not initialized**: The kernel object failed to create the interpreter
- **ZMQ context error**: Check that libzmq is properly installed

Debug mode helps: `@debug 1`

### Kernel Disconnects

If the kernel disconnects unexpectedly:

1. Check Max console for errors
2. Ensure the kernel thread is running
3. Restart Max and try again

## Advanced Usage

### Multiple Kernels

You can run multiple kernel instances:

```
[kernel @name kernel1 @debug 1]
[kernel @name kernel2 @debug 1]
```

Each will create its own connection file and can be connected to independently.

### Programmatic Control

Send messages to control the kernel:

- `start` - Start the kernel server
- `stop` - Stop the kernel server
- `info` - Display kernel metadata
- `eval <code>` - Evaluate code (currently echoes to outlet)
- `bang` - Output status bang

### Connection File Format

The kernel creates standard Jupyter connection files:

```json
{
  "transport": "tcp",
  "ip": "127.0.0.1",
  "control_port": 54321,
  "shell_port": 54322,
  "stdin_port": 54323,
  "iopub_port": 54324,
  "hb_port": 54325,
  "signature_scheme": "hmac-sha256",
  "key": "..."
}
```

## Next Steps

### Extending the Interpreter

The `max_interpreter` class can be extended to:

1. **Execute actual Max code**: Route code to Max for evaluation
2. **Provide completion**: Analyze Max objects and provide completion
3. **Inspect objects**: Return documentation for Max objects
4. **Rich output**: Send images, audio, or other media back to Jupyter

### Integration Ideas

- Send messages to Max objects from Jupyter
- Query Max patch state from Jupyter
- Control DSP processing from notebooks
- Log data analysis results to Jupyter from Max
- Create interactive visualizations of Max data

## Example Workflow

```python
# In Jupyter Console connected to Max kernel

In [1]: # This executes in the Max kernel
        print("Testing kernel")

In [2]: # You can send any code
        for i in range(5):
            print(f"Count: {i}")

In [3]: # Check kernel info
        %who
```

In Max, you'll see these messages appear in the console and route through the outlets.
