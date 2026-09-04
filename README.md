# tiny.h

> ⚠️ **ALPHA. NO WARRANTY.**
>
> You break it, you own the pieces.

Single-header, no-libc runtime toolkit for **x86-64 Linux** written in C.

`tiny.h` provides direct Linux syscalls, manual memory management, SSE/x87-backed floating-point operations, low-level string and memory primitives, terminal I/O, namespace isolation, and a small functional-programming toolkit.

It is intentionally low-level, macro-heavy, non-portable, and opinionated.

The goal is not to replace libc.

The goal is to make it possible to write small C programs that talk to the Linux kernel directly, with very little machinery in between.

```text
data is truth
code is pointer arithmetic
branches are shame
kernel is friend
```

## ⚠️ Status

This project is **experimental and educational**.

It is useful for:

- learning how C maps onto machine instructions;
- learning Linux system calls and ABI details;
- experimenting with custom runtime environments;
- writing very small binaries;
- AED/homework-style exercises;
- terminal experiments and tiny games;
- exploring manual memory management;
- experimenting with low-level functional-style helpers.

It is **not**:

- a libc replacement;
- portable C;
- safe for production software;
- thread-safe;
- ABI-independent;
- feature-complete;
- a general-purpose allocator;
- a standards-compliant replacement for `printf`, `malloc`, `strlen`, `strtod`, etc.

Expect sharp edges. Some of them are intentional. Some of them are bugs. Distinguishing between the two is one of the project's more educational features.

---

# Platform

`tiny.h` currently targets exactly:

```text
Architecture : x86-64
OS           : Linux
Compiler     : GCC
Assembly     : GNU inline assembly
Floating point: SSE2 + x87
Runtime      : direct syscalls, optionally no libc
```

The header contains compile-time checks and should fail when compiled for unsupported architectures.

There is no portability layer.

There is no Windows implementation.

There is no ARM implementation.

There is no “we'll abstract it later”.

---

# Repository layout

```text
tiny.h
    The entire runtime API.
    Single header, roughly 1670 lines / 72 KB.

example.c
    Example program.

test.c
    Low-level integration tests, roughly 1000 lines.

tests/test_tiny.c
    Deterministic unit-test suite.

Makefile
    Builds libc and no-libc test targets.

tiny.ld
    Custom linker script for the no-libc/minimal binary.

LICENSE
    AGPLv3.
```

The header is organized into sections:

```text
§1   Syscall numbers
§2   Alignment and pointer utilities
§3   Stack-frame helpers
§4   Scalar f64 arithmetic
§5   f64 comparisons
§6   f64 control flow
§7   f64 array helpers
§8   Integer bit operations
§9   String and memory operations
§10  Parsing and numeric conversion
§11  I/O
§12  Memory allocation
§13  tiny_str_t / small-string optimization
§14  Math constants and numeric helpers
§15  Interactive terminal input
§16  Entry points
§17  Functional programming helpers
§18  Linux namespaces
§19  Bump allocator
§20  Terminal and game loop
```

---

# Quick Start

## 1. Clone the project

```sh
git clone <repository-url>
cd <repository-directory>
```

## 2. Build a normal libc-backed program

This is the easiest mode for development and testing.

```sh
gcc -O2 \
    -fno-omit-frame-pointer \
    -fno-builtin \
    prog.c \
    -o prog
```

Run it normally:

```sh
./prog
```

## 3. Build without libc

This is the interesting mode.

```sh
gcc -O2 \
    -fno-omit-frame-pointer \
    -fno-builtin \
    -nostdlib \
    -D_TINY_NOSTDLIB \
    -T tiny.ld \
    prog.c \
    -o prog
```

Optionally strip ELF metadata:

```sh
sstrip prog
```

The resulting binary can be dramatically smaller than an ordinary dynamically linked program.

The actual size depends on the program and linker/toolchain behavior. The project includes examples targeting extremely small binaries, but size should be treated as an experiment rather than a promise.

---

# Why these compiler flags matter

Two flags are particularly important.

## `-fno-omit-frame-pointer`

`STACK(off)` accesses memory relative to `rbp`.

That means the generated code expects a conventional frame pointer.

Without:

```sh
-fno-omit-frame-pointer
```

the compiler may use `rbp` for something else or omit the frame pointer entirely.

Then code using `STACK(off)` can stop meaning what you think it means.

In other words:

```text
STACK(off)
    ↓
rbp-relative addressing
    ↓
requires predictable rbp
```

Do not casually remove this flag.

## `-fno-builtin`

The no-libc mode does not provide libc functions.

GCC is still allowed to recognize operations as built-ins and replace them with library/helper calls.

For example, apparently innocent code can turn into a call to an external helper such as:

```text
memcpy
__divdi3
__moddi3
```

which defeats the whole point of the no-libc build.

Use:

```sh
-fno-builtin
```

in no-libc builds.

---

# Minimal program

The same source can be compiled in either mode.

```c
#include "tiny.h"

BEGIN
    PRINTLN_STR("say hello", 9);
END
```

With libc:

```sh
gcc -O2 -fno-omit-frame-pointer -fno-builtin prog.c -o prog
```

Without libc:

```sh
gcc -O2 \
    -fno-omit-frame-pointer \
    -fno-builtin \
    -nostdlib \
    -D_TINY_NOSTDLIB \
    -T tiny.ld \
    prog.c \
    -o prog
```

`BEGIN` and `END` adapt the entry point automatically.

In normal mode they behave approximately like:

```c
int main(void) {
    ...
    return 0;
}
```

In no-libc mode they create a tiny runtime entry path using `_start` and direct exit syscalls.

The important idea is:

```text
same program
     │
     ├── libc build
     │      → main()
     │
     └── no-libc build
            → _start
            → tiny runtime
            → Linux syscalls
```

---

# Testing

Run the deterministic unit suite:

```sh
make test
```

The unit tests exercise the API without requiring unusual host behavior.

Coverage includes:

```text
alignment
arithmetic
comparisons
arrays
integer operations
strings
parsing
numeric helpers
functional helpers
```

The lower-level integration tests are separate because they depend on Linux behavior, process permissions, terminal state, or the host environment.

```sh
make test_integration_libc
```

and:

```sh
make test_nostdlib
```

The no-libc integration target requires `tiny.ld`.

A useful mental model is:

```text
make test
    deterministic API tests

make test_integration_libc
    runtime + Linux integration with libc

make test_nostdlib
    actual no-libc integration
```

Passing the unit tests does **not** mean the project is production-safe.

It means the test suite passed.

Humanity has historically confused those two concepts.

---

# How the runtime fits together

A typical program looks approximately like this:

```text
             your C source
                  │
                  ▼
             tiny.h macros
                  │
        ┌─────────┼──────────┐
        │         │          │
        ▼         ▼          ▼
      stack      XMM        heap
    STACK(off) registers     brk
        │         │          │
        └─────────┼──────────┘
                  ▼
          direct Linux syscalls
                  │
                  ▼
                kernel
```

There is deliberately very little between your code and the kernel.

The stack is commonly used as the primary working area.

Floating-point values are manipulated through explicit SSE/XMM operations.

Heap memory comes from `brk`.

I/O is performed directly through syscalls.

The no-libc build does not rely on the usual C runtime startup or standard library.

---

# API Reference

## §1 — Syscalls

The header defines syscall numbers used by the runtime.

Examples:

```c
SYS_READ
SYS_WRITE
SYS_EXIT
SYS_BRK
SYS_IOCTL
SYS_POLL
SYS_OPEN
SYS_CLOSE
SYS_UNSHARE

SYS_GETUID
SYS_GETGID

STDIN
STDOUT
```

Important values include:

```text
read       0
write      1
open       2
close      3
poll       7
brk       12
ioctl     16
exit      60
unshare   272
```

Critical values are guarded with `_Static_assert`.

---

# §2 — Alignment and pointer utilities

```c
ALIGN_UP(s)
ALIGN_DOWN(s)

OFFSET(ptr, bytes)
INDEX(base, i)
DEREF(addr)
```

Typical use:

```c
void *p = OFFSET(base, 16);
double *x = INDEX(array, 4);
double value = DEREF(addr);
```

`ALIGN_UP` rounds values to the project's required 8-byte alignment and is used throughout the allocators and stack-layout helpers.

---

# §3 — Stack-frame helpers

```c
STACK(off)
PUSH_FRAME(n)
POP_FRAME
```

`STACK(off)` provides an address relative to the current frame:

```c
#define MY_VAR  STACK(8)
#define MY_VAR2 STACK(16)

DEF(MY_VAR, 42.0);
PRINTLN(MY_VAR);
```

## Important

`PUSH_FRAME` and `POP_FRAME` are intended only for naked functions:

```c
__attribute__((naked))
```

Do not manually take over GCC's stack frame management in an ordinary C function.

A recommended layout style is:

```c
enum {
    OFF_X      = 8,
    OFF_Y      = OFF_X + ALIGN_UP(sizeof(double)),
    OFF_RESULT = OFF_Y + ALIGN_UP(sizeof(double)),
    TOTAL      = OFF_RESULT + ALIGN_UP(sizeof(double))
};

#define VAR_X      STACK(OFF_X)
#define VAR_Y      STACK(OFF_Y)
#define VAR_RESULT STACK(OFF_RESULT)
```

Prefer named offsets over magic numbers.

That way, changing the frame layout means changing one enum instead of hunting through a pile of unexplained `STACK(56)` calls.

---

# §4 — Scalar f64 arithmetic

Operations work on addresses containing `double`.

```c
DEF(addr, val)
COPY(dst, src)
SWAP(a, b)

INC(addr)
DEC(addr)

SUM(out, a, b)
SUB(out, a, b)
MUL(out, a, b)
DIV(out, a, b)
MOD(out, a, b)

ROOT(out, a)
ABS(out, a)
NEG(out, a)

MIN(out, a, b)
MAX(out, a, b)
AVG(out, a, b)
```

Example:

```c
DEF(STACK(8), 10.0);
DEF(STACK(16), 3.0);

DIV(STACK(24), STACK(8), STACK(16));
PRINTLN(STACK(24));
```

`ROOT` uses `sqrtsd`.

`ABS` and `NEG` are implemented using bit manipulation on the floating-point representation rather than branches.

`MOD` uses x87 `fprem1`.

It is functional, but its latency is variable and can be significantly worse than the other arithmetic helpers for some inputs.

---

# §5 — f64 comparisons

Comparison results are stored as doubles:

```text
true  = 1.0
false = 0.0
```

Available operations:

```c
IS_GT(out, a, b)
IS_LT(out, a, b)
IS_GE(out, a, b)
IS_LE(out, a, b)
IS_EQ(out, a, b)
IS_NE(out, a, b)

CMP(out, a, b)
```

`CMP` produces:

```text
-1.0   less than
 0.0   equal
 1.0   greater than
```

This is useful for sort-style comparators.

The comparison implementation uses `ucomisd`.

---

# §6 — f64 control flow

```c
FOR_RANGE(idx_addr, start, end)
FOR_DOWN(idx_addr, start, end)
WHILE_NZ(flag_addr)

IF_GT(a, b) { ... }
IF_LT(a, b) { ... }
IF_GE(a, b) { ... }
IF_LE(a, b) { ... }
IF_EQ(a, b) { ... }
IF_NE(a, b) { ... }
```

Example:

```c
FOR_RANGE(STACK(8), 0.0, 10.0) {
    PRINTLN(STACK(8));
}
```

The loop index is stored as a `double` in the provided address.

---

# §7 — f64 arrays

```c
FILL(base, count, val)
ZERO(addr)

ARRAY_GET(base, i, dst)
ARRAY_SET(base, i, src)
```

There are also generic pointer-array helpers:

```c
NTH(arr, idx, len)
find_if(base, count, stride, pred)
```

`NTH` returns `NULL` for an invalid index.

```c
const char *days[] = {
    "MON", "TUE", "WED", "THU",
    "FRI", "SAT", "SUN"
};

const char *d = NTH(days, 2, 7);   // "WED"
const char *x = NTH(days, 9, 7);   // NULL
```

---

# §8 — Integer bit operations

The project contains several branchless bit-manipulation helpers:

```c
IS_POW2(v)
IS_OPP_SIGN(x, y)
SIGN(v)

IABS(v)
IMIN(x, y)
IMAX(x, y)
IAVG(x, y)

MERGE(a, b, mask)
NEXT_POW2(v)
POPCOUNT(v)
TRAILING_ZEROS(v)
```

Examples:

```c
IS_POW2(16)
SIGN(-4)
IAVG(a, b)
NEXT_POW2(37)
POPCOUNT(mask)
```

`IAVG` avoids the overflow-prone intermediate:

```c
(x + y) / 2
```

and instead computes the average using bit operations.

`TRAILING_ZEROS` uses a De Bruijn sequence.

---

# ⚠️ Integer division in no-libc mode

Be particularly careful with ordinary integer arithmetic in `-nostdlib` builds.

The compiler may emit calls to libgcc helpers such as:

```text
__moddi3
__divdi3
```

Those helpers are not provided automatically by `tiny.h`.

For example, code such as:

```c
x % y
x / y
```

can produce unresolved runtime dependencies depending on the type and target.

For simple power-of-two cases, use bit operations where appropriate:

```c
x & 1
x >> n
```

Do not assume that “it's C, therefore the compiler must turn it into one instruction.”

Compilers enjoy surprises.

---

# §9 — String and memory operations

```c
STRLEN(s)

MEM_COPY(dst, src, n)
MEM_ZERO(dst, n)
```

These use x86 `rep` string instructions.

### `STRLEN`

Searches for the terminating NUL byte:

```c
long n = STRLEN("hello");
```

Complexity:

```text
O(n)
```

There is no explicit maximum length.

A non-terminated string can therefore cause an extremely long scan and potentially fault.

### `MEM_COPY`

Copies exactly `n` bytes:

```c
MEM_COPY(dst, src, n);
```

### `MEM_ZERO`

Zeroes exactly `n` bytes:

```c
MEM_ZERO(ptr, n);
```

These primitives are functional, but their performance should not automatically be assumed equivalent to optimized libc implementations for large workloads.

---

# §10 — Parsing and numeric conversion

```c
DTOA(x, out)
ATOF(s, end_ptr)
ATOI(s, end_ptr)
```

## `DTOA`

Converts a `double` to decimal text.

```c
char buf[32];

int len = DTOA(value, buf);
```

Important:

```text
buffer size: at least 32 bytes
output: not NUL-terminated
maximum: 15 fractional digits
```

Trailing fractional zeroes are removed.

The conversion is **not guaranteed to produce an IEEE-754 round-trip representation**.

It is intended for small educational programs, not precise serialization.

## `ATOF`

Parses a decimal string:

```c
const char *end;
double x = ATOF("123.45abc", &end);
```

`end` points to the first character that was not consumed.

Leading whitespace and a leading `-` are supported.

## `ATOI`

Same general idea for signed integers:

```c
long x = ATOI("1234", &end);
```

## Unsupported formats

The current parsers do not support:

```text
hexadecimal floating-point
scientific notation
NaN
Inf
```

The input scan is also not length-bounded.

---

# §11 — I/O

All I/O is direct syscall-based and unbuffered.

```c
READ(buf, len)

PRINT(addr)
PRINTLN(addr)

PRINT_INT(n)
PRINTLN_INT(n)

PRINT_STR(ptr, len)
PRINTLN_STR(ptr, len)

EXIT(code)
```

For example:

```c
PRINTLN_INT(42);
```

For raw strings:

```c
PRINT_STR("hello", 5);
```

`PRINT_STR` does not determine the string length.

The caller is responsible for providing the exact byte count.

Wrong length means you may print garbage or read past the intended memory.

For NUL-terminated strings:

```c
PRINT_STR(s, STRLEN(s));
```

---

# §12 — Memory allocation

The runtime exposes Linux `brk` directly:

```c
BRK_GET()
BRK_SET(addr)
SBRK(n)
```

`SBRK(n)` grows the heap and returns the beginning of the allocated region.

On failure it returns:

```c
(void *)-1
```

There are two higher-level allocators.

## Slab allocator

```c
SlabPool pool;

SLAB_INIT(&pool, slot_size, total_slots);

void *p = SLAB_ALLOC(&pool);

SLAB_FREE(&pool, p);
```

Characteristics:

```text
fixed-size objects
O(1) allocation
O(1) free
one large brk allocation
not thread-safe
not growable
```

If `slot_size` is smaller than the minimum slot metadata size, it is increased automatically.

Good for things such as:

```text
game entities
tree nodes
linked-list nodes
fixed-size objects
```

Do not use it if you need arbitrary-size allocations or `realloc`.

Do not double-free.

There is little sympathy built into the allocator.

---

# §13 — tiny_str_t

`tiny_str_t` is a fixed 16-byte string representation using small-string optimization.

It has two modes.

```text
INLINE

[ uint32_t length | char data[12] ]

HEAP / VIEW

[ uint32_t length+flag | char prefix[4] | char *ptr ]
```

The high bit of the length field indicates heap/view mode.

Inline strings hold up to 12 bytes directly.

Heap-mode strings borrow an external buffer.

They do **not** own the memory.

They do **not** free the memory.

The caller owns the underlying storage.

## Constructors

```c
S("literal")
S_PTR(ptr, len)
S_VIEW(ptr, len)
STR_FROM_BUF(buf, len)

STR_FROM_DOUBLE(x)
STR_FROM_INT(n)
```

`S("literal")` uses a compile-time assertion for literals longer than 12 bytes.

For example:

```c
tiny_str_t s = S("HELLO");
```

For external memory:

```c
tiny_str_t s = S_PTR(ptr, len);
```

For a buffer where ownership/lifetime is inconvenient:

```c
tiny_str_t s = STR_FROM_BUF(buf, len);
```

For short strings, `STR_FROM_BUF` copies into the inline representation.

For long strings, it creates a borrowed view.

## Accessors

```c
STR_LEN(s)
STR_IS_INLINED(s)
STR_DATA(s)
```

## Comparison

```c
STR_EQ(s1, s2)
STR_EQ_LIT(s, "literal")
STR_STARTS_WITH(s, "prefix")
```

`STR_EQ` can reject unequal strings using their stored length and four-byte prefix before doing the full comparison.

## Operations

```c
STR_SLICE(s, start, n)
STR_FIND_BYTE(s, c)
```

`STR_SLICE` does not allocate.

It produces a borrowed pointer-style view.

## Printing

```c
STR_PRINT(s)
STR_PRINTLN(s)
```

---

# ⚠️ tiny_str_t lifetime rule

This is one of the easiest ways to create a dangling pointer:

```c
tiny_str_t make_string(void) {
    char buf[32] = "hello";
    return S_PTR(buf, 5);
}
```

When the function returns:

```text
buf dies
   ↓
tiny_str_t still contains pointer
   ↓
dangling reference
```

Use a copying constructor such as:

```c
STR_FROM_BUF(buf, len)
```

when the backing storage will not outlive the string.

---

# §14 — Math helpers

Constants:

```c
PI
TAU
EULER
```

Helpers:

```c
CLAMP(v, lo, hi)
LERP(a, b, t)

SIGN_IDX(n)

INT_POW(base, exp)
GAUSS(n)
FACTORIAL(n)

IS_LEAP(y)
SWAP_CHARS(a, b)
```

Examples:

```c
double x = LERP(0.0, 100.0, 0.25);
```

produces:

```text
25.0
```

`FACTORIAL` uses `long` and overflows for values above 20 on the intended platform.

That overflow is not checked.

---

# §15 — Interactive input

These helpers are aimed at simple command-line exercises.

```c
LEER_NUMERO("Enter number: ")
LEER_LETRA("Enter letter: ")

PRINT_LABEL_D("value: ", value)
PRINT_LABEL_I("count: ", count)
PRINT_LABEL_S("name: ", name, len)
```

`LEER_NUMERO` reads one byte at a time until newline or EOF.

Its input buffer is limited to 31 characters.

`LEER_LETRA` consumes the line and returns the first non-space character, converted from lowercase ASCII to uppercase.

These are convenience functions, not robust terminal input libraries.

---

# §16 — Entry points

Normally:

```c
BEGIN
    ...
END
```

is enough.

The header generates different entry behavior depending on:

```c
_TINY_NOSTDLIB
```

In no-libc mode, the runtime provides `_start` and eventually uses the exit syscall.

There is also:

```c
BEGIN_ISOLATED
```

which establishes Linux namespaces before executing the program body.

---

# §17 — Functional toolkit

The functional helpers operate directly on arrays.

The project provides variants for:

```text
double
long
```

and common operations such as:

```c
MAP_D
MAP_D_INTO

MAP_L
MAP_L_INTO

FILTER_D
FILTER_L

REDUCE_D
REDUCE_L

FOR_EACH_D
FOR_EACH_L

ZIP_D
ZIP_L
```

Typical function signatures are:

```c
double fn_d(double x);

long fn_l(long x);

double fn_dd(double acc, double x);

long fn_ll(long acc, long x);

double fn_zip(double a, double b);

int pred_d(const double *p);

int pred_l(const long *p);
```

Example:

```c
double add(double a, double b) {
    return a + b;
}

double biggest(double a, double b) {
    return a > b ? a : b;
}

int positive(const double *p) {
    return *p > 0.0;
}

double total = REDUCE_D(arr, 10, 0.0, add);

double maximum = REDUCE_D(arr, 10, arr[0], biggest);

long count;

FILTER_D(arr, 10, out, &count, positive);
```

The functional layer does not allocate memory.

Function pointers are used to apply transformations or predicates.

---

# §18 — Linux namespaces

The runtime exposes:

```c
UNSHARE(flags)
WRITE_IDMAP(path, content, len)

GETUID()
GETGID()
```

Supported namespace flags include:

```c
CLONE_NEWUSER
CLONE_NEWNS
CLONE_NEWPID
CLONE_NEWNET
CLONE_NEWUTS
CLONE_NEWIPC
```

For example:

```c
UNSHARE(CLONE_NEWUSER);
```

User namespace setup can then populate:

```text
/proc/self/setgroups
/proc/self/uid_map
/proc/self/gid_map
```

The provided `BEGIN_ISOLATED` path automates the project's intended isolation setup.

## PID namespace warning

After:

```c
UNSHARE(CLONE_NEWPID);
```

the current process does not magically become PID 1.

A child must be created afterward to become PID 1 in the new PID namespace.

---

# §19 — Bump allocator

The bump allocator is the simple alternative to the slab allocator.

```c
BumpAlloc b;

BUMP_INIT(&b, total_bytes);

void *p = BUMP_ALLOC(&b, size);

BUMP_RESET(&b);

BUMP_USED(&b);
BUMP_LEFT(&b);
```

Behavior:

```text
allocate → move pointer forward
allocate → move pointer forward
allocate → move pointer forward
reset    → move pointer back to beginning
```

Freeing individual allocations is not supported.

This is useful when the lifetime of many objects is identical.

For example:

```text
load level
    ↓
allocate level data
allocate entities
allocate temporary arrays
allocate strings
    ↓
finish level
    ↓
BUMP_RESET()
```

Choose:

```text
Slab
    fixed object size
    individual free needed

Bump
    varying sizes
    everything dies together
```

Neither allocator provides `realloc`.

---

# §20 — Terminal and game loop

The terminal subsystem uses direct `ioctl` and `poll` syscalls.

```c
TERM_RAW()
TERM_RESET()
TERM_NONBLOCK()

POLL_KEY(ms)
READ_KEY()
```

ANSI helpers:

```c
CLEAR_SCREEN()

MOVE_CURSOR(row, col)

CURSOR_HIDE()
CURSOR_SHOW()
```

Key constants:

```c
KEY_ESC
KEY_UP
KEY_DOWN
KEY_LEFT
KEY_RIGHT
KEY_ENTER
KEY_SPACE

KEY_CTRL(c)
```

Arrow keys arrive as ANSI escape sequences:

```text
ESC [
 A   up
 B   down
 C   right
 D   left
```

After reading `KEY_ESC`, use:

```c
CONSUME_ESCAPE()
```

to consume the rest of the sequence.

## Minimal game loop

```c
BEGIN

    TERM_RAW();
    TERM_NONBLOCK();
    CURSOR_HIDE();

    int px = 10;
    int py = 10;
    int running = 1;

    while (running) {

        CLEAR_SCREEN();

        MOVE_CURSOR(py, px);
        PRINT_STR("@", 1);

        if (POLL_KEY(33)) {

            int k = READ_KEY();

            if (k == 'q')
                running = 0;

            if (k == 'w')
                py--;

            if (k == 's')
                py++;

            if (k == 'a')
                px--;

            if (k == 'd')
                px++;

            if (k == KEY_ESC) {
                int direction = CONSUME_ESCAPE();

                if (direction == KEY_UP)
                    py--;

                if (direction == KEY_DOWN)
                    py++;

                if (direction == KEY_LEFT)
                    px--;

                if (direction == KEY_RIGHT)
                    px++;
            }
        }
    }

    CURSOR_SHOW();
    TERM_RESET();
    CLEAR_SCREEN();

END
```

`33 ms` gives approximately a 30 Hz polling interval.

---

# ⚠️ Terminal safety

Always restore the terminal:

```c
TERM_RESET();
```

before exiting.

If the process leaves the terminal in raw mode, the shell can become unpleasantly broken.

If that happens, type:

```sh
reset
```

and press Enter.

This is one of those bugs where the program can technically be finished while the computer still appears haunted.

---

# Common mistakes

| Mistake | Result | Fix |
|---|---|---|
| Remove `-fno-omit-frame-pointer` | `STACK()` assumptions break | Keep the flag |
| Remove `-fno-builtin` | GCC emits libc/libgcc helper calls | Keep the flag |
| Use `%` in no-libc code | May require `__moddi3` | Use explicit low-level operations where appropriate |
| Use integer `/` in no-libc code | May require `__divdi3` | Avoid unsupported helper generation |
| Give `PRINT_STR` the wrong length | Garbage or invalid memory access | Use exact length |
| Use `PUSH_FRAME` in a normal function | GCC/frame corruption | Naked functions only |
| Return `S_PTR()` into a dead stack buffer | Dangling pointer | Use `STR_FROM_BUF()` |
| Forget `TERM_RESET()` | Shell remains in raw mode | Run `reset` |
| Use `-Wl,-N` with `tiny.ld` | Can produce invalid/misaligned segments | Let `tiny.ld` handle layout |
| Use `CLONE_NEWPID` without forking | Current process keeps its old PID | Fork after unshare |
| `FACTORIAL(n)` with large `n` | Integer overflow | Keep `n <= 20` |
| Assume `tiny_str_t` owns its buffer | Use-after-lifetime bugs | Treat heap strings as borrowed views |

---

# Known Issues and Performance Notes

This section is intentionally separate from the normal API documentation.

These are not theoretical “maybe someday” concerns. They are known design limitations, implementation weaknesses, or areas that require investigation.

## 1. `MOD` uses x87 `fprem1`

```c
MOD(out, a, b)
```

uses an `fprem1` loop.

The operation is valid, but the loop can require multiple iterations depending on the operands.

That means its latency is less predictable than operations such as:

```text
ADD
SUB
MUL
DIV
SQRT
```

Do not use it where tightly bounded execution time matters.

---

## 2. `MEM_COPY` is not a universal high-performance memcpy

The implementation uses:

```asm
rep movsb
```

This is simple and compact, but large copies may benefit from more specialized implementations depending on the processor and workload.

Possible future optimizations include:

```text
rep movsq
SSE2 copies
AVX copies
alignment-aware paths
small-copy fast paths
```

The important distinction is:

```text
works
≠
optimal for every workload
```

---

## 3. `STRLEN` has no explicit bound

The implementation scans until it finds NUL.

That means:

```c
STRLEN(ptr);
```

assumes that `ptr` points to a valid NUL-terminated string.

A malformed pointer or missing terminator can result in a scan far beyond the intended object.

Modern libc implementations may use much more sophisticated vectorized strategies.

`tiny.h` deliberately favors directness and small implementation size.

---

## 4. `DTOA` is not round-trip-safe

`DTOA` generates at most 15 fractional digits.

Repeated floating-point operations can introduce rounding effects, and the implementation does not promise that:

```text
double
  ↓
DTOA
  ↓
ATOF
  ↓
same exact bit pattern
```

will always hold.

Use it for:

```text
display
homework
simple terminal output
experiments
```

not as a general-purpose floating-point serialization format.

---

## 5. `ATOF` and `ATOI` are deliberately incomplete

They do not implement every format accepted by a full standard conversion routine.

Notably:

```text
hexadecimal
scientific notation
NaN
Inf
```

are unsupported.

Input scanning is also not externally length-bounded.

---

## 6. `STR_EQ` has an optimization that is not magic

`STR_EQ` can compare:

```text
length
    ↓
four-byte prefix
    ↓
full string
```

The prefix can quickly reject many unequal strings.

But if the first four bytes match, the full string comparison is still required.

The optimization therefore helps primarily with early rejection.

It cannot turn arbitrary long-string equality into O(1).

---

## 7. Inline constants may create unnecessary memory traffic

Some floating-point macros use static constants stored in memory.

For example, operations conceptually equivalent to:

```c
*x += 1.0;
```

may involve loading the constant from memory rather than having it represented in some more efficient form.

This is a micro-optimization issue, not a primary correctness problem.

---

## 8. XMM register usage is intentionally explicit

The floating-point API directly manipulates XMM registers.

This keeps the implementation visible and predictable from an educational perspective, but complex macro composition can increase register pressure.

Some operations can also use more registers than strictly necessary.

The project values explicit low-level behavior more highly than abstraction.

---

## 9. Slab initialization updates the free-list head repeatedly

During slab initialization, the free-list pointer is updated for every slot.

The implementation could construct the list locally and assign the final head once.

This matters primarily during initialization and is not currently the dominant cost of the allocator.

---

## 10. `STR_SLICE` always produces pointer/view mode

A short slice of a long string does not get repacked into the inline representation.

So:

```text
long string
   ↓
STR_SLICE(...)
   ↓
borrowed view
```

rather than:

```text
short result
   ↓
12-byte inline string
```

This keeps slicing allocation-free and simple but sacrifices the compact inline representation in some cases.

---

# ⚠️ Critical correctness issue

The original implementation has a known bug in the sign-mask path used by the floating-point absolute-value/negation machinery.

The problematic pattern moves a 64-bit mask through a 32-bit `movd` operation:

```asm
movq  (%2), %%rax
movd  %%rax, %%xmm1
```

`movd` transfers only 32 bits.

For a mask such as:

```text
0x7FFFFFFFFFFFFFFF
```

that is not equivalent to transferring the complete 64-bit value.

The intended instruction is a 64-bit transfer such as:

```asm
movq %%rax, %%xmm1
```

This should be treated as a **correctness bug**, not merely a performance issue.

Until verified and fixed in the implementation, code relying on that path should be considered suspect.

---

# Design philosophy

The project makes several deliberate trade-offs.

## Small implementation over completeness

The runtime does not try to reproduce libc.

Instead it implements only the pieces needed by the project's intended experiments.

## Explicit machine-level behavior

Many operations intentionally expose:

```text
registers
stack layout
syscalls
pointer arithmetic
instruction selection
```

rather than hiding them behind generic abstractions.

## No allocation where possible

The string and functional APIs try to operate without dynamic allocation.

The allocators themselves are deliberately simple:

```text
slab
bump
```

rather than trying to become another general-purpose heap manager.

## One source, two runtime modes

The same program can be built:

```text
with libc
```

for convenient testing, or:

```text
without libc
```

for direct kernel interaction and minimal binaries.

That dual-mode behavior is one of the project's central features.

---

# Recommended workflow

For normal development:

```sh
# 1. Write program
vim prog.c

# 2. Compile with libc
gcc -O2 -fno-omit-frame-pointer -fno-builtin \
    prog.c -o prog

# 3. Test
./prog

# 4. Run unit tests
make test

# 5. Run integration tests
make test_integration_libc

# 6. Test the real no-libc path
make test_nostdlib

# 7. Build an intentionally tiny binary
gcc -O2 \
    -fno-omit-frame-pointer \
    -fno-builtin \
    -nostdlib \
    -D_TINY_NOSTDLIB \
    -T tiny.ld \
    prog.c \
    -o prog

# 8. Optionally strip
sstrip prog
```

A good rule is:

```text
libc build
    → development and debugging

unit tests
    → API correctness

integration tests
    → Linux/runtime behavior

no-libc build
    → verify actual tiny-runtime compatibility

sstrip
    → final size experiment
```

Do not start debugging an obscure linker problem in the no-libc binary when the same program has not even been tested in libc mode.

That is simply choosing suffering manually.

---

# When to use tiny.h

Use it when you want:

```text
small binaries
direct syscalls
Linux ABI experiments
assembly-adjacent C
custom memory management
terminal programming
educational exercises
low-level experimentation
```

Do not use it when you need:

```text
portability
memory safety
threads
production-grade allocation
complete numeric parsing
precise floating-point serialization
standard C runtime behavior
battle-tested string handling
```

There is nothing wrong with using libc because someone else already spent decades discovering how many ways `strlen()` can go wrong.

---

# License

AGPLv3.

See `LICENSE`.

The project documentation intentionally interprets this as part of the project's “knowledge should remain open” philosophy.

---

# Bug reports

Bugs are expected.

Useful bug reports should include:

```text
compiler version
kernel version
CPU architecture
build command
program/source reproducer
expected behavior
actual behavior
```

A report consisting only of:

```text
doesn't work
```

is technically a bug report, but mostly an emotional statement.

Fixes are not guaranteed.

Regressions are possible.

---

# Contributions

The project currently prefers focused, minimal changes.

Public API naming convention:

```text
ALL_CAPS
```

Internal implementation identifiers:

```text
_lowercase
```

Prefer:

```text
small changes
existing style
minimal abstractions
focused patches
```

Avoid adding abstractions unless they solve a real problem for the project's intended use.

The project is deliberately opinionated about low-level implementation details.

---

# Final mental model

When coming back to the project after six months and wondering what on Earth was going on, remember:

```text
tiny.h
│
├── stack
│     STACK(off)
│
├── floating point
│     XMM / x87
│
├── heap
│     brk
│     ├── slab
│     └── bump
│
├── strings
│     tiny_str_t
│     ├── inline ≤ 12 bytes
│     └── borrowed view
│
├── I/O
│     direct syscalls
│
├── terminal
│     ioctl + poll + ANSI
│
├── Linux isolation
│     namespaces
│
├── functional helpers
│     map / filter / reduce / zip
│
└── entry point
      main()
        or
      _start
```

The project is not trying to make C safer.

It is trying to make the machinery underneath C visible.

That is the point.
