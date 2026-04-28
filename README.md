# ⚠️ DO NOT USE — ALPHA STAGE — NO WARRANTIES

This repository contains **`tiny.h`**, a single-header, no-libc toolkit for x86-64 Linux.

It is **experimental, incomplete, and unstable**. It exists as a low-level playground, not a reliable foundation.

If you are looking for safety, portability, or maintainability, you are in the wrong place.

---

# Overview

`tiny.h` is a **minimal systems programming toolkit** built around:

* Direct Linux syscalls (no libc)
* Heavy macro usage
* Pointer arithmetic as a primary abstraction
* Branchless or low-branch patterns where possible
* Inline assembly for performance-critical operations

The design favors:

* Control over convenience
* Predictability over abstraction
* Explicitness over safety

---

# Platform Requirements

* Architecture: **x86-64 only**
* OS: **Linux only**
* Compiler: must support:

  * GNU-style inline assembly
  * SSE2 (`xmm` registers)

Compilation will fail outside these constraints.

---

# Design Philosophy

* **No libc**
  All functionality is built directly on syscalls and raw memory.

* **Data-oriented**
  Structures and memory layouts take priority over APIs.

* **Macros over functions**
  Many operations are implemented as macros for zero overhead.

* **Minimal runtime assumptions**
  No hidden initialization, no runtime dependencies.

* **Control flow as data**
  Floating-point comparisons and masks are used to reduce branching.

---

# File Structure

Everything lives in:

* `tiny.h` — the entire toolkit

There are no separate modules, no linking steps, and no dependency graph. Include it and deal with the consequences.

---

# Feature Breakdown

## 1. Syscalls

Defines raw syscall numbers and file descriptors:

* `SYS_READ`, `SYS_WRITE`, `SYS_EXIT`, `SYS_BRK`
* `STDIN`, `STDOUT`

Used directly via inline assembly.

---

## 2. Memory & Pointer Utilities

* Alignment helpers (e.g. `ALIGN_UP`)
* Pointer arithmetic macros
* Manual layout control

No safety checks are performed.

---

## 3. Stack Helpers

Utilities/macros for working with stack frames explicitly.

Useful if you enjoy thinking like a compiler.

---

## 4–6. Floating Point (f64 via XMM)

* Scalar arithmetic using SSE (`xmm`)
* Comparison operations producing flags
* Control flow patterns based on floating masks

This avoids typical branching in some cases.

---

## 7. Array Helpers

Basic operations over arrays of `f64`.

Minimal abstraction, mostly macro-driven.

---

## 8. Integer Bit Operations

Low-level bit manipulation utilities.

Expect no guardrails.

---

## 9. String & Memory Ops

Custom implementations of:

* Memory copy / set
* Basic string handling

No bounds checking, no encoding awareness.

---

## 10. Parsing & Conversion

* `atoi`
* `atof`
* DTOA (double → string)

Accuracy and edge cases are not guaranteed.

---

## 11. I/O

Built directly on syscalls:

* Print:

  * integers
  * floating point
  * strings
* Read input from stdin

No buffering, no formatting safety.

---

## 12. Memory Allocation (brk)

* Thin wrapper over `brk`
* Simple slab-style allocation

Not thread-safe. Not robust. Not forgiving.

---

## 13. `tiny_str_t`

A small-string-optimized structure (“German-style SSO”):

* Inline storage for short strings
* Heap fallback via allocator

Implementation details may change without notice.

---

## 14. Math & Constants

Basic numeric helpers and constants.

Not a full math library.

---

## 15. Interactive Helpers

Utilities like:

* `LEER_NUMERO`
* `LEER_LETRA`
* `print_label`

Naming reflects mixed-language origins. Consistency was clearly not the goal.

---

## 16. Entry Points

Dual-mode entry:

* `main`
* `_start`

Allows use with or without a standard runtime.

---

## 17. Functional Toolkit

Macro-based utilities:

* `MAP`
* `FILTER`
* `REDUCE`
* `FOR_EACH`
* `ZIP`
* `STR_FROM_BUF`

These operate at a very low level and assume correct usage.

---

## 18. Namespaces

* `UNSHARE`
* `BEGIN_ISOLATED`

Mechanisms to isolate scopes or behaviors.

---

## 19. Bump Allocator

* `BUMP_INIT`
* `BUMP_ALLOC`
* `BUMP_RESET`

Fast, linear allocation. No freeing. No tracking.

---

## 20. Terminal & Input Loop

* Raw terminal mode (`TERM_RAW`, `TERM_RESET`)
* Input polling (`POLL_KEY`, `READ_KEY`)

Suitable for simple interactive loops or games.

---

# Usage

There is no stable API.

Typical pattern:

```c
#include "tiny.h"

BEGIN
    // your code
END
```

Beyond that, you are expected to read the header and understand what each macro does before using it.

---

# Safety Notes

* No bounds checking
* No memory safety
* No error handling guarantees
* Undefined behavior is easy to trigger

If something breaks, that is expected behavior.

---

# Bug Reports

Bugs are likely everywhere.

Reporting them is useful, but:

* fixes are not guaranteed
* responses may be blunt
* regressions are possible

---

# Bug Bounty

Reward: vague appreciation and possible acknowledgment.

---

# Contributions

Contributions are accepted under strict conditions:

* Follow existing style exactly
* Do not introduce abstractions unless absolutely necessary
* Keep changes minimal and focused
* Expect rejection even if your code is correct

The project has a strong opinionated direction.

---

# License

This project is licensed under the GNU Affero General Public License v3.0 (AGPLv3).

See the LICENSE file for details.

---

# Final Warning

This is a low-level experimental toolkit.

Using it in production would be an impressive act of confidence in something that explicitly tells you not to trust it.
