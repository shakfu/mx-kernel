# Notes -- historical records

These files document investigations as they happened. They are kept because the
reasoning, the dead ends, and the rejected alternatives are worth having: two of
the problems here took real effort to diagnose and would be expensive to
rediscover.

They are **not** current documentation. File names, code excerpts, and status
claims reflect the repository as it was at the time. For how the object behaves
now, see [the object README](../projects/kernel/README.md).

| Note | Date | Subject |
|------|------|---------|
| [jupyter_console_issue.md](jupyter_console_issue.md) | 2025-11-06 | `'NoneType' object has no attribute 'get'` on connect. Traced from a Python traceback in jupyter-console to an uninitialised `nl::json` in xeus-zmq. Resolved; the fix is carried as a patch (see [patches/README.md](../../patches/README.md)). |
| [shutdown_compromise.md](shutdown_compromise.md) | 2025-11-07 | Max hanging on quit because xeus joins threads blocked in `poll_channels`. Originally resolved by deliberately leaking the kernel and context. Carries a 2026-08-18 correction: that leak was incomplete (it left a use-after-free) and has since been retired entirely by patching xeus-zmq to poll with a timeout. |
| [success.md](success.md) | 2025-11-07 | Status snapshot from when the round trip first worked end to end. |
| [testing.md](testing.md) | 2025-11-07 | Client-side setup and troubleshooting. Largely superseded by the walkthrough in the object README. |
