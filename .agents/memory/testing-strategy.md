---
name: Testing strategy
description: Why the project separates deterministic unit coverage from host-dependent low-level checks.
---

Pure `tiny.h` helpers should be tested through a libc-backed runner so failures can be reported reliably in restricted development environments. Raw syscall, namespace, terminal, and `brk` behavior belongs in opt-in integration checks because host policy or the absence of a TTY can terminate or invalidate those tests before they report results.

**Why:** The development sandbox can deny low-level syscalls such as namespace operations, and the imported repository does not currently include every input required by its low-level build target.

**How to apply:** Add deterministic assertions to the unit suite for pure helpers. Run the low-level suite only when its linker script, example source, kernel permissions, and terminal requirements are available.