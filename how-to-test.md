❯ How do test it from the client side?

The flow has two sides: Max (the kernel host) and Jupyter (the client). Here's the manual test procedure:

1. Start the kernel in Max

Create a patch with:

```text
[kernel @name test @debug 1 @timeout 30]
```

Send it start. You should see in the Max console:

```sh
kernel: connection file: ~/.local/share/jupyter/runtime/kernel-test.json
kernel: shell=XXXXX control=XXXXX iopub=XXXXX ...
kernel: started successfully
```

2. Connect from Jupyter

```sh
make connect
```

Or manually:

```sh
uv run jupyter console --existing
~/.local/share/jupyter/runtime/kernel-test.json
```

You should get a In [1]: prompt.

3. Test Jupyter -> Max (code execution)

Type anything in the Jupyter console:

```python
In [1]: hello world
```

In Max, the left outlet should emit:

```text
code execute "hello world"
```

Since no result message is sent back within 30s, Jupyter will display:

```text
Sent to Max (no response): hello world
```

4. Test Max -> Jupyter (result return)

To close the loop, you need to wire Max to send results back. Connect the left outlet to a handler that sends a result message back to the kernel inlet:

```text
  [kernel @name test @debug 1]
   |                    ^
   | [prepend result]   |
   +---->[route code]--->+
```


More concretely: route the code selector from the left outlet, extract the
text, and send result <your reply> back to the kernel's inlet. For example, to echo the input:

```python
In [1]: hello
Out[1]: 'hello'
```

Or to send an error:

```text
result error MaxError "something went wrong"
```


5. Test dict serialization

Create a dict in Max ([dict mydict]), populate it, then send:

```text
dict mydict
```

to the kernel inlet. The dict contents appear as JSON in the Jupyter cell output.

6. Test kernelspec install Send install to the kernel inlet, then verify:

```sh
jupyter kernelspec list
```

should show mx-kernel.

7. Shutdown

Send stop to the kernel. The Jupyter console will disconnect.

---

The critical thing to test is step 4 -- the round-trip. Without wiring a
result message back, every Jupyter cell will timeout after 30s with the "no
response" fallback.

The execution model is:

```text
Jupyter sends code -> kernel
pushes it out the Max outlet -> your patch processes it -> your patch sends
result <text> back -> Jupyter gets the reply.
```

