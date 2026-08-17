# mx-kernel: Project Review

> **Status: addressed 2026-08-18.** Every item in the suggested order of work
> (Section 8) has been acted on. The findings below describe the code at
> `ed57795` and are kept as the record of why the changes were made -- they are
> no longer a description of the current tree.
>
> | # | Item | Status |
> |---|------|--------|
> | 1 | Teardown use-after-free | Fixed. Initially by leaking the impl with the kernel; now properly -- the server thread is joined and everything destroyed, with the notifier cleared under a mutex before the qelem is freed. |
> | 2 | `make clean` typo | Fixed, along with `.PHONY`, the undefined `section` macro, incremental `build`, and `connect NAME=`. |
> | 3 | Undistributable binary | Fixed. `script/bundle_dylibs.sh` bundles libzmq, libcrypto and libsodium into the bundle at `@rpath` and re-signs. Verified: no absolute Homebrew paths remain. |
> | 4 | One-shot lifecycle | Fixed. `eval` guard removed, kernel released on `stop`, interpreter constructed per `start`, shutdown no longer latches `alive` off. |
> | 5 | Uncorrelated results | Fixed. Stale replies are drained per cell and stamped with the executing cell's counter; mismatches are discarded. |
> | 6 | World-readable connection file | Fixed. Created `0600` via `open(2)` up front, so the key is never briefly world-readable. |
> | 7 | Stale documentation | Rewritten: object README, root README, dated notes, `how-to-test.md` folded in. |
> | 8 | Untested interpreter | Fixed. `qelem_set` replaced by an injected notifier; `interpreter.cpp` is now under test, plus real-kernel shutdown integration tests. 6 tests/109 assertions -> 37/231. |
> | 9 | Unmarked vendored patch | Fixed. Marked in-tree, extracted to `patches/`, with an idempotent `apply.sh`. Not yet reported upstream. |
> | 10 | No CI | Added `.github/workflows/test.yml`, including a check that the external stays self-contained. |
> | 11 | No Max-initiated output | Done. `print` is delivered during a cell and, via the timed-poll patch against xeus-zmq, within ~50ms while the kernel is idle. |
>
> **Item 11 was completed subsequently** by patching xeus-zmq to poll with a
> timeout and invoke a callback on each idle tick
> (`patches/xeus-zmq-0003-*`). The same patch fixes the shutdown hang at its
> root, so the deliberate leak of the kernel, context and impl described in
> finding 3.1 is gone entirely -- the thread is joined and everything is
> destroyed normally, with a deadline-bounded fallback to the old behaviour.
> `tests/test_server_shutdown.cpp` starts a real kernel and asserts that stop,
> join and destruction all complete; reverting the poll to `-1` makes those
> tests hang, which their watchdog reports as a failure.
>
> **Verified in Max on 2026-08-18** (Max 9, arm64), not only by the test suite:
> start and port binding; connection file written `0600`; connecting with
> `jupyter console` without the `parent_header` exception; the round trip
> (`Out[n]` matching its input across consecutive cells); `execute_input`
> published once; streaming followed by a result; `MaxTimeout` reported as an
> error; the `stop` -> `start` restart cycle with a clean join (`kernel: stopped`,
> no fallback warning); and object deletion followed by quitting Max with no
> hang. That last one is the force-quit bug the shutdown investigation was about.
>
> Additional defects found while fixing the above, none of which were in the
> original review:
>
> - `execute_input` was published twice per cell (xeus publishes it, and the
>   interpreter published it again).
> - `dict` shipped Max dictionary text under the `application/json` mime type.
> - `xserver_zmq_impl::m_request_stop` was a plain `bool` read and written from
>   two threads -- latent, but load-bearing once the poll loop reads it per tick.
> - Stream output carried no trailing newline. Jupyter concatenates stream text
>   verbatim, so consecutive `print` messages ran together and the next `Out[n]`
>   collided with them. Found by manual testing in Max, not by the suite.
> - Every `object_post`/`object_warn`/`object_error` double-prefixed its message
>   (`kernel: kernel: ...`), since Max already prefixes the object name.
> - The `shutdown` status was emitted on a locally initiated `stop` as well as a
>   client request, because `xkernel::stop()` calls `shutdown_request()` itself.
>
> All fixed. The last three were found by running the object in Max, not by the
> suite -- which is what should be expected of `external.cpp`, the one
> translation unit the tests cannot reach.

**Date:** 2026-08-17
**Reviewed at:** `ed57795` (clean tree)
**Scope:** first-party C++ (`source/projects/kernel/*.cpp|h`, 1255 lines including tests), build system, tests, packaging, documentation. Vendored `xeus`, `xeus-zmq`, `nlohmann/json`, `doctest` were reviewed only where this project patches or configures them.

> This file replaces an earlier review written against `dc5740d`, when the external was a single 648-line `kernel.cpp`. That version is still in git history. Roughly half its findings have since been addressed by the split into `external.cpp` / `interpreter.cpp` / `connection.cpp` and the move to `arc4random_buf`; the rest are re-derived below against the current code.

---

## 1. Summary

mx-kernel embeds a Jupyter kernel inside a Max/MSP external so a Jupyter client can drive a Max patch over ZMQ. The protocol layer works: the kernel binds five channels, writes a spec-compliant connection file, and a real round trip (Jupyter cell -> Max outlet -> `result` message -> Jupyter output) is implemented end to end. The module split is good, the Max-free modules are genuinely testable, and the two hard bugs the project hit (xeus-zmq `parent_header` null, shutdown join deadlock) were diagnosed properly rather than papered over.

What holds it back is not the protocol work. Three things stand between this and something usable by anyone other than its author:

1. **Object teardown has a use-after-free window** (Section 3.1). The design deliberately leaks the `xkernel`, but then deletes the `t_kernel_impl` that the leaked kernel's interpreter still points at, and frees the qelem that same interpreter still calls. This is the most serious finding.
2. **Lifecycle is one-shot.** After `start`, `eval` is permanently broken; after `stop`, `start` cannot succeed; after a Jupyter shutdown request, every subsequent cell times out. Each is a small bug, but together they mean the object works exactly once per instantiation (Section 3.2).
3. **The binary is not distributable.** `kernel.mxo` links `/opt/homebrew/opt/...` absolute paths for libzmq and libcrypto; it will fail to load on any machine without Homebrew at that exact prefix (Section 6.1).

The result-passing design also has a correctness gap independent of any bug: results are not correlated with the requests that asked for them (Section 3.3).

Severity labels below: **High** = memory-unsafe or silently wrong output; **Medium** = feature does not work as documented; **Low** = hygiene. Findings marked *(by inspection)* were derived from reading the code, not reproduced under a debugger.

---

## 2. What works well

- **Module boundary.** `connection.cpp` and `message_queue.h` have no Max SDK dependency and are unit-tested without it (`tests/CMakeLists.txt` links only `connection.cpp`). That is the right seam, and it is enforced by the build rather than by convention.
- **Thread hand-off.** Kernel thread -> main thread goes through `ThreadSafeQueue` plus `qelem_set`, which is the correct Max idiom; `outlet_anything` is never called off the main thread. Main thread -> kernel thread goes through a second queue. No shared mutable state beyond the queues and one atomic.
- **Key generation.** `generate_random_key()` (`connection.cpp:20`) uses `arc4random_buf` on Apple platforms. 256 bits of CSPRNG output for an HMAC key is correct, and the earlier `rand()`-based version is gone.
- **Failure reporting.** Every fallible path reports through `object_error` on the offending object rather than `post()`, so errors are attributable in the Max console.
- **Investigation notes.** `source/notes/jupyter_console_issue.md` and `shutdown_compromise.md` record the full diagnostic path including rejected alternatives. This is unusually good for a personal project and is the reason the shutdown compromise reads as a decision rather than an accident.

---

## 3. Correctness

### 3.1 High: use-after-free on object deletion *(by inspection)*

`kernel_free` (`external.cpp:149-190`) does, in order: stop the kernel, detach the kernel thread, `qelem_free(x->outlet_qelem)`, `impl->kernel.release()`, `impl->alive.store(false)`, `delete impl`.

The `release()` call is deliberate and documented -- destroying the `xkernel` would join threads blocked in `poll_channels` and hang Max. But the released `xkernel` owns the `max_interpreter` (ownership was transferred at `external.cpp:276`), and that interpreter holds `m_impl`, a raw pointer to the object being deleted on the next line. The detached kernel thread is still alive and still serving the ZMQ sockets.

Concretely, if a Jupyter client sends an `execute_request` while or after the Max object is being freed:

- `execute_request_impl` writes to `m_impl->outlet_queue` (`interpreter.cpp:39`) -- freed memory.
- It then calls `qelem_set(m_impl->qelem)` (`interpreter.cpp:43`) -- a qelem freed at `external.cpp:170`.
- Its poll loop reads `m_impl->alive` and `m_impl->result_queue` (`interpreter.cpp:56-57`) -- freed memory, and the loop can run for up to `timeout` seconds after the free.

`impl->alive.store(false)` immediately before `delete impl` does not close this: the store and the reader's `load()` race against the delete, not against each other, and even a correctly observed `false` only stops the poll loop -- steps 1 and 2 have already happened by then.

The window is not narrow. Deleting a `[kernel]` object in a patch while a Jupyter console sits at a prompt is a normal user action, and the connection file is removed at `external.cpp:177` but the sockets stay bound, so a client that is already connected keeps working.

Two ways out, in increasing order of effort:

- **Leak the impl too.** If the kernel is deliberately leaked, everything it transitively points at must be leaked with it. Replace `delete impl` with `impl->alive.store(false)` and a release of ownership (e.g. hold the impl in a `unique_ptr` on `t_kernel` and `release()` it), and stop calling `qelem_free` when a kernel was ever started. Consistent with the existing compromise, costs a few KB per freed object, and closes the hole completely. This is the minimal correct fix.
- **Break the back-pointer.** Give the interpreter a `shared_ptr<t_kernel_impl>` (or a `weak_ptr` it locks per call), so the impl outlives whichever side dies last and the interpreter can detect that the Max object is gone. More code, but it also removes the need to leak the qelem and makes the ownership story explainable in one sentence.

Either way, `qelem_free` and the impl must not be freed while a detached thread can still reach them.

### 3.2 Medium: the object only works once

Three independent defects, all in the same area:

**`eval` is dead after `start`.** `kernel_start` moves the interpreter into the kernel (`external.cpp:276`), leaving `impl->interpreter` null. `kernel_eval` guards on exactly that pointer (`external.cpp:383`) and errors with "interpreter not initialized". So `eval` works only before the kernel is started, which is the opposite of what `source/projects/kernel/README.md` documents. The guard is also spurious -- `kernel_eval` never touches the interpreter; it concatenates atoms and calls `outlet_anything`. Deleting the guard fixes the symptom; see 3.4 for the deeper issue that `eval` has nowhere to send anything.

**`start` after `stop` cannot succeed.** `kernel_stop` (`external.cpp:336-370`) calls `impl->kernel->stop()` and detaches the thread but leaves `impl->kernel` non-null, so the next `start` hits the `if (impl->kernel)` guard at line 250 and reports "already running". Even if that guard were cleared, `impl->interpreter` is null from the first start, so line 255 would then reject the restart. A working restart needs both: release `impl->kernel` in `kernel_stop`, and construct a fresh `max_interpreter` at the top of `kernel_start` rather than at object creation.

**Jupyter shutdown permanently disarms the object.** `shutdown_request_impl` (`interpreter.cpp:153`) sets `alive = false` and nothing ever sets it back. `alive` is also the poll-loop condition at `interpreter.cpp:56`, so after any client sends a shutdown request, every later cell skips the wait entirely and falls through to the "no response" branch -- while the Max side still reports the kernel as running. Use a separate flag for shutdown, or reset `alive` on `start`.

### 3.3 Medium: results are not correlated with requests

`ResultMessage` carries an `execution_counter` field (`message_queue.h:30`) that is never written and never read. `execute_request_impl` pops whatever happens to be at the head of `result_queue` (`interpreter.cpp:57`). Consequences:

- A `result` message sent from Max at any time before a cell runs is queued and consumed by the *next* unrelated cell.
- If a patch answers one cell with two `result` messages, the second stays queued and is delivered as the answer to the following cell. Every subsequent cell is then off by one, permanently, with no error anywhere.
- A late reply, arriving after the timeout, poisons the next cell the same way.

None of this surfaces as a failure -- it surfaces as wrong output attributed to the wrong input, which is the worst failure mode for a notebook.

Minimum fix: drain `result_queue` at the top of `execute_request_impl` so each cell starts clean. Better: stamp the counter into the `OutletMessage` (it already carries one), require patches to echo it back, and discard non-matching results. That costs one extra atom in the outlet message and makes the round trip verifiable.

Related, same function: a `result` whose `stream_name` is set publishes a stream and then sets `got_result = true` and breaks (`interpreter.cpp:72-86`), so a cell can produce either streamed output or a result, never streamed output followed by a result. That is a real restriction for the "Max prints progress, then answers" case.

### 3.4 Medium: no Max-initiated path to Jupyter

Every message to a client must originate inside an `execute_request`. `kernel_eval` (`external.cpp:381`) sends its text to the *right* outlet -- documented as the status outlet -- and never to Jupyter at all, despite `source/projects/kernel/README.md` describing it as "Evaluates code through the Jupyter interpreter". Likewise `kernel_dict` pushes onto `result_queue`, which is only ever drained inside an execute cycle, so a `dict` sent while no cell is running sits in the queue and later corrupts an unrelated cell per 3.3.

This is the main architectural gap. A kernel that can only answer, never speak, cannot support the use cases the notes list (logging analysis results from Max, streaming patch state). xeus does support unsolicited IOPub publishing; it needs a channel from the main thread to the kernel thread that is not the request/response queue.

### 3.5 Medium: `dict` output is labelled JSON but is not JSON

`kernel_dict` (`external.cpp:507-559`) serialises via `dictobj_dictionarytoatoms`, concatenates the atoms into a string, and tags it `mime_type = "application/json"`. The Max dictionary text format is not JSON. The comment at lines 547-549 acknowledges this ("may not be direct JSON, but we attempt it") but no parse is actually attempted -- the raw text is shipped under the JSON mime type unconditionally. Clients that pretty-print `application/json` will show a parse error or nothing.

Either walk the dictionary with `dictionary_getkeys` plus the typed getters and build an `nl::json` properly, or drop the mime type and send `text/plain`. Shipping non-JSON under a JSON label is the one option that should be off the table.

### 3.6 Low: blocking wait holds the shell channel

`execute_request_impl` busy-waits up to `timeout` seconds in 10 ms sleeps (`interpreter.cpp:53-89`). During that window the xeus shell thread is blocked, so no other shell request is served -- a client asking for completion or kernel info during a slow cell sees a hang. With the default `@timeout 30` and a patch that never answers, that is 30 seconds of unresponsiveness per cell.

A `std::condition_variable` on the result queue removes the polling (and cuts latency from ~10 ms to ~0), but not the head-of-line blocking, which is inherent to answering synchronously. If long-running Max work is expected, the cell should reply immediately and stream results later via the mechanism from 3.4.

Also here: `timeout` is not validated. `@timeout 0` or a negative value puts the deadline in the past, so every cell falls straight through to "no response". Clamp to a sane minimum and document that behaviour.

### 3.7 Low: timeout is reported as success

On timeout the interpreter publishes "Sent to Max (no response)" as *stdout* and then replies `status: "ok"` (`interpreter.cpp:92-103`). Programmatic clients see a successful execution with no result. A timeout is an error condition and should reply `status: "error"` with a distinguishable `ename`.

### 3.8 Low: non-atomic timeout, dropped atom types

- `impl->timeout` is a plain `long` written by the main thread at `external.cpp:262` and read by the kernel thread at `interpreter.cpp:52`. In practice the write only happens at start, but it is still a technically racy pair; `std::atomic<long>` costs nothing here.
- The four atom-to-string loops (`external.cpp:389-398`, `470-479`, `484-493`, `533-542`) all silently drop anything that is not `A_SYM`/`A_LONG`/`A_FLOAT`. The same loop is duplicated four times and should be one `atoms_to_string(argc, argv)` helper that at least warns on unhandled types.

---

## 4. Security

Everything binds to `127.0.0.1`, so the exposure is local-user only. Within that scope:

**Medium: the connection file is world-readable.** `write_connection_file` (`connection.cpp:93`) writes via `std::ofstream`, giving 0644 under a typical umask. The file contains the HMAC key and the live ports. Any local user or process can read it and connect to the kernel, and connecting means sending arbitrary messages into the user's Max patch. Jupyter itself writes connection files 0600 for exactly this reason. Fix with `chmod`/`std::filesystem::permissions` to `owner_read | owner_write` immediately after creation -- ideally before writing the key, by creating the file with restrictive permissions in the first place.

**Low: `std::system` with an interpolated `HOME`.** `kernel_install` (`external.cpp:574-575`) builds `mkdir -p "$HOME/..."` as a shell string. A `HOME` containing a double quote escapes the quoting and executes attacker-chosen text. The threat model is thin -- you generally control your own `HOME` -- but there is no reason to spawn a shell at all: `std::filesystem::create_directories` does the job with no quoting question. The same applies to the `std::system("mkdir -p ...")` in `tests/test_connection.cpp:67`.

**Low: kernel name is unsanitised in a path.** `kernel_name` comes straight from the `@name` symbol and is interpolated into the connection file path (`connection.cpp:72`). A name containing `/` or `..` writes outside the runtime directory. Restrict to `[A-Za-z0-9._-]` and reject the rest.

**Low: non-Apple RNG fallback.** `connection.cpp:27-30` fills the key from `std::random_device` one byte at a time. That is fine on Linux and macOS, but `std::random_device` is permitted to be deterministic, and historically was on MinGW. Since Windows is a stated future target, this fallback becomes a real weakness the moment the port happens. Prefer `BCryptGenRandom` on Windows and `getrandom`/`/dev/urandom` on Linux.

---

## 5. Tests

`make test` passes: 6 cases, 109 assertions, doctest 2.4.11. Verified during this review.

**Coverage is the problem.** The test target compiles `connection.cpp` and the `message_queue.h` templates -- roughly 180 lines of the 950 first-party non-test lines. `interpreter.cpp`, which contains all the protocol semantics and every bug in Sections 3.2-3.7, has no tests at all. `external.cpp` cannot be tested without Max, which is expected and fine.

The interpreter is *nearly* testable already: its only Max dependency is the `extern "C" void qelem_set(void*)` declaration at `interpreter.cpp:10-12`. Replace that with a `std::function<void()> notify` stored on `t_kernel_impl` and set by `external.cpp`, and `interpreter.cpp` links into the test binary with a no-op notifier. That single change unlocks tests for the cases that matter most:

- a result arriving before the deadline produces `execution_result` and `status: ok`;
- no result within the deadline produces the timeout path (and, after fixing 3.7, an error status);
- an `error`-prefixed result produces `publish_execution_error` and `status: error`;
- a stale result left in the queue does not leak into the next execution (this is 3.3 -- write the test first, watch it fail);
- `shutdown_request` followed by `start` leaves the kernel able to execute (3.2).

Smaller notes:

- The `extern "C"` declaration is fragile on its own terms. It duplicates a Max SDK prototype with the argument typed as `void*` instead of `t_qelem*`; a signature change upstream would link cleanly and misbehave at runtime. The `std::function` approach removes the duplicate declaration entirely.
- `tests/test_connection.cpp` mutates the real `JUPYTER_RUNTIME_DIR` and writes into a fixed `/tmp/mx-kernel-test`. Two concurrent test runs collide, and the `rmdir` at the end silently fails if anything else is in the directory. Minor, but a per-run unique directory is two lines.
- There is no CI. A GitHub Actions job running `make test` on macOS would catch build breakage in the modules that do not need Max, which is most of the ones under active change.

---

## 6. Build and packaging

### 6.1 High: the built external is not distributable

`otool -L externals/kernel.mxo/Contents/MacOS/kernel` shows:

```
/opt/homebrew/opt/zeromq/lib/libzmq.5.dylib
/opt/homebrew/opt/openssl@3/lib/libcrypto.3.dylib
```

Absolute Homebrew paths baked into the binary. On a machine without Homebrew -- or an Intel Mac, where the prefix is `/usr/local` -- the external fails to load, and Max's failure mode for a missing dylib is an unhelpful "object not found" in the console. Every non-author user hits this.

Options, best first:

- **Bundle and relocate.** Copy both dylibs into `kernel.mxo/Contents/Frameworks/` and rewrite the install names to `@loader_path/../Frameworks/...` with `install_name_tool`, as a post-build step. This is the standard approach for Max externals and keeps the build fast.
- **Link statically.** Set `XEUS_ZMQ_STATIC_DEPENDENCIES ON` (currently forced `OFF` at `source/projects/kernel/CMakeLists.txt`) and provide static libzmq and libcrypto. Simplest to distribute, but pulls OpenSSL's licensing into the binary and needs static builds of both libraries available.

Either way this needs to be part of `make build`, not a manual step, or it will be forgotten.

The same section also builds a universal binary question: `CMakeLists.txt:46-54` sets `CMAKE_OSX_ARCHITECTURES` to the host processor, and `C74_BUILD_FAT` is off. Max 8 users on Intel therefore need a separate build. Worth deciding explicitly rather than by default.

### 6.2 Medium: vendored dependencies with an undocumented local patch

`xeus`, `xeus-zmq`, `nlohmann/json`, and `doctest` are copied into the repo as tracked files -- 1395 of them -- rather than being submodules. Only `max-sdk-base` is a submodule, even though the Makefile's `update-submodules` target and the README both imply the dependency story is submodule-based.

The sharp edge is that `thirdparty/xeus-zmq/src/server/xpublisher.cpp:44-45` carries a local two-line fix (the `parent_header` / `metadata` null fix), and nothing in the vendored tree records that it is patched. The next person to refresh xeus-zmq -- including the author in six months -- deletes the fix and gets a bug whose symptom (`'NoneType' object has no attribute 'get'` in an unrelated Python file) took a full investigation to diagnose the first time. `source/notes/jupyter_console_issue.md` documents it, but nothing links the note to the file.

Recommended, cheapest first:

1. Add a comment above the patch naming it as a local divergence and pointing at the note. Two minutes, prevents the worst outcome.
2. Move the diff into `patches/xeus-zmq-parent-header.patch` and apply it from CMake or the Makefile, so the vendored tree stays pristine and the divergence is one file you can read.
3. Report it upstream. The fix is small, obviously correct, and benefits every xeus-zmq kernel; carrying it locally forever is the worse end state.

Also worth reconsidering: vendoring `nlohmann/json` as full source when the build already finds Homebrew's `nlohmann_json` 3.12.0 for xeus-zmq means two copies of the same library in one build.

### 6.3 Low: Makefile defects

- **`make clean` does not clean.** `clean:` runs `rm -rf exterals build` -- "externals" is misspelled (`Makefile:19`). Stale externals survive every clean, so a build that silently fails can leave you testing yesterday's binary. This is worth fixing today.
- **`.phony` should be `.PHONY`.** Make's special target is uppercase; the lowercase form declares nothing (`Makefile:6`). The targets currently work by accident, because no files named `test`, `clean`, or `setup` exist -- but `build/` *does* exist as a directory, and `build` only rebuilds because its `clean` prerequisite is itself treated as always-out-of-date. Fragile.
- **`$(call section,...)` expands to nothing.** `section` is never defined, so the "setup complete" and "symlink" banners in `setup`, `update-submodules`, and `link` print nothing (`Makefile:22`, `25`, `43`).
- **`build` depends on `clean`.** Every build is a full rebuild of xeus and xeus-zmq. ccache softens this, but an incremental target would help the edit-compile loop more.
- **`make test` configures the entire project**, including the Max external and both xeus trees, to produce a test binary that links only `connection.cpp`. A test-only CMake entry point would turn a multi-minute first run into seconds.
- **`connect` hardcodes `kernel-testkernel.json`** (`Makefile:33`), so it only works for an object named `testkernel`. Take the name as a variable with that default.
- **`install-kernelspec` installs a non-functional kernelspec.** Its `argv` is `["echo", "Connect via Max"]` (`Makefile:38`, mirrored in `kernel_install` at `external.cpp:582`). Selecting "Max/MSP" in the Jupyter Lab launcher runs `echo`, which exits without a connection file, and the client hangs or errors. The spec is only useful for making the kernel *visible*; as installed it is a trap. Either document it as discovery-only in the `display_name` (e.g. "Max/MSP (connect to running patch)") or drop the feature.

### 6.4 Low: licensing tension

The project is GPL-3 (`LICENSE`), but the external links the proprietary Max SDK, whose license is not GPL-compatible. GPL-3's requirement that all linked components be distributable under compatible terms is at least arguably violated by distributing a built `.mxo`. Most Max externals ship under MIT or BSD for precisely this reason, and the vendored xeus stack is already BSD-3. Worth a deliberate decision before the first release; if GPL-3 is intended, a Max SDK linking exception should be stated explicitly.

---

## 7. Documentation

The docs are the weakest part of the repo relative to the effort already spent on them -- there is plenty of writing, but a large fraction of it is now false.

**`source/projects/kernel/README.md` is substantially wrong.** Its "Current Limitations" section states that `start` is a placeholder that does not create connection files or bind ports, and that there is "no actual network communication with Jupyter frontends yet". All three claims were true two commits ago and are false now. It also omits `result`, `dict`, and `install` from the methods list, and `timeout` from the attributes list -- that is, it documents none of the messages a user needs for the round trip that is the project's whole point. A reader following this file concludes the project does not work.

**The notes are stale in the same way.** `source/notes/shutdown_compromise.md` quotes `kernel.cpp` at length; that file no longer exists. `source/notes/success.md` references `kernel.cpp - Main implementation (+350 lines)`, `TESTING.md`, `SUCCESS.md`, and `CLAUDE.md`, none of which are at those paths, and claims "No memory leaks (RAII)" as an achieved success metric while the design deliberately leaks the kernel and context. `success.md` also still lists the jupyter-console exception under "Known Issues" though `testing.md` and `jupyter_console_issue.md` both record it as fixed. These files are valuable as *investigation records* -- they should be dated and framed as history, not as current status, and the code paths they quote should be updated or removed.

**The root `README.md` is five lines** and does not mention how to build, what the object's messages are, or what the message protocol looks like. `how-to-test.md` is the only document that actually explains the round trip, and it reads like a pasted chat transcript (it opens mid-answer with a stray prompt character).

**Nothing specifies the wire contract between the external and the patch.** That contract is small and is the single most important thing to write down:

```
kernel -> patch (left outlet):   code execute <text>
patch -> kernel (inlet):         result <text...>
                                 result error <ename> <evalue...>
                                 dict <dict-name>
```

Concrete plan, in priority order:

1. Rewrite `source/projects/kernel/README.md` against the current code: all nine methods, all three attributes, the wire contract above, and the actual limitations (no Max-initiated output, one result per cell, timeout semantics).
2. Expand the root `README.md` to build instructions, the `make` targets, and a pointer to the object docs.
3. Move `source/notes/*` under a `history/` heading with dates, and strip the stale file references.
4. Fold `how-to-test.md` into the object README as a "Round trip walkthrough" section, edited into prose.

The help patch (`help/kernel.maxhelp`) already demonstrates the round trip with `route code` / `prepend result`, which is the right thing to show -- it is ahead of the prose docs.

---

## 8. Suggested order of work

**Before anything else** (memory safety and a broken build tool):

1. Close the teardown use-after-free -- leak the impl and qelem alongside the kernel, or move to shared ownership (3.1).
2. Fix the `exterals` typo in `make clean` (6.3).

**To make it usable by a second person:**

3. Bundle or statically link libzmq and libcrypto so the external loads off the author's machine (6.1).
4. Fix the lifecycle: drop the spurious `eval` guard, release the kernel on `stop`, construct the interpreter in `start`, stop letting shutdown latch `alive` off (3.2).
5. Drain stale results at the start of each execution, then correlate by execution counter (3.3).
6. `chmod 0600` the connection file (Section 4).
7. Rewrite the object README against the current code, including the wire contract (Section 7).

**To keep it working:**

8. Replace `qelem_set` with an injected notifier and put `interpreter.cpp` under test (Section 5).
9. Mark the xeus-zmq patch in-tree, extract it to a patch file, and report it upstream (6.2).
10. Add a CI job running `make test`.

**Then the design gap:**

11. Build a Max-initiated output path so the kernel can stream to a client outside an execute cycle (3.4). This is the item that decides whether the project stays a request/response bridge or becomes a live-coding surface -- worth designing before more features land on top of the current model.
