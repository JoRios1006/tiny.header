# ⚠️ ALPHA. NO WARRANTY. YOU BREAK IT, YOU OWN PIECES.

---

# tiny.h

Single header. No libc. x86-64 Linux only.

 write this because C string bad, malloc bad, include 47 headers for `printf` bad.
 want write program that yell at operating system directly. No middleman. No mystery.

This not safe. This not portable. This not for production.
This for learning, for fun, for AED homework that run in 1152 bytes.

---

## WHAT THIS IS

Direct syscall toolkit. Macro-heavy. Inline asm everywhere.
Philosophy: **data is truth. code is pointer arithmetic. branches are shame.**

Stack = scratchpad. XMM registers = calculator. `brk` = heap. Kernel = only friend.

---

## WHAT THIS IS NOT

- Not libc replacement
- Not safe
- Not portable (x86-64 Linux ONLY, hard error if wrong)
- Not finished
- Not your fault when it breaks (actually maybe your fault)

---

## PLATFORM

```
Architecture : x86-64
OS           : Linux
Compiler     : GCC with GNU inline asm + SSE2
```

Header guards enforce this. Wrong arch = `#error` at compile time. Good.

---

## BUILD

```sh
# with libc (quick test, normal main)
gcc -O2 -fno-omit-frame-pointer -fno-builtin prog.c -o prog

# without libc (real tiny, _start entry)
gcc -O2 -fno-omit-frame-pointer -fno-builtin -nostdlib \
    -D_TINY_NOSTDLIB -T tiny.ld prog.c -o prog
sstrip prog   # optional: remove all ELF metadata, ~1KB result
```

`-fno-omit-frame-pointer` **required**. `STACK(off)` reads `rbp`. No `rbp` = chaos.
`-fno-builtin` **required**. Without it GCC replaces loops with `memcpy` call = linker fail.

---

## MINIMAL PROGRAM

```c
#include "tiny.h"

BEGIN
    PRINTLN_STR(" say hello", 14);
END
```

`BEGIN`/`END` expand to `main()` or naked `_start` depending on `_TINY_NOSTDLIB`.
Same source file compiles both ways. No code change needed.

---

## SECTION REFERENCE

---

### §1 — Syscall Numbers

```c
SYS_READ   0     SYS_WRITE  1     SYS_EXIT  60
SYS_BRK    12    SYS_IOCTL  16    SYS_POLL   7
SYS_OPEN   2     SYS_CLOSE  3     SYS_UNSHARE 272
SYS_GETUID 102   SYS_GETGID 104
STDIN 0    STDOUT 1
```

`_Static_assert` guards on critical values. If kernel ABI changes, header fails loudly.

---

### §2 — Alignment & Pointer Utils

```c
ALIGN_UP(s)         // round s up to next 8-byte boundary
ALIGN_DOWN(s)       // round s down to 8-byte boundary
OFFSET(ptr, bytes)  // ptr + bytes (byte arithmetic, any type)
INDEX(base, i)      // &((double*)base)[i]
DEREF(addr)         // *(double*)addr
```

`ALIGN_UP` used everywhere — slab, bump, stack layout.
`DEREF` read/write double at void* addr.  use often.

---

### §3 — Stack Frame Helpers

```c
STACK(off)      // void* to [rbp - off] in current frame
PUSH_FRAME(n)   // naked fn only: push rbp, set frame, reserve n bytes
POP_FRAME       // naked fn only: restore rbp/rsp
```

`STACK(off)` is how  name stack memory:

```c
#define MY_VAR  STACK(8)
#define MY_VAR2 STACK(16)
// ...
DEF(MY_VAR, 42.0);
PRINTLN(MY_VAR);
```

**`PUSH_FRAME`/`POP_FRAME` only safe in `__attribute__((naked))` functions.**
In normal functions GCC owns frame. Do not fight GCC.  learn hard way.

Stack layout macro pattern (clean, zero-cost):

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

---

### §4 — XMM Scalar f64 Arithmetic

All ops take `void*` addresses. All use `xmm0`/`xmm1`. All `volatile` — optimizer cannot remove.

```c
DEF(addr, val)      // *addr = literal double
COPY(dst, src)      // *dst  = *src
SWAP(a, b)          // swap *a, *b  (two xmm regs, no stack temp)
INC(addr)           // *addr += 1.0
DEC(addr)           // *addr -= 1.0
SUM(out, a, b)      // *out = *a + *b
SUB(out, a, b)      // *out = *a - *b
MUL(out, a, b)      // *out = *a * *b
DIV(out, a, b)      // *out = *a / *b
MOD(out, a, b)      // *out = fmod(*a, *b)  via x87 fprem1, no libm
ROOT(out, a)        // *out = sqrt(*a)       sqrtsd, one instruction
ABS(out, a)         // *out = |*a|           andpd sign mask, branchless
NEG(out, a)         // *out = -*a            xorpd sign bit, branchless
MIN(out, a, b)      // *out = min(*a, *b)    minsd
MAX(out, a, b)      // *out = max(*a, *b)    maxsd
AVG(out, a, b)      // *out = (*a + *b) * 0.5
```

`MOD` uses x87 `fprem1` loop. Correct but slower than others. Fine for homework.
`ABS`/`NEG` are bitmask ops — no branch, no comparison, just flip sign bit.

---

### §5 — f64 Comparisons → Double Flag

Store result as `1.0` (true) or `0.0` (false) at `out`. Use `ucomisd` — NaN safe.

```c
IS_GT(out, a, b)    // *out = (*a >  *b) ? 1.0 : 0.0
IS_LT(out, a, b)    // *out = (*a <  *b) ? 1.0 : 0.0
IS_GE(out, a, b)    // *out = (*a >= *b) ? 1.0 : 0.0
IS_LE(out, a, b)    // *out = (*a <= *b) ? 1.0 : 0.0
IS_EQ(out, a, b)    // *out = (*a == *b) ? 1.0 : 0.0
IS_NE(out, a, b)    // *out = (*a != *b) ? 1.0 : 0.0
CMP(out, a, b)      // *out = -1.0 / 0.0 / 1.0  (less/equal/greater)
```

`CMP` useful for sort comparators. `IS_*` useful for storing condition results
in stack slots and branching later, or using as multiplier masks.

---

### §6 — f64 Control Flow

```c
FOR_RANGE(idx_addr, start, end)   // forward loop, INC each iter
FOR_DOWN(idx_addr, start, end)    // reverse loop, DEC each iter
WHILE_NZ(flag_addr)               // loop while *flag != 0.0
IF_GT(a, b)  { }                  // conditional block
IF_LT(a, b)  { }
IF_GE(a, b)  { }
IF_LE(a, b)  { }
IF_EQ(a, b)  { }
IF_NE(a, b)  { }
```

`idx_addr` must be `void*` to double on stack. `start`/`end` are double literals.

```c
FOR_RANGE(STACK(8), 0.0, 10.0) {
    // DEREF(STACK(8)) is current index
    PRINTLN(STACK(8));
}
```

---

### §7 — f64 Array Helpers

```c
FILL(base, count, val)        // set count doubles to val
ZERO(addr)                    // zero one double slot  (= DEF(addr, 0.0))
ARRAY_GET(base, i, dst)       // dst = base[i]
ARRAY_SET(base, i, src)       // base[i] = src

NTH(arr, idx, len)
// bounds-checked pointer into any pointer array
// returns arr[idx] if 0 <= idx < len, NULL otherwise
// NULL = "not valid index". 0 IS valid index. negative idx → wraps to huge → NULL.
// example:
const char *days[] = {"MON","TUE","WED","THU","FRI","SAT","SUN"};
const char *d = NTH(days, 2, 7);   // "WED"
const char *x = NTH(days, 9, 7);   // NULL

find_if(base, count, stride, pred)
// linear scan, returns void* to first match or NULL
// pred: static inline int my_pred(void *p) { return *(double*)p > 0.0; }
```

---

### §8 — Integer Bit Ops

Branchless integer hacks. Techniques from Sean Eron Anderson's Bit Twiddling Hacks (public domain).

```c
IS_POW2(v)          // 1 if v is power of 2 (v > 0)
IS_OPP_SIGN(x, y)   // 1 if x and y have opposite signs
SIGN(v)             // -1, 0, or +1
IABS(v)             // |v| without branch  (signed long)
IMIN(x, y)          // branchless min  (signed long)
IMAX(x, y)          // branchless max  (signed long)
IAVG(x, y)          // (x+y)/2 without overflow: (x&y)+((x^y)>>1)
MERGE(a, b, mask)   // bits from b where mask=1, from a where mask=0
NEXT_POW2(v)        // round up to next power of 2 (32-bit)
POPCOUNT(v)         // count set bits (32-bit, parallel Hamming weight)
TRAILING_ZEROS(v)   // count trailing zero bits (DeBruijn multiply, 32-bit)
```

`IAVG` overflow-safe — no `(x+y)` intermediate. Use instead of `(x+y)/2`.
`NEXT_POW2` useful for slab/hash table sizing.
`TRAILING_ZEROS` uses DeBruijn sequence multiply — O(1), no loop, no branch.

**Warning:** do NOT use `%` operator in `-nostdlib` code. GCC replaces with `__moddi3`
from libgcc = undefined symbol = segfault. Use `& 1` for even/odd, shifts for powers of 2.

---

### §9 — String & Memory Ops

```c
STRLEN(s)              // NUL-terminated string length via repne scasb
                       // returns long. Does NOT count NUL byte.
MEM_COPY(dst, src, n)  // copy n bytes via rep movsb
MEM_ZERO(dst, n)       // zero n bytes via rep stosb
```

All three use rep-string instructions — efficient, correct, no libm.
`STRLEN` scans using `rdi`/`rcx`/`al`. Cost O(n). Called at runtime, not compile-time.

---

### §10 — Parsing & Conversion

```c
DTOA(x, out)              // double → decimal string in out[]. No NUL. Returns length (int).
                          // out must be >= 32 bytes. Strips trailing fractional zeros.
                          // Integer part via x87 fbstp (BCD). No soft division.

ATOF(s, end_ptr)          // decimal string → double.
                          // Skips leading whitespace. Handles '-'.
                          // *end_ptr set to first unparsed char (pass NULL to ignore).

ATOI(s, end_ptr)          // decimal string → long.
                          // Same rules as ATOF.
```

`DTOA` accuracy: 15 fractional digits max. Not IEEE-754 round-trip guaranteed.
Fine for homework, not for serialization.

`ATOF`/`ATOI` do NOT handle hex, scientific notation, or `NaN`/`Inf`.

---

### §11 — I/O

All I/O is direct syscall. No buffering. No formatting safety.

```c
READ(buf, len)            // read from stdin. Returns bytes read (int).
PRINT(addr)               // print *(double*)addr to stdout
PRINTLN(addr)             // print *(double*)addr + '\n'
PRINT_INT(n)              // print signed long
PRINTLN_INT(n)            // print signed long + '\n'
PRINT_STR(ptr, len)       // write raw bytes to stdout
PRINTLN_STR(ptr, len)     // write raw bytes + '\n'
EXIT(code)                // _exit syscall. Noreturn. __builtin_unreachable after.
```

`PRINT_STR(ptr, len)` — **len must be exact**. No NUL detection. Wrong len = garbage or segfault.
Use `STRLEN(s)` to compute len at runtime, or count manually for literals.

---

### §12 — Memory Allocation (brk + Slab)

```c
BRK_GET()                 // return current brk (program break)
BRK_SET(addr)             // set brk to addr, return new brk
SBRK(n)                   // grow heap by n bytes, return pointer to allocated region
                          // returns (void*)-1 on failure

// Slab allocator — fixed-size slots, O(1) alloc/free
SlabPool pool;
SLAB_INIT(&pool, slot_size, total_slots)
// one SBRK call, builds free list. slot_size auto-aligned via ALIGN_UP.
// slot_size < sizeof(SlabSlot) → bumped to sizeof(SlabSlot) automatically.
// returns -1 on SBRK failure.

void *ptr = SLAB_ALLOC(&pool)   // pop free list head. NULL if exhausted.
SLAB_FREE(&pool, ptr)           // push back to free list. No bounds check.
```

Slab = fixed-size homogeneous data (linked list nodes, tree nodes, game entities).
Not thread-safe. Not growable. Not forgiving of double-free.

---

### §13 — tiny_str_t (SSO String)

German-style small-string optimization. 16 bytes always.

```
Inline  (S()):      [ uint32_t len (bit31=0) | char data[12]          ]
Heap    (S_PTR()):  [ uint32_t len (bit31=1) | char prefix[4] | char* ]
```

Bit 31 of `length` = heap flag. `STR_LEN` masks it off. Inline ≤ 12 chars stored directly.
Heap strings borrow pointer — **no ownership, no free, caller manages lifetime.**
`prefix[4]` = first 4 bytes of heap string, copied on construction. Fast EQ rejection.

```c
// constructors
tiny_str_t s  = S("HELLO")              // inline, compile-time, _Static_assert if > 12 chars
tiny_str_t s  = S_PTR(ptr, len)         // heap borrow, copies prefix
tiny_str_t s  = S_VIEW(ptr, len)        // alias for S_PTR, signals read-only intent
tiny_str_t s  = STR_FROM_BUF(buf, len)  // inline if len<=12 (copies), heap if len>12 (borrows)
tiny_str_t s  = STR_FROM_DOUBLE(x)      // DTOA → STR_FROM_BUF
tiny_str_t s  = STR_FROM_INT(n)         // _itoa → STR_FROM_BUF

// accessors
STR_LEN(s)                  // uint32_t length (bit31 masked off)
STR_IS_INLINED(s)           // 1 if inline mode
STR_DATA(s)                 // const char* to bytes (correct for both modes)

// comparison
STR_EQ(s1, s2)              // full equality: len → prefix uint32 → rep cmpsb
STR_EQ_LIT(s, "lit")        // compare against literal (no temp tiny_str_t)
STR_STARTS_WITH(s, "pre")   // prefix match

// operations
STR_SLICE(s, start, n)      // pointer-mode view, no alloc. Borrows from s.
STR_FIND_BYTE(s, c)         // index of first c via repne scasb, or -1

// I/O
STR_PRINT(s)                // write to stdout
STR_PRINTLN(s)              // write to stdout + '\n'
```

`S("literal")` fires `_Static_assert` if literal > 12 chars at compile time. Good.
`S_PTR` for anything longer. `STR_SLICE` always returns heap-mode view — no ambiguity.

**Common mistake:** building `S_PTR` from a stack buffer then returning the `tiny_str_t`.
Buffer dies, pointer dangles. Use `STR_FROM_BUF` instead — copies if ≤ 12 chars.

---

### §14 — Math Constants & Numeric Helpers

```c
PI              // 355.0/113.0  (Milü, error < 3e-7, no libm)
TAU             // 2*PI
EULER           // 878.0/323.0  (e approximation)

CLAMP(v, lo, hi)        // lo if v<lo, hi if v>hi, else v  (double)
LERP(a, b, t)           // a + t*(b-a)  linear interpolation
SIGN_IDX(n)             // 0=negative, 1=zero, 2=positive  → array index
INT_POW(base, exp)      // base^exp, long, exp>=0, no overflow check
GAUSS(n)                // n*(n+1)/2  sum of 1..n, long
FACTORIAL(n)            // n! iterative, long. Overflows for n > 20.
IS_LEAP(y)              // 1 if y is Gregorian leap year
SWAP_CHARS(a, b)        // XOR swap two chars, no temp
```

`SIGN_IDX` pattern from AED exercises — throw it at a 3-element array:

```c
double buckets[3] = {0};           // [negative, zero, positive]
buckets[SIGN_IDX(value)] += value; // no if-chain
```

`IS_LEAP` uses `%` internally → needs libc or `-fno-builtin`. In `-nostdlib` mode,
replace with bitwise if needed (only powers-of-2 divisors are cheap).

---

### §15 — Interactive I/O Helpers

Prompted input pattern. Common in AED homework. Built on `READ` + `ATOF`.

```c
double n  = LEER_NUMERO("Enter number: ")
// prints prompt, reads line, returns double

char c    = LEER_LETRA("Enter letter: ")
// prints prompt, returns first non-space char uppercased

PRINT_LABEL_D("result: ", value)   // label + double + '\n'
PRINT_LABEL_I("count:  ", n)       // label + long   + '\n'
PRINT_LABEL_S("name:   ", s, len)  // label + string + '\n'
```

`LEER_NUMERO` reads one byte at a time until `\n` or EOF. Max 31 chars.
`LEER_LETRA` drains whole line, returns first non-space char. Uppercases a-z.

---

### §16 — Entry Points (BEGIN / END)

Dual-mode entry. Same source file, two compilation targets.

```c
BEGIN       // expands to: int main(void) {
END         // expands to: return 0; }

// with -D_TINY_NOSTDLIB:
BEGIN       // expands to: __attribute__((noinline)) void _tiny_run(void) {
END         // expands to: EXIT(0); }
            // _start defined automatically, calls _tiny_run
```

`BEGIN_ISOLATED` variant — same but unshares `CLONE_NEWUSER | CLONE_NEWIPC | CLONE_NEWUTS`
before running body. Writes uid/gid maps. Sandboxed execution, no sudo needed.

```c
BEGIN_ISOLATED
    // running in isolated user+IPC+UTS namespace
    // uid 0 inside namespace, original uid outside
END
```

---

### §17 — Functional Toolkit

Higher-order ops over arrays. Function pointer per element. No alloc.
Inspired by Clojure/FP. Throw function at data, get data back.

```c
// function signatures expected:
double fn_d(double x)              // MAP_D, FOR_EACH_D
long   fn_l(long x)                // MAP_L, FOR_EACH_L
double fn_dd(double acc, double x) // REDUCE_D
long   fn_ll(long acc, long x)     // REDUCE_L
double fn_zip(double a, double b)  // ZIP_D
int    pred_d(const double *p)     // FILTER_D
int    pred_l(const long *p)       // FILTER_L

MAP_D(arr, len, fn)                // arr[i] = fn(arr[i])  in place
MAP_D_INTO(src, dst, len, fn)      // dst[i] = fn(src[i])  src unchanged
MAP_L / MAP_L_INTO                 // same for long[]

FILTER_D(src, len, dst, &out_len, pred)
// compact: keep elements where pred(&src[i]) != 0
// dst may alias src — write cursor <= read cursor always

REDUCE_D(arr, len, init, fn)       // fold: acc=init; acc=fn(acc,arr[i]); return acc
REDUCE_L(arr, len, init, fn)       // same for long[]

FOR_EACH_D(arr, len, fn)           // fn(arr[i]) for side effects
FOR_EACH_L(arr, len, fn)

ZIP_D(a, b, out, len, fn)          // out[i] = fn(a[i], b[i])
ZIP_L(a, b, out, len, fn)

STR_FROM_BUF(buf, len)             // char buffer → tiny_str_t (copies if <=12, borrows if >12)
STR_FROM_DOUBLE(x)                 // DTOA result → tiny_str_t
STR_FROM_INT(n)                    // _itoa result → tiny_str_t
```

Example — sum, max, filter positives:

```c
double add(double a, double b) { return a + b; }
double dmax(double a, double b) { return a > b ? a : b; }
int positive(const double *p)  { return *p > 0.0; }

double total   = REDUCE_D(arr, 10, 0.0,    add);
double biggest = REDUCE_D(arr, 10, arr[0], dmax);
long   n_pos;
FILTER_D(arr, 10, out, &n_pos, positive);
```

---

### §18 — Namespaces

Linux namespace isolation via `unshare(2)`.

```c
UNSHARE(flags)      // returns 0 on success, -errno on fail
                    // unprivileged process can use CLONE_NEWUSER alone

// flags (combinable with |):
CLONE_NEWUSER       // new user namespace. Remap uid/gid → fake root inside.
CLONE_NEWNS         // new mount namespace
CLONE_NEWPID        // new PID namespace. Fork after to become PID 1 in child.
CLONE_NEWNET        // new network namespace. Only loopback survives.
CLONE_NEWUTS        // new UTS namespace. Independent hostname.
CLONE_NEWIPC        // new IPC namespace. Isolated SysV IPC + POSIX MQ.

WRITE_IDMAP(path, content, len)   // write uid_map/gid_map/setgroups files

GETUID()            // getuid syscall → long
GETGID()            // getgid syscall → long
```

Sequence for unprivileged user namespace:

```c
UNSHARE(CLONE_NEWUSER);
WRITE_IDMAP("/proc/self/setgroups", "deny",    4);
WRITE_IDMAP("/proc/self/uid_map",   "0 1000 1", 9);   // map uid 1000 → root
WRITE_IDMAP("/proc/self/gid_map",   "0 1000 1", 9);
// now getuid() returns 0 inside namespace
```

`BEGIN_ISOLATED` does this automatically with current uid/gid.

**`CLONE_NEWPID` note:** calling process keeps original PID.
Fork after `UNSHARE(CLONE_NEWPID)` — child becomes PID 1 in new namespace.

---

### §19 — Bump Allocator

Simpler than slab. One `SBRK` call. Pointer walks forward. Free = reset everything.

```c
BumpAlloc b;
BUMP_INIT(&b, total_bytes)      // claim total_bytes from brk. Returns -1 on fail.
                                // total_bytes auto-aligned via ALIGN_UP.

void *p = BUMP_ALLOC(&b, size)  // return aligned ptr, advance cursor. NULL if full.
                                // size auto-aligned. Different sizes allowed.

BUMP_RESET(&b)                  // cursor = base. Frees everything. O(1).
BUMP_USED(&b)                   // bytes allocated (long)
BUMP_LEFT(&b)                   // bytes remaining (long)
```

Use slab when: all allocations same size, need individual free.
Use bump when: many different sizes, free everything at once (e.g. per-level game data).
Use neither when: you need `realloc`. You are on your own then.

---

### §20 — Terminal & Game Loop

Raw terminal mode + keyboard polling. No ncurses. Direct `ioctl` + `poll` syscalls.

```c
TERM_RAW()          // disable canonical mode + echo. Save previous termios.
                    // Disables: ICANON, ECHO, ECHOE, ECHOK, ISIG, IXON
                    // Sets VMIN=1 VTIME=0 (block until 1 char)
                    // Returns 0 on success, -1 on fail.

TERM_RESET()        // restore saved termios. Call before EXIT. Always.

TERM_NONBLOCK()     // VMIN=0 VTIME=0: READ returns immediately, 0 if no key.
                    // Call after TERM_RAW for game loop use.

POLL_KEY(ms)        // 1 = key available within ms milliseconds
                    // 0 = timeout   -1 = error
                    // ms=0: instant check   ms=-1: block forever

READ_KEY()          // read one char. Returns int (0-255) or -1 if no data.
                    // Call TERM_RAW + TERM_NONBLOCK first.

// ANSI terminal control
CLEAR_SCREEN()              // ESC[2J ESC[H — clear + home cursor
MOVE_CURSOR(row, col)       // ESC[row;colH — 1-indexed
CURSOR_HIDE()               // ESC[?25l
CURSOR_SHOW()               // ESC[?25h

// key constants
KEY_ESC             27
KEY_UP              'A'     // read AFTER ESC + '['
KEY_DOWN            'B'
KEY_RIGHT           'C'
KEY_LEFT            'D'
KEY_ENTER           '\n'
KEY_SPACE           ' '
KEY_CTRL(c)         ((c) & 0x1F)    // e.g. KEY_CTRL('c') == 3

CONSUME_ESCAPE()    // read '[' + final byte after ESC. Returns final byte or -1.
```

Arrow keys are 3-byte sequences: `27 '[' 'A'/'B'/'C'/'D'`.
When `READ_KEY()` returns `KEY_ESC`, call `CONSUME_ESCAPE()` to get direction.

Full game loop pattern:

```c
BEGIN
    TERM_RAW();
    TERM_NONBLOCK();
    CURSOR_HIDE();

    int px = 10, py = 10, running = 1;

    while (running) {
        CLEAR_SCREEN();
        MOVE_CURSOR(py, px);
        PRINT_STR("@", 1);

        if (POLL_KEY(33)) {         // 33ms ≈ 30fps
            int k = READ_KEY();
            if (k == 'q')               running = 0;
            if (k == 'w' || k == KEY_ESC && CONSUME_ESCAPE() == KEY_UP)    py--;
            if (k == 's')               py++;
            if (k == 'a')               px--;
            if (k == 'd')               px++;
        }
    }

    CURSOR_SHOW();
    TERM_RESET();
    CLEAR_SCREEN();
END
```

**Always call `TERM_RESET()` before `EXIT`.**
Terminal stays raw if program crashes without resetting. Shell becomes unusable.
If this happens: type `reset` blind and press Enter.

---

## COMMON MISTAKES

| Mistake | Result | Fix |
|---|---|---|
| Use `%` operator with `-nostdlib` | segfault (calls `__moddi3`) | use `& 1` or shifts |
| Use `/` on integers with `-nostdlib` | segfault (calls `__divdi3`) | only powers of 2 via shifts |
| Wrong length in `PRINT_STR` | garbage output or segfault | count exact or use `STRLEN` |
| `PUSH_FRAME`/`POP_FRAME` in normal fn | GCC error | naked fns only |
| `S_PTR` on stack buffer, return `tiny_str_t` | dangling pointer | use `STR_FROM_BUF` (copies if ≤12) |
| Forget `TERM_RESET()` before crash | shell goes raw | type `reset` + Enter |
| `-Wl,-N` with custom linker script | misaligned segments, segfault | remove `-Wl,-N`, `tiny.ld` handles it |
| `CLONE_NEWPID` without fork | process keeps old PID | fork after unshare |
| `FACTORIAL(n)` with n > 20 | silent overflow | keep n ≤ 20 |

---

## STACK LAYOUT DISCIPLINE

 recommend name all stack slots with enum + macros. No magic numbers.

```c
enum {
    OFF_A     = 8,
    OFF_B     = OFF_A + ALIGN_UP(sizeof(double)),
    OFF_C     = OFF_B + ALIGN_UP(sizeof(double)),
    OFF_BUF   = OFF_C + ALIGN_UP(sizeof(double)),   // char buf[16] = 2 doubles worth
    FRAME_SZ  = OFF_BUF + ALIGN_UP(16)
};
#define VAR_A   STACK(OFF_A)
#define VAR_B   STACK(OFF_B)
#define VAR_C   STACK(OFF_C)
#define VAR_BUF STACK(OFF_BUF)
```

Now code reads like intention, not offsets. Refactor = change enum, macros follow.

---

## LICENSE

AGPLv3. See LICENSE file.

 note: AGPLv3 means if you use this in network service, you share source.
 think fair.  not hide knowledge.

---

## BUG REPORTS

Bugs probably everywhere. Report useful.
Fixes not guaranteed. Responses may be blunt. Regressions possible.

**Bug bounty:** vague appreciation. Maybe acknowledgment.  is poor.

---

## CONTRIBUTIONS

Accepted under strict conditions:

- Follow existing style exactly (ALL_CAPS public API, `_lowercase` internal)
- No new abstractions unless absolutely necessary
- Changes minimal and focused
- Expect rejection even if code correct

Project has strong opinionated direction.  know what  want.

---

*"Complexity bad. Pointer good. Syscall friend. Linker enemy until understood."*
