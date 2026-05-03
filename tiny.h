#pragma once

/* ─── arch guard ─────────────────────────────────────────────────────────── */
#if !defined(__x86_64__)
#  error "tiny.h: x86-64 only"
#endif
#if !defined(__linux__)
#  error "tiny.h: Linux only"
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * TINY.H — no-libc x86-64 Linux toolkit
 *
 * Philosophy: data as truth, code as pointer arithmetic.
 *             no branches where masks suffice.
 *             no libc, no headers, no runtime.
 *
 * Sections:
 *   1.  syscall numbers
 *   2.  alignment & pointer utils
 *   3.  stack frame helpers
 *   4.  xmm scalar f64 arithmetic
 *   5.  f64 comparisons → double flag
 *   6.  f64 control flow
 *   7.  f64 array helpers
 *   8.  integer bit ops
 *   9.  string & memory ops
 *  10.  DTOA / atof / atoi
 *  11.  I/O  (print f64, int, str / read)
 *  12.  brk / slab allocator
 *  13.  tiny_str_t  (German-style SSO string)
 *  14.  math constants & numeric helpers
 *  15.  interactive I/O helpers  (LEER_NUMERO, LEER_LETRA, print_label)
 *  16.  BEGIN / END  (dual-mode entry: main or _start)
 *  17.  functional toolkit  (MAP, FILTER, REDUCE, FOR_EACH, ZIP, STR_FROM_BUF)
 *  18.  namespaces  (UNSHARE, BEGIN_ISOLATED)
 *  19.  bump allocator  (BUMP_INIT, BUMP_ALLOC, BUMP_RESET)
 *  20.  terminal & game loop  (TERM_RAW, TERM_RESET, POLL_KEY, READ_KEY)
 * ═══════════════════════════════════════════════════════════════════════════ */


/* ─── 1. syscall numbers ─────────────────────────────────────────────────── */
#define SYS_READ  0
#define SYS_WRITE 1
#define SYS_EXIT  60
#define SYS_BRK   12
#define STDIN     0
#define STDOUT    1

_Static_assert(SYS_READ  == 0,  "syscall ABI mismatch");
_Static_assert(SYS_WRITE == 1,  "syscall ABI mismatch");
_Static_assert(SYS_EXIT  == 60, "syscall ABI mismatch");
_Static_assert(STDOUT    == 1,  "fd assumption broken");


/* ─── 2. alignment & pointer utils ──────────────────────────────────────── */
#define ALIGN_UP(s)         (((s) + 7) & ~7)   /* round up to 8-byte boundary   */
#define ALIGN_DOWN(s)       ((s) & ~7)          /* round down to 8-byte boundary */

#define OFFSET(ptr, bytes)  ((void*)((char*)(ptr) + (bytes)))
#define INDEX(base, i)      ((void*)((double*)(base) + (i)))
#define DEREF(addr)         (*(double*)(addr))


/* ─── 3. stack frame helpers ─────────────────────────────────────────────── */
/*
 * STACK(off) — void* to [rbp - off] in current frame.
 * REQUIRES valid frame pointer (-fno-omit-frame-pointer).
 * Use inside non-naked fns only (naked fns have no GCC-managed frame).
 */
#define STACK(off) ((void*)((char*)__builtin_frame_address(0) - (off)))

#define PUSH_FRAME(n) \
    asm volatile(                \
        "pushq %%rbp\n"          \
        "movq  %%rsp, %%rbp\n"   \
        "subq  %0, %%rsp"        \
        :: "i"(n) : "rsp", "rbp")

#define POP_FRAME \
    asm volatile(                \
        "movq %%rbp, %%rsp\n"    \
        "popq %%rbp"             \
        ::: "rsp", "rbp")


/* ─── 4. xmm scalar f64 arithmetic ──────────────────────────────────────── */
/*
 * All ops take void* addresses pointing to doubles on the stack/heap.
 * DEF  — *addr  = literal double
 * COPY — *dst   = *src
 * SWAP — swap *a <-> *b           (xmm0/xmm1, no stack temp)
 * INC  — *addr += 1.0
 * DEC  — *addr -= 1.0
 * SUM  — *out   = *a + *b
 * SUB  — *out   = *a - *b
 * MUL  — *out   = *a * *b
 * DIV  — *out   = *a / *b
 * MOD  — *out   = fmod(*a, *b)    (x87 fprem1, no libm)
 * ROOT — *out   = sqrt(*a)        (sqrtsd)
 * ABS  — *out   = |*a|            (andpd sign mask, branchless)
 * NEG  — *out   = -*a             (xorpd sign mask, branchless)
 * MIN  — *out   = min(*a, *b)     (minsd)
 * MAX  — *out   = max(*a, *b)     (maxsd)
 * AVG  — *out   = (*a + *b) * 0.5 (xmm, no overflow)
 */
#define DEF(addr, val) ({                                                      \
    double _v = (val);                                                         \
    asm volatile(                                                              \
        "movsd %1, %%xmm0\n"                                                   \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(addr), "m"(_v) : "xmm0", "memory");                            \
})

#define COPY(dst, src) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(dst), "r"(src) : "xmm0", "memory");                            \
})

#define SWAP(a, b) ({                                                          \
    asm volatile(                                                              \
        "movsd (%0), %%xmm0\n"                                                 \
        "movsd (%1), %%xmm1\n"                                                 \
        "movsd %%xmm1, (%0)\n"                                                 \
        "movsd %%xmm0, (%1)"                                                   \
        :: "r"(a), "r"(b) : "xmm0", "xmm1", "memory");                        \
})

#define INC(addr) ({                                                           \
    static const double _one = 1.0;                                            \
    asm volatile(                                                              \
        "movsd (%0),   %%xmm0\n"                                               \
        "addsd (%1),   %%xmm0\n"                                               \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(addr), "r"(&_one) : "xmm0", "memory");                         \
})

#define DEC(addr) ({                                                           \
    static const double _one = 1.0;                                            \
    asm volatile(                                                              \
        "movsd (%0),   %%xmm0\n"                                               \
        "subsd (%1),   %%xmm0\n"                                               \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(addr), "r"(&_one) : "xmm0", "memory");                         \
})

#define SUM(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "addsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define SUB(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "subsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define MUL(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "mulsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define DIV(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "divsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

/* MOD via x87 fprem1 — iterates until C2 flag clears (full remainder done) */
#define MOD(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "fldl   (%2)\n"                                                        \
        "fldl   (%1)\n"                                                        \
        "1: fprem1\n"                                                          \
        "fnstsw %%ax\n"                                                        \
        "testb  $4, %%ah\n"                                                    \
        "jnz    1b\n"                                                          \
        "fstpl  (%0)\n"                                                        \
        "fstp   %%st(0)"                                                       \
        :: "r"(out), "r"(a), "r"(b) : "ax", "memory");                        \
})

#define ROOT(out, a) ({                                                        \
    asm volatile(                                                              \
        "sqrtsd (%1), %%xmm0\n"                                                \
        "movsd  %%xmm0, (%0)"                                                  \
        :: "r"(out), "r"(a) : "xmm0", "memory");                              \
})

/* ABS: clear IEEE 754 sign bit via andpd with 0x7FFFFFFFFFFFFFFF mask */
#define ABS(out, a) ({                                                         \
    static const long long _absmask = 0x7FFFFFFFFFFFFFFFLL;                   \
    asm volatile(                                                              \
        "movsd  (%1),   %%xmm0\n"                                              \
        "andpd  (%2),   %%xmm0\n"                                              \
        "movsd  %%xmm0, (%0)"                                                  \
        :: "r"(out), "r"(a), "r"(&_absmask) : "xmm0", "memory");              \
})

/* NEG: flip IEEE 754 sign bit via xorpd with 0x8000000000000000 mask */
#define NEG(out, a) ({                                                         \
    static const long long _negmask = (long long)0x8000000000000000LL;        \
    asm volatile(                                                              \
        "movsd  (%1),   %%xmm0\n"                                              \
        "xorpd  (%2),   %%xmm0\n"                                              \
        "movsd  %%xmm0, (%0)"                                                  \
        :: "r"(out), "r"(a), "r"(&_negmask) : "xmm0", "memory");              \
})

#define MIN(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "minsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define MAX(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "maxsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define AVG(out, a, b) ({                                                      \
    static const double _half = 0.5;                                           \
    asm volatile(                                                              \
        "movsd (%1),   %%xmm0\n"                                               \
        "addsd (%2),   %%xmm0\n"                                               \
        "mulsd (%3),   %%xmm0\n"                                               \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b), "r"(&_half) : "xmm0", "memory");         \
})


/* ─── 5. f64 comparisons -> double flag ─────────────────────────────────── */
/*
 * Each macro stores 1.0 (true) or 0.0 (false) at out.
 * Uses ucomisd — NaN-safe (NaN comparisons always yield false).
 *
 * IS_GT(out,a,b) — *a >  *b
 * IS_LT(out,a,b) — *a <  *b
 * IS_GE(out,a,b) — *a >= *b
 * IS_LE(out,a,b) — *a <= *b
 * IS_EQ(out,a,b) — *a == *b
 * IS_NE(out,a,b) — *a != *b
 * CMP(out,a,b)   — -1.0 / 0.0 / 1.0  (less / equal / greater)
 */
#define _CMP_IMPL(out, a, b, setcc) ({                                         \
    static const double _t = 1.0, _f = 0.0;                                   \
    asm volatile(                                                              \
        "movsd   (%1),   %%xmm0\n"                                             \
        "ucomisd (%2),   %%xmm0\n"                                             \
        setcc "  %%al\n"                                                       \
        "testb   %%al,   %%al\n"                                               \
        "jz      1f\n"                                                         \
        "movsd   (%3),   %%xmm1\n"                                             \
        "jmp     2f\n"                                                         \
        "1: movsd (%4),  %%xmm1\n"                                             \
        "2: movsd %%xmm1, (%0)"                                                \
        :: "r"(out), "r"(a), "r"(b), "r"(&_t), "r"(&_f)                       \
        : "xmm0", "xmm1", "rax", "memory");                                   \
})

#define IS_GT(out, a, b)  _CMP_IMPL(out, a, b, "seta")
#define IS_LT(out, a, b)  _CMP_IMPL(out, a, b, "setb")
#define IS_GE(out, a, b)  _CMP_IMPL(out, a, b, "setae")
#define IS_LE(out, a, b)  _CMP_IMPL(out, a, b, "setbe")
#define IS_EQ(out, a, b)  _CMP_IMPL(out, a, b, "sete")
#define IS_NE(out, a, b)  _CMP_IMPL(out, a, b, "setne")

/* CMP: stores -1.0 / 0.0 / 1.0 — useful as sort comparator */
#define CMP(out, a, b) ({                                                      \
    static const double _neg = -1.0, _zer = 0.0, _pos = 1.0;                  \
    asm volatile(                                                              \
        "movsd   (%1),   %%xmm0\n"                                             \
        "ucomisd (%2),   %%xmm0\n"                                             \
        "movsd   (%5),   %%xmm1\n"                                             \
        "ja      1f\n"                                                         \
        "jb      2f\n"                                                         \
        "jmp     3f\n"                                                         \
        "1: movsd (%4),  %%xmm1\n jmp 3f\n"                                   \
        "2: movsd (%3),  %%xmm1\n"                                             \
        "3: movsd %%xmm1, (%0)"                                                \
        :: "r"(out), "r"(a), "r"(b),                                           \
           "r"(&_neg), "r"(&_pos), "r"(&_zer)                                  \
        : "xmm0", "xmm1", "memory");                                           \
})


/* ─── 6. f64 control flow ────────────────────────────────────────────────── */
/*
 * FOR_RANGE(idx_addr, start, end)
 *   — idx_addr: void* to a double on the stack used as loop counter
 *   — iterates while *idx < end, INC each iteration
 *   — use: FOR_RANGE(STACK(8), 0.0, 10.0) { ... }
 *
 * FOR_DOWN(idx_addr, start, end)
 *   — iterates while *idx > end, DEC each iteration
 *
 * WHILE_NZ(flag_addr)
 *   — loops while *(double*)flag_addr != 0.0
 *   — set to 0.0 inside body to exit
 *
 * IF_GT/LT/GE/LE/EQ/NE(a, b) { ... }
 *   — conditional block on two void* addr values via DEREF
 */
#define FOR_RANGE(idx_addr, start, end) \
    for (DEF((idx_addr), (start));      \
         DEREF(idx_addr) < (end);       \
         INC(idx_addr))

#define FOR_DOWN(idx_addr, start, end)  \
    for (DEF((idx_addr), (start));      \
         DEREF(idx_addr) > (end);       \
         DEC(idx_addr))

#define WHILE_NZ(flag_addr) \
    while (DEREF(flag_addr) != 0.0)

#define IF_GT(a, b)  if (DEREF(a) >  DEREF(b))
#define IF_LT(a, b)  if (DEREF(a) <  DEREF(b))
#define IF_GE(a, b)  if (DEREF(a) >= DEREF(b))
#define IF_LE(a, b)  if (DEREF(a) <= DEREF(b))
#define IF_EQ(a, b)  if (DEREF(a) == DEREF(b))
#define IF_NE(a, b)  if (DEREF(a) != DEREF(b))


/* ─── 7. f64 array helpers ───────────────────────────────────────────────── */
/*
 * FILL(base, count, val) — set count doubles starting at base to val
 * ZERO(addr)             — zero one double slot
 * ARRAY_GET(base,i,dst)  — dst = base[i]
 * ARRAY_SET(base,i,src)  — base[i] = src
 */
#define FILL(base, count, val) ({                                              \
    double  _fv = (double)(val);                                               \
    double *_fp = (double*)(base);                                             \
    for (long _fi = 0; _fi < (long)(count); _fi++) _fp[_fi] = _fv;            \
})

#define ZERO(addr)              DEF((addr), 0.0)
#define ARRAY_GET(base, i, dst) COPY((dst),            INDEX((base), (i)))
#define ARRAY_SET(base, i, src) COPY(INDEX((base),(i)), (src))


/* ─── 8. integer bit ops ─────────────────────────────────────────────────── */
/*
 * All operate on unsigned long (64-bit) unless noted.
 * Branchless where possible — masks, not jumps.
 *
 * IABS(x)          — |x|, no branch. mask = x>>63; (x+mask)^mask
 * IMIN(x,y)        — branchless min
 * IMAX(x,y)        — branchless max
 * IAVG(x,y)        — (x&y)+((x^y)>>1), no overflow
 * IS_POW2(x)       — 1 if x is power of 2 (x > 0 required)
 * IS_OPP_SIGN(x,y) — 1 if x and y have opposite signs
 * NEXT_POW2(v)     — next power of 2 >= v (32-bit)
 * POPCOUNT(v)      — count set bits, parallel method (32-bit, 12 ops)
 * TRAILING_ZEROS(v)— trailing zero count, DeBruijn multiply (32-bit, 4 ops)
 * MERGE(a,b,mask)  — (a&~mask)|(b&mask), branchless bit-select
 * BIT_SET(x,n)     — set bit n
 * BIT_CLR(x,n)     — clear bit n
 * BIT_TST(x,n)     — test bit n (nonzero if set)
 * BIT_FLP(x,n)     — flip bit n
 * COND_SET(w,m,f)  — if f: w|=m else w&=~m, branchless
 */
#define IABS(x)           ({ long _x=(long)(x); long _m=_x>>63; (_x+_m)^_m; })
#define IMIN(x,y)         ({ long _x=(long)(x),_y=(long)(y); _y^((_x^_y)&-((_x)<(_y))); })
#define IMAX(x,y)         ({ long _x=(long)(x),_y=(long)(y); _x^((_x^_y)&-((_x)<(_y))); })
#define IAVG(x,y)         ({ unsigned long _x=(x),_y=(y); (_x&_y)+((_x^_y)>>1); })
#define IS_POW2(x)        ({ unsigned long _x=(x); (_x && !(_x&(_x-1))); })
#define IS_OPP_SIGN(x,y)  (((long)(x)^(long)(y)) < 0)
#define MERGE(a,b,mask)   ((a)^(((a)^(b))&(mask)))
#define BIT_SET(x,n)      ((x) |=  (1UL<<(n)))
#define BIT_CLR(x,n)      ((x) &= ~(1UL<<(n)))
#define BIT_TST(x,n)      ((x) &   (1UL<<(n)))
#define BIT_FLP(x,n)      ((x) ^=  (1UL<<(n)))
#define COND_SET(w,m,f)   ((w) = ((w)&~(m))|(-(unsigned long)(f)&(m)))

/* NEXT_POW2 — round 32-bit v up to next power of 2 (0 stays 0) */
#define NEXT_POW2(v) ({                                                        \
    unsigned int _v = (unsigned int)(v);                                       \
    _v--; _v|=_v>>1; _v|=_v>>2; _v|=_v>>4; _v|=_v>>8; _v|=_v>>16; _v++;     \
    _v;                                                                        \
})

/*
 * POPCOUNT — parallel Hamming weight, 32-bit.
 * 12 ops, no table, no 64-bit multiply needed.
 */
#define POPCOUNT(v) ({                                                         \
    unsigned int _v = (unsigned int)(v);                                       \
    _v  = _v - ((_v >> 1) & 0x55555555U);                                     \
    _v  = (_v & 0x33333333U) + ((_v >> 2) & 0x33333333U);                     \
    _v  = ((_v + (_v >> 4)) & 0x0F0F0F0FU);                                   \
    (int)((_v * 0x01010101U) >> 24);                                           \
})

/*
 * TRAILING_ZEROS — count trailing zero bits, 32-bit.
 * DeBruijn multiply + lookup: 4 ops, no branch.
 * Returns 32 if v == 0.
 */
#define TRAILING_ZEROS(v) ({                                                   \
    static const int _db32[32] = {                                             \
         0, 1,28, 2,29,14,24, 3,30,22,20,15,25,17, 4, 8,                      \
        31,27,13,23,21,19,16, 7,26,12,18, 6,11, 5,10, 9                        \
    };                                                                         \
    unsigned int _v = (unsigned int)(v);                                       \
    _v ? _db32[((unsigned int)((_v & -_v) * 0x077CB531U)) >> 27] : 32;        \
})


/* ─── 9. string & memory ops ─────────────────────────────────────────────── */
/*
 * STRLEN(s)
 *   — length of NUL-terminated string via repne scasb.
 *     One asm instruction, no C loop.
 *
 * MEM_COPY(dst, src, n)
 *   — copy n bytes via rep movsb.
 *     No alignment requirement. Use to move data between STACK and slab.
 *
 * NTH(arr, idx, len)
 *   — safe indexed access into any pointer array (char**, void**, etc.)
 *   — returns arr[idx] if 0 <= idx < len, else NULL.
 *   — NULL = explicit "no value" sentinel. 0 is a valid index, NULL is not.
 *   — unsigned cast trick: negative idx wraps to huge value -> OOB -> NULL.
 *     Single comparison, no branch at call site with -O2.
 *
 * FIND_IF(base, count, stride, pred)
 *   — linear scan. Returns void* to first element where pred(ptr) != 0.
 *   — Returns NULL if not found.
 *   — stride = sizeof(element), pred = int(*)(void*)
 *
 * Example — day lookup (your Python one-liner, in C, no libc):
 *
 *   static const char *_semana[] = {
 *       "LUNES","MARTES","MIERCOLES","JUEVES","VIERNES","SABADO","DOMINGO"
 *   };
 *   char buf[16]; READ(buf, sizeof(buf));
 *   long n         = ATOI(buf, NULL) - 1;
 *   const char *dia = NTH(_semana, n, 7);
 *   if (dia) PRINTLN_STR(dia, STRLEN(dia));
 *   else     PRINTLN_STR("ERROR", 5);
 */

 long tiny_strlen(const char *s) {
    long len;
    asm volatile(
        "cld\n"
        "xorb  %%al,  %%al\n"      /* search for NUL */
        "movq  $-1,   %%rcx\n"     /* max scan length */
        "repne scasb\n"            /* scan until [rdi] == 0 */
        "notq  %%rcx\n"
        "decq  %%rcx"              /* rcx = length excluding NUL */
        : "=c"(len)
        : "D"(s)
        : "rax", "memory");
    return len;
}
#define STRLEN(s) tiny_strlen(s)

 void tiny_memcopy(void *dst, const void *src, long n) {
    asm volatile(
        "cld\n"
        "rep movsb"
        : "+D"(dst), "+S"(src), "+c"(n)
        :: "memory");
}
#define MEM_COPY(dst, src, n) tiny_memcopy((dst), (src), (n))

#define NTH(arr, idx, len) \
    ((unsigned long)(idx) < (unsigned long)(len) ? (arr)[(idx)] : (void*)0)

 void *tiny_find_if(void *base, long count, long stride,
                                  int (*pred)(void *)) {
    char *p = (char *)base;
    for (long i = 0; i < count; i++, p += stride)
        if (pred(p)) return (void *)p;
    return (void *)0;
}
#define FIND_IF(base, count, stride, pred) \
    tiny_find_if((base), (count), (stride), (pred))


/* ─── 10. DTOA / atof / atoi ─────────────────────────────────────────────── */
/*
 * DTOA(x, out)          — double -> decimal string. No NUL. Returns length.
 *                         out must be >= 32 bytes.
 *                         Integer part via x87 fbstp (BCD). Trailing zeros stripped.
 * ATOF(s, end_ptr) — decimal string -> double. Skips leading whitespace.
 * ATOI(s, end_ptr) — decimal string -> long.  Skips leading whitespace.
 *                         Both set *end_ptr to first unparsed char (pass NULL to ignore).
 */
 int DTOA(double x, char *out) {
    char *start = out;
    if (x < 0.0) { *out++ = '-'; x = -x; }
    unsigned long i = (unsigned long)x;
    double f = x - (double)i;
    unsigned char bcd[10];
    asm volatile("fildq %1\nfbstp %0" : "=m"(bcd) : "m"(i));
    int leading = 1;
    for (int j = 8; j >= 0; j--) {
        unsigned char hi = (bcd[j] >> 4) & 0xF;
        unsigned char lo =  bcd[j]       & 0xF;
        if (hi || !leading) { *out++ = '0' + hi; leading = 0; }
        if (lo || !leading) { *out++ = '0' + lo; leading = 0; }
    }
    if (leading) *out++ = '0';
    char frac[15]; int last_nonzero = -1;
    for (int j = 0; j < 15; j++) {
        f *= 10.0; int d = (int)f;
        frac[j] = '0' + d; f -= (double)d;
        if (d) last_nonzero = j;
    }
    if (last_nonzero >= 0) {
        *out++ = '.';
        for (int j = 0; j <= last_nonzero; j++) *out++ = frac[j];
    }
    return (int)(out - start);
}

 double ATOF(const char *s, const char **end_ptr) {
    double result = 0.0, frac = 1.0; int neg = 0, in_frac = 0;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    if (*s == '-') { neg = 1; s++; }
    for (; *s; s++) {
        if      (*s >= '0' && *s <= '9') {
            if (in_frac) { frac *= 0.1; result += (*s - '0') * frac; }
            else         { result = result * 10.0 + (*s - '0'); }
        } else if (*s == '.') { in_frac = 1; }
        else break;
    }
    if (end_ptr) *end_ptr = s;
    return neg ? -result : result;
}

 long ATOI(const char *s, const char **end_ptr) {
    long result = 0; int neg = 0;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { result = result * 10 + (*s - '0'); s++; }
    if (end_ptr) *end_ptr = s;
    return neg ? -result : result;
}


/* ─── 11. I/O ────────────────────────────────────────────────────────────── */
/*
 * READ(buf,len)          — read stdin -> buf. Returns bytes read (int).
 * PRINT(addr)            — print *(double*)addr
 * PRINTLN(addr)          — print *(double*)addr + newline
 * PRINT_INT(n)           — print signed long
 * PRINTLN_INT(n)         — print signed long + newline
 * PRINT_STR(ptr,len)     — write raw bytes  (use STRLEN for len)
 * PRINTLN_STR(ptr,len)   — write raw bytes + newline
 * EXIT(code)             — _exit syscall, noreturn
 */
#define READ(buf, len) ({                           \
    register long  _rax asm("rax") = SYS_READ;     \
    register long  _rdi asm("rdi") = STDIN;         \
    register char *_rsi asm("rsi") = (char*)(buf);  \
    register long  _rdx asm("rdx") = (long)(len);   \
    asm volatile("syscall"                          \
        : "+r"(_rax)                               \
        : "r"(_rdi), "r"(_rsi), "r"(_rdx)         \
        : "rcx", "r11", "memory");                 \
    (int)_rax;                                     \
})

#define _WRITE_IMPL(ptr, len) ({                                               \
    register long   _rax asm("rax") = SYS_WRITE;                              \
    register long   _rdi asm("rdi") = STDOUT;                                  \
    register char  *_rsi asm("rsi") = (char*)(ptr);                            \
    register long   _rdx asm("rdx") = (long)(len);                             \
    asm volatile("syscall"                                                     \
        : "+r"(_rax)                                                           \
        : "r"(_rdi), "r"(_rsi), "r"(_rdx)                                     \
        : "rcx", "r11", "memory");                                             \
})

#define _PRINT_IMPL(addr, nl) ({                                               \
    char _buf[32]; int _len = DTOA(*(double*)(addr), _buf);                    \
    if (nl) { _buf[_len++] = '\n'; }                                           \
    _WRITE_IMPL(_buf, _len);                                                   \
})
#define PRINT(addr)   _PRINT_IMPL(addr, 0)
#define PRINTLN(addr) _PRINT_IMPL(addr, 1)

/* integer -> decimal via x87 BCD, same engine as DTOA, integer part only */
 int _itoa(long x, char *out) {
    char *start = out;
    if (x < 0) { *out++ = '-'; x = -x; }
    unsigned long u = (unsigned long)x;
    unsigned char bcd[10];
    asm volatile("fildq %1\nfbstp %0" : "=m"(bcd) : "m"(u));
    int leading = 1;
    for (int j = 8; j >= 0; j--) {
        unsigned char hi = (bcd[j] >> 4) & 0xF;
        unsigned char lo =  bcd[j]       & 0xF;
        if (hi || !leading) { *out++ = '0' + hi; leading = 0; }
        if (lo || !leading) { *out++ = '0' + lo; leading = 0; }
    }
    if (leading) *out++ = '0';
    return (int)(out - start);
}

#define _PRINT_INT_IMPL(n, nl) ({                                              \
    char _ibuf[24]; int _ilen = _itoa((long)(n), _ibuf);                       \
    if (nl) { _ibuf[_ilen++] = '\n'; }                                         \
    _WRITE_IMPL(_ibuf, _ilen);                                                 \
})
#define PRINT_INT(n)   _PRINT_INT_IMPL(n, 0)
#define PRINTLN_INT(n) _PRINT_INT_IMPL(n, 1)

#define _PRINT_STR_IMPL(ptr, len, nl) ({                                       \
    _WRITE_IMPL((ptr), (len));                                                 \
    if (nl) { char _nl = '\n'; _WRITE_IMPL(&_nl, 1); }                        \
})
#define PRINT_STR(ptr, len)   _PRINT_STR_IMPL(ptr, len, 0)
#define PRINTLN_STR(ptr, len) _PRINT_STR_IMPL(ptr, len, 1)

#define EXIT(code) ({                                                          \
    register long _rax asm("rax") = SYS_EXIT;                                 \
    register long _rdi asm("rdi") = (code);                                   \
    asm volatile("syscall" :: "r"(_rax), "r"(_rdi));                          \
    __builtin_unreachable();                                                   \
})


/* ─── 12. brk / slab allocator ───────────────────────────────────────────── */
/*
 * tiny_brk(addr)  — raw brk syscall. Pass NULL to query current brk.
 * BRK_GET()       — query current program break
 * BRK_SET(addr)   — set program break
 * SBRK(n)    — grow heap by n bytes. Returns old brk (base of new region).
 *                   Returns (void*)-1 on failure.
 *
 * SlabPool — O(1) fixed-size allocator on top of brk heap.
 *   SLAB_INIT(pool, slot_size, total_slots)
 *     — one brk call total. slot_size clamped to sizeof(SlabSlot) min,
 *       then ALIGN_UP'd so xmm doubles stored in slots stay aligned.
 *     — builds free list in O(n).
 *   SLAB_ALLOC(pool) — pop head. O(1). Returns NULL if exhausted.
 *   SLAB_FREE(pool, ptr) — push back. O(1). No bounds check.
 */
 void *tiny_brk(void *addr) {
    register long  _rax asm("rax") = SYS_BRK;
    register void *_rdi asm("rdi") = addr;
    asm volatile("syscall"
        : "+r"(_rax) : "r"(_rdi) : "rcx", "r11", "memory");
    return (void *)_rax;
}

#define BRK_GET()     tiny_brk((void*)0)
#define BRK_SET(addr) tiny_brk((void*)(addr))

 void *SBRK(long n) {
    void *cur = BRK_GET();
    void *new = BRK_SET((char*)cur + n);
    return (new == cur) ? (void*)-1L : cur;
}

typedef struct SlabSlot { struct SlabSlot *next; } SlabSlot;

typedef struct {
    void     *mem_base;
    long      slot_size;
    long      total_slots;
    SlabSlot *free_list;
} SlabPool;

 int SLAB_INIT(SlabPool *pool, long slot_size, long total_slots) {
    if (slot_size < (long)sizeof(SlabSlot)) slot_size = sizeof(SlabSlot);
    slot_size = ALIGN_UP(slot_size);
    long  bytes = slot_size * total_slots;
    void *base  = SBRK(bytes);
    if (base == (void*)-1L) return -1;
    pool->mem_base    = base;
    pool->slot_size   = slot_size;
    pool->total_slots = total_slots;
    pool->free_list   = (SlabSlot*)0;
    char *p = (char*)base + bytes - slot_size;
    for (long i = 0; i < total_slots; i++, p -= slot_size) {
        SlabSlot *s     = (SlabSlot*)p;
        s->next         = pool->free_list;
        pool->free_list = s;
    }
    return 0;
}

 void *SLAB_ALLOC(SlabPool *pool) {
    SlabSlot *s = pool->free_list;
    if (!s) return (void*)0;
    pool->free_list = s->next;
    return (void*)s;
}

 void SLAB_FREE(SlabPool *pool, void *ptr) {
    SlabSlot *s     = (SlabSlot*)ptr;
    s->next         = pool->free_list;
    pool->free_list = s;
}


/* ─── 13. tiny_str_t  (German-style SSO string) ──────────────────────────
 *
 * Layout (16 bytes total, always):
 *
 *   Inlined  (built with S()):
 *     [ uint32_t len (bit31=0) | char data[12] ]
 *
 *   Pointer  (built with S_PTR() / S_VIEW() / STR_SLICE()):
 *     [ uint32_t len (bit31=1) | char prefix[4] | char *ptr ]
 *       bit31 of length = HEAP flag. Real length = len & 0x7FFFFFFF.
 *       prefix = first min(4,len) bytes, for fast EQ rejection.
 *       ptr    = borrowed pointer, not owned.
 *
 * Why bit31 flag? Disambiguates inline vs heap regardless of string length.
 *   Previous design used len<=12 as the discriminant — broken for short
 *   heap strings (slice, S_PTR of 1-char string, etc).
 * Why prefix? Fast rejection in STR_EQ without a cache miss on ptr.
 * No NUL stored. Length always explicit.
 * No alloc. No ownership. Caller manages pointer lifetime.
 * ─────────────────────────────────────────────────────────────────────────── */

#include <stdint.h>

#define _TINY_STR_HEAP_FLAG  ((uint32_t)0x80000000u)
#define _TINY_STR_LEN_MASK   ((uint32_t)0x7FFFFFFFu)

typedef struct {
    union {
        struct {
            uint32_t length;        /* bit31=0 → inline */
            char     data[12];
        } inlined;
        struct {
            uint32_t length;        /* bit31=1 → heap */
            char     prefix[4];
            char    *ptr;
        } heap;
    };
} tiny_str_t;

/* ── discriminant & accessors ── */
#define STR_IS_INLINED(s)   (!((s).inlined.length & _TINY_STR_HEAP_FLAG))
#define STR_LEN(s)          ((s).inlined.length & _TINY_STR_LEN_MASK)
#define STR_DATA(s) \
    (STR_IS_INLINED(s) ? (const char*)(s).inlined.data \
                       : (const char*)(s).heap.ptr)

/* ── compile-time literal constructor (inline path) ──
 * Hard error if literal > 12 chars. Use S_PTR() for longer strings.
 * bit31 left 0 — inline flag.
 */
#define S(lit) (__extension__({                                                \
    _Static_assert(sizeof(lit)-1 <= 12,                                        \
        "S(): literal exceeds 12 chars max inline. Use S_PTR().");             \
    (tiny_str_t){ .inlined = {                                                 \
        .length = (uint32_t)(sizeof(lit)-1),   /* bit31=0, always inline */    \
        .data   = lit                                                          \
    }};                                                                        \
}))

/* ── runtime pointer constructor (heap path) ──
 * Sets bit31. Copies first min(len,4) bytes to prefix.
 * Does NOT copy string data. ptr must outlive this tiny_str_t.
 */
 tiny_str_t S_PTR(const char *ptr, uint32_t len) {
    tiny_str_t s;
    s.heap.length = len | _TINY_STR_HEAP_FLAG;   /* set heap flag */
    uint32_t plen = len < 4 ? len : 4;
    for (uint32_t i = 0;    i < plen; i++) s.heap.prefix[i] = ptr[i];
    for (uint32_t i = plen; i < 4;    i++) s.heap.prefix[i] = 0;
    s.heap.ptr = (char *)ptr;
    return s;
}

#define S_VIEW(ptr, len) S_PTR((ptr), (len))

/* ── equality ──────────────────────────────────────────────────────────────
 * len mismatch       → false (no data touch)
 * prefix mismatch    → false (one uint32 load from each, no ptr deref)
 * prefix match       → rep cmpsb on full data
 * Copy to locals: macro args may be temporaries — &(expr) is not an lvalue.
 */
 int _tiny_str_memeq(const char *a, const char *b, long n) {
    int result;
    asm volatile(
        "cld\n"
        "repe cmpsb\n"
        "sete %b0"
        : "=r"(result), "+S"(a), "+D"(b), "+c"(n)
        :: "memory");
    return result;
}

#define STR_EQ(s1, s2) ({                                                      \
    tiny_str_t _a = (s1), _b = (s2);                                           \
    int _eq = 0;                                                               \
    if (STR_LEN(_a) == STR_LEN(_b)) {                                          \
        /* bytes [4..7] = prefix in both arms (same offset after length).     \
           uint32 load = compare first 4 chars in one instruction. */          \
        uint32_t _p1, _p2;                                                     \
        __builtin_memcpy(&_p1, (const char*)&_a + 4, 4);                      \
        __builtin_memcpy(&_p2, (const char*)&_b + 4, 4);                      \
        if (_p1 == _p2)                                                        \
            _eq = _tiny_str_memeq(STR_DATA(_a), STR_DATA(_b),                 \
                                  (long)STR_LEN(_a));                          \
    }                                                                          \
    _eq;                                                                       \
})

/* ── compare against string literal ── */
#define STR_EQ_LIT(s, lit) ({                                                  \
    _Static_assert(sizeof(lit)-1 <= 12,                                        \
        "STR_EQ_LIT: literal > 12 chars. Use S_PTR() + STR_EQ().");           \
    uint32_t _ll = (uint32_t)(sizeof(lit)-1);                                  \
    int _eq = 0;                                                               \
    if (STR_LEN(s) == _ll)                                                     \
        _eq = _tiny_str_memeq(STR_DATA(s), (lit), (long)_ll);                 \
    _eq;                                                                       \
})

/* ── starts with literal ── */
#define STR_STARTS_WITH(s, lit) ({                                             \
    _Static_assert(sizeof(lit)-1 <= 12,                                        \
        "STR_STARTS_WITH: literal > 12 chars.");                               \
    uint32_t _pl = (uint32_t)(sizeof(lit)-1);                                  \
    int _sw = 0;                                                               \
    if (STR_LEN(s) >= _pl)                                                     \
        _sw = _tiny_str_memeq(STR_DATA(s), (lit), (long)_pl);                 \
    _sw;                                                                       \
})

/* ── slice — pointer-mode view, no alloc ──
 * Borrows from s. s must outlive the slice.
 * start >= len → empty string.
 */
 tiny_str_t STR_SLICE(tiny_str_t s, uint32_t start, uint32_t n) {
    uint32_t slen = STR_LEN(s);
    if (start >= slen) return S_PTR("", 0);
    uint32_t avail = slen - start;
    uint32_t take  = n < avail ? n : avail;
    return S_PTR(STR_DATA(s) + start, take);   /* always heap flag → no confusion */
}

/* ── find byte — repne scasb, returns index or -1 ── */
 long STR_FIND_BYTE(tiny_str_t s, char byte) {
    uint32_t len = STR_LEN(s);
    if (!len) return -1L;
    const char *start = STR_DATA(s);
    const char *p     = start;
    long remaining    = (long)len;
    asm volatile(
        "cld\n"
        "repne scasb"
        : "+D"(p), "+c"(remaining)
        : "a"((int)(unsigned char)byte)
        : "memory");
    /* repne scasb stops AFTER the matching byte (rdi points one past).
     * remaining was decremented once extra after match.
     * Not found: remaining reaches 0 with ZF clear.
     * Use sete on ZF to detect match cleanly. */
    int found;
    asm volatile("sete %b0" : "=r"(found) :: "cc");
    if (!found) return -1L;
    /* p now points one past the match; subtract start to get index */
    return (long)(p - start) - 1L;
}

/* ── I/O ── */
#define STR_PRINT(s)   _WRITE_IMPL(STR_DATA(s), STR_LEN(s))
#define STR_PRINTLN(s) ({                                                      \
    _WRITE_IMPL(STR_DATA(s), STR_LEN(s));                                      \
    char _nl = '\n'; _WRITE_IMPL(&_nl, 1);                                     \
})


/* ─── 14. math constants & numeric helpers ───────────────────────────────
 *
 * PI          — Milü approximation 355/113 (~3.14159292). No libm needed.
 *               Error < 3e-7. Fine for homework and most geometry.
 * TAU         — 2*PI
 * EULER       — e ≈ 2.718281828 (rational approx 878/323)
 *
 * CLAMP(v,lo,hi)    — lo if v<lo, hi if v>hi, else v  (double)
 * LERP(a,b,t)       — linear interpolation: a + t*(b-a)
 * SIGN_IDX(n)       — 0 if n<0, 1 if n==0, 2 if n>0  (index into 3-elem array)
 * INT_POW(base,exp) — base^exp by repeated multiply (long, exp >= 0)
 * GAUSS(n)          — n*(n+1)/2, sum of 1..n  (long)
 * FACTORIAL(n)      — n! iterative  (long, n <= 20 to avoid overflow)
 * IS_LEAP(y)        — 1 if y is a leap year  (long)
 * SWAP_CHARS(a,b)   — swap two char variables in place (XOR, no temp)
 * ─────────────────────────────────────────────────────────────────────────── */

#define PI      (355.0 / 113.0)
#define TAU     (710.0 / 113.0)
#define EULER   (878.0 / 323.0)

#define CLAMP(v, lo, hi) \
    ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

#define LERP(a, b, t) \
    ((a) + (t) * ((b) - (a)))

/*
 * SIGN_IDX — maps sign to array index 0/1/2.
 * Pattern from practica.c ej9 & ej24:
 *   double sums[3]; sums[SIGN_IDX(n)] += n;
 *   0 = negative, 1 = zero, 2 = positive
 */
#define SIGN_IDX(n) ((int)((n) > 0.0) - (int)((n) < 0.0) + 1)

/*
 * INT_POW — integer base raised to non-negative integer exponent.
 * Uses long. base^0 = 1. No overflow check — caller's problem.
 */
#define INT_POW(base, exp) ({                                                  \
    long _b = (long)(base), _e = (long)(exp), _r = 1L;                        \
    for (long _i = 0; _i < _e; _i++) _r *= _b;                                \
    _r;                                                                        \
})

/* GAUSS — closed-form sum 1..n. One multiply, one shift. */
#define GAUSS(n) (((long)(n) * ((long)(n) + 1L)) / 2L)

/* FACTORIAL — iterative n!. Overflows long for n > 20. */
#define FACTORIAL(n) ({                                                        \
    long _n = (long)(n), _f = 1L;                                             \
    for (long _i = 2L; _i <= _n; _i++) _f *= _i;                              \
    _f;                                                                        \
})

/*
 * IS_LEAP — Gregorian leap year rule:
 *   divisible by 4 AND (not divisible by 100 OR divisible by 400)
 */
#define IS_LEAP(y) ({                                                          \
    long _y = (long)(y);                                                       \
    int _p4  = (_y % 4   == 0);                                                \
    int _p100 = (_y % 100 == 0);                                               \
    int _p400 = (_y % 400 == 0);                                               \
    _p4 && (!_p100 || _p400);                                                  \
})

/* SWAP_CHARS — XOR swap, no temp, no branch */
#define SWAP_CHARS(a, b) ({ (a) ^= (b); (b) ^= (a); (a) ^= (b); })


/* ─── 15. interactive I/O helpers ────────────────────────────────────────
 *
 * These wrap the byte-at-a-time read-until-newline pattern from practica.c.
 * They use only READ + ATOF + PRINT_STR — no libc.
 *
 * LEER_NUMERO(msg)      — print prompt, read line, return double
 * LEER_LETRA(msg)       — print prompt, return first non-space char (uppercased)
 * PRINT_LABEL_D(lbl, v) — print label then double then '\n'
 * PRINT_LABEL_I(lbl, v) — print label then long then '\n'
 * PRINT_LABEL_S(lbl, s, n) — print label then raw string then '\n'
 *
 * All are  — zero overhead when inlined, one copy if not.
 * ─────────────────────────────────────────────────────────────────────────── */

 double LEER_NUMERO(const char *msg) {
    char buf[32];
    long mlen = STRLEN(msg);
    PRINT_STR(msg, mlen);
    int i = 0;
    while (i < 31) {
        char c;
        int r = READ(&c, 1);
        if (r <= 0 || c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return ATOF(buf, 0);
}

 char LEER_LETRA(const char *msg) {
    long mlen = STRLEN(msg);
    PRINT_STR(msg, mlen);
    char c = 0, tmp;
    int got = 0;
    while (1) {
        int r = READ(&tmp, 1);
        if (r <= 0 || tmp == '\n') break;
        if (!got && tmp != ' ' && tmp != '\t') { c = tmp; got = 1; }
    }
    /* to uppercase */
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    return c;
}

 void PRINT_LABEL_D(const char *label, double v) {
    long len = STRLEN(label);
    PRINT_STR(label, len);
    char buf[32];
    int blen = DTOA(v, buf);
    buf[blen++] = '\n';
    _WRITE_IMPL(buf, blen);
}

 void PRINT_LABEL_I(const char *label, long v) {
    long len = STRLEN(label);
    PRINT_STR(label, len);
    PRINTLN_INT(v);
}

 void PRINT_LABEL_S(const char *label, const char *s, long n) {
    long len = STRLEN(label);
    PRINT_STR(label, len);
    PRINTLN_STR(s, n);
}


/* ─── 16. BEGIN / END  (dual-mode entry) ─────────────────────────────────
 *
 * Allows the same .c file to compile in two modes:
 *
 *   Mode A — with libc (default):
 *     gcc -O2 -fno-omit-frame-pointer -o prog prog.c
 *     BEGIN expands to: int main(void) {
 *     END   expands to: return 0; }
 *
 *   Mode B — without libc (_start entry):
 *     gcc -O2 -fno-omit-frame-pointer -nostdlib -o prog prog.c
 *     BEGIN expands to: naked _start prologue + noinline run() {
 *     END   expands to: EXIT(0); } + naked _start body
 *
 * Usage:
 *   BEGIN
 *       // your program here
 *   END
 *
 * Note: BEGIN/END wrap exactly one function body.
 *       Do not nest BEGIN/END.
 *       Do not use return inside END — use EXIT(code) if you need early exit.
 * ─────────────────────────────────────────────────────────────────────────── */

#ifdef _TINY_NOSTDLIB
/* ── nostdlib mode ── */

__attribute__((noinline)) void _tiny_run(void);

__attribute__((naked)) void _start(void) {
    asm volatile(
        "pushq %rbp\n"
        "movq  %rsp, %rbp\n"
        "subq  $256, %rsp\n"
        "call  _tiny_run\n"
        "xor   %edi, %edi\n"
        "mov   $60,  %eax\n"
        "syscall"
    );
}

#define BEGIN  __attribute__((noinline)) void _tiny_run(void) {
#define END    EXIT(0); }

#else
/* ── libc mode ── */
#define BEGIN  int main(void) {
#define END    return 0; }
#endif


/* ─── 17. functional toolkit ─────────────────────────────────────────────
 *
 * Higher-order operations over arrays. All take function pointers.
 * Inspired by Clojure/FP: throw a function at data, get data back.
 * No alloc. No hidden state. Caller owns all memory.
 *
 * Function pointer signatures:
 *   double (*fn_d)(double)           — double → double  (MAP_D, FOR_EACH_D)
 *   long   (*fn_l)(long)             — long   → long    (MAP_L, FOR_EACH_L)
 *   double (*fn_dd)(double, double)  — (acc, x) → acc  (REDUCE_D)
 *   long   (*fn_ll)(long, long)      — (acc, x) → acc  (REDUCE_L)
 *   double (*fn_zip)(double, double) — (a[i],b[i])→out[i] (ZIP_D)
 *   int    (*pred_d)(double*)        — double* → bool  (FILTER_D)
 *   int    (*pred_l)(long*)          — long*   → bool  (FILTER_L)
 *
 * MAP_D(arr, len, fn)
 *   Transform arr[0..len] in place. arr[i] = fn(arr[i]).
 *
 * MAP_D_INTO(src, dst, len, fn)
 *   Transform src into dst. dst[i] = fn(src[i]). src unchanged.
 *
 * MAP_L / MAP_L_INTO — same for long arrays.
 *
 * FILTER_D(src, len, dst, out_len_ptr, pred)
 *   Compact src into dst keeping elements where pred(&src[i]) != 0.
 *   *out_len_ptr set to number of elements written.
 *   dst may alias src (in-place filter safe — write cursor <= read cursor).
 *
 * FILTER_L — same for long arrays.
 *
 * REDUCE_D(arr, len, init, fn)  →  double
 *   Fold: acc = init; for each x: acc = fn(acc, x); return acc.
 *   sum:  double add(double a, double b) { return a + b; }
 *   max:  double dmax(double a, double b) { return a > b ? a : b; }
 *
 * REDUCE_L — same for long arrays.
 *
 * FOR_EACH_D(arr, len, fn)
 *   Call fn(arr[i]) for side effects. Return value ignored.
 *   Useful for printing, accumulating into external state, etc.
 *
 * FOR_EACH_L — same for long arrays.
 *
 * ZIP_D(a, b, out, len, fn)
 *   out[i] = fn(a[i], b[i]). a, b, out may all alias.
 *
 * STR_FROM_BUF(buf, len)
 *   Wrap an already-filled char buffer as a tiny_str_t view.
 *   If len <= 12: copies into inline storage (buf no longer needed).
 *   If len >  12: heap-mode view — buf must outlive the tiny_str_t.
 *   Use after DTOA, sprintf-style fills, READ, etc.
 * ─────────────────────────────────────────────────────────────────────────── */

/* ── MAP ── */
 void MAP_D(double *arr, long len, double (*fn)(double)) {
    for (long i = 0; i < len; i++) arr[i] = fn(arr[i]);
}

 void MAP_D_INTO(const double *src, double *dst, long len,
                               double (*fn)(double)) {
    for (long i = 0; i < len; i++) dst[i] = fn(src[i]);
}

 void MAP_L(long *arr, long len, long (*fn)(long)) {
    for (long i = 0; i < len; i++) arr[i] = fn(arr[i]);
}

 void MAP_L_INTO(const long *src, long *dst, long len,
                               long (*fn)(long)) {
    for (long i = 0; i < len; i++) dst[i] = fn(src[i]);
}

/* ── FILTER ──
 * Write cursor <= read cursor always, so dst == src is safe.
 */
 void FILTER_D(const double *src, long len,
                             double *dst, long *out_len,
                             int (*pred)(const double *)) {
    long w = 0;
    for (long i = 0; i < len; i++)
        if (pred(&src[i])) dst[w++] = src[i];
    *out_len = w;
}

 void FILTER_L(const long *src, long len,
                             long *dst, long *out_len,
                             int (*pred)(const long *)) {
    long w = 0;
    for (long i = 0; i < len; i++)
        if (pred(&src[i])) dst[w++] = src[i];
    *out_len = w;
}

/* ── REDUCE ── */
 double REDUCE_D(const double *arr, long len, double init,
                               double (*fn)(double, double)) {
    double acc = init;
    for (long i = 0; i < len; i++) acc = fn(acc, arr[i]);
    return acc;
}

 long REDUCE_L(const long *arr, long len, long init,
                             long (*fn)(long, long)) {
    long acc = init;
    for (long i = 0; i < len; i++) acc = fn(acc, arr[i]);
    return acc;
}

/* ── FOR_EACH ── */
 void FOR_EACH_D(const double *arr, long len, void (*fn)(double)) {
    for (long i = 0; i < len; i++) fn(arr[i]);
}

 void FOR_EACH_L(const long *arr, long len, void (*fn)(long)) {
    for (long i = 0; i < len; i++) fn(arr[i]);
}

/* ── ZIP ── */
 void ZIP_D(const double *a, const double *b, double *out,
                          long len, double (*fn)(double, double)) {
    for (long i = 0; i < len; i++) out[i] = fn(a[i], b[i]);
}

 void ZIP_L(const long *a, const long *b, long *out,
                          long len, long (*fn)(long, long)) {
    for (long i = 0; i < len; i++) out[i] = fn(a[i], b[i]);
}

/* ── STR_FROM_BUF ──
 * Wrap a filled char buffer as tiny_str_t.
 * len <= 12: copies bytes into inline storage — buf lifetime irrelevant after.
 * len >  12: heap-mode borrow — buf must outlive the returned tiny_str_t.
 */
 tiny_str_t STR_FROM_BUF(const char *buf, uint32_t len) {
    if (len <= 12) {
        tiny_str_t s;
        s.inlined.length = len;                    /* bit31=0, inline */
        uint32_t i;
        for (i = 0; i < len; i++) s.inlined.data[i] = buf[i];
        for (     ; i < 12;  i++) s.inlined.data[i] = 0;
        return s;
    }
    return S_PTR(buf, len);                        /* heap borrow */
}

/* ── convenience: wrap DTOA result directly into tiny_str_t ── */
#define STR_FROM_DOUBLE(x) ({                      \
    char _sb[32];                                  \
    int  _sl = DTOA((x), _sb);                     \
    STR_FROM_BUF(_sb, (uint32_t)_sl);              \
})

#define STR_FROM_INT(n) ({                         \
    char _ib[24];                                  \
    int  _il = _itoa((long)(n), _ib);              \
    STR_FROM_BUF(_ib, (uint32_t)_il);              \
})


/* ─── 18. namespaces ─────────────────────────────────────────────────────
 *
 * UNSHARE(flags)  — detach calling process from shared kernel namespaces.
 *                   Returns 0 on success, -errno on failure.
 *                   Requires CAP_SYS_ADMIN, OR CLONE_NEWUSER (unprivileged).
 *
 * Namespace flags (combinable with |):
 *   CLONE_NEWUSER  — new user namespace. Unprivileged. Remap uid/gid
 *                    via WRITE_IDMAP after calling. Gets "fake root" inside.
 *   CLONE_NEWNS    — new mount namespace. Requires CLONE_NEWUSER first
 *                    if unprivileged.
 *   CLONE_NEWPID   — new PID namespace. NOTE: calling process keeps its
 *                    original PID. Children become PID 1 in new ns.
 *                    Fork after unshare to get a true PID-1 child.
 *   CLONE_NEWNET   — new network namespace. No interfaces except loopback.
 *   CLONE_NEWUTS   — new UTS namespace. Can set hostname independently.
 *   CLONE_NEWIPC   — new IPC namespace. Isolated SysV IPC + POSIX MQ.
 *
 * WRITE_IDMAP(path, content, len)
 *   — write uid_map / gid_map / setgroups files after CLONE_NEWUSER.
 *     Required sequence for unprivileged user namespace:
 *       UNSHARE(CLONE_NEWUSER);
 *       WRITE_IDMAP("/proc/self/setgroups", "deny", 4);
 *       WRITE_IDMAP("/proc/self/uid_map",   "0 1000 1", 9);  // root→uid 1000
 *       WRITE_IDMAP("/proc/self/gid_map",   "0 1000 1", 9);
 *
 * BEGIN_ISOLATED
 *   — variant of BEGIN that unshares user+mount+IPC namespaces before
 *     running the program body. Writes identity uid/gid maps.
 *     Falls back gracefully if unshare fails (non-Linux or no permission).
 *     Reads current uid from /proc/self/status — no getuid() needed.
 *     Use instead of BEGIN for sandboxed exercises.
 * ─────────────────────────────────────────────────────────────────────────── */

#define SYS_UNSHARE  272
#define SYS_OPEN     2
#define SYS_CLOSE    3
#define SYS_GETUID   102
#define SYS_GETGID   104

#define CLONE_NEWNS   0x00020000
#define CLONE_NEWUTS  0x04000000
#define CLONE_NEWIPC  0x08000000
#define CLONE_NEWUSER 0x10000000
#define CLONE_NEWPID  0x20000000
#define CLONE_NEWNET  0x40000000

#define UNSHARE(flags) ({                                                      \
    register long _rax asm("rax") = SYS_UNSHARE;                              \
    register long _rdi asm("rdi") = (long)(flags);                             \
    asm volatile("syscall"                                                     \
        : "+r"(_rax) : "r"(_rdi) : "rcx", "r11", "memory");                   \
    _rax;                                                                      \
})

/* raw open/write/close for idmap files — can't use libc */
 int _tiny_open_write(const char *path,
                                    const char *data, long len) {
    /* open(path, O_WRONLY) */
    register long _rax asm("rax") = SYS_OPEN;
    register long _rdi asm("rdi") = (long)path;
    register long _rsi asm("rsi") = 1L;   /* O_WRONLY */
    register long _rdx asm("rdx") = 0L;
    asm volatile("syscall"
        : "+r"(_rax) : "r"(_rdi), "r"(_rsi), "r"(_rdx)
        : "rcx", "r11", "memory");
    long fd = _rax;
    if (fd < 0) return (int)fd;
    /* write */
    register long _w_rax asm("rax") = SYS_WRITE;
    register long _w_rdi asm("rdi") = fd;
    register long _w_rsi asm("rsi") = (long)data;
    register long _w_rdx asm("rdx") = len;
    asm volatile("syscall"
        : "+r"(_w_rax)
        : "r"(_w_rdi), "r"(_w_rsi), "r"(_w_rdx)
        : "rcx", "r11", "memory");
    /* close */
    register long _c_rax asm("rax") = SYS_CLOSE;
    register long _c_rdi asm("rdi") = fd;
    asm volatile("syscall"
        : "+r"(_c_rax) : "r"(_c_rdi) : "rcx", "r11", "memory");
    return 0;
}

#define WRITE_IDMAP(path, content, len) \
    _tiny_open_write((path), (content), (len))

/* getuid/getgid via syscall */
 long GETUID(void) {
    register long _rax asm("rax") = SYS_GETUID;
    asm volatile("syscall" : "+r"(_rax) :: "rcx", "r11", "memory");
    return _rax;
}
 long GETGID(void) {
    register long _rax asm("rax") = SYS_GETGID;
    asm volatile("syscall" : "+r"(_rax) :: "rcx", "r11", "memory");
    return _rax;
}

/*
 * _tiny_write_idmaps — write uid_map + gid_map for CLONE_NEWUSER.
 * Maps current uid/gid → 0 (root) inside the namespace.
 * "deny" setgroups first — required by kernel before writing gid_map.
 */
 void _tiny_write_idmaps(void) {
    long uid = GETUID();
    long gid = GETGID();

    /* build "0 <uid> 1\0" */
    char uid_map[32], gid_map[32];
    int ulen = 0, glen = 0;

    /* "0 " */
    uid_map[ulen++] = '0'; uid_map[ulen++] = ' ';
    gid_map[glen++] = '0'; gid_map[glen++] = ' ';

    /* uid digits */
    char tmp[20]; int tlen = (int)(_itoa(uid, tmp));
    for (int i = 0; i < tlen; i++) uid_map[ulen++] = tmp[i];
    tlen = (int)(_itoa(gid, tmp));
    for (int i = 0; i < tlen; i++) gid_map[glen++] = tmp[i];

    /* " 1" */
    uid_map[ulen++] = ' '; uid_map[ulen++] = '1';
    gid_map[glen++] = ' '; gid_map[glen++] = '1';

    _tiny_open_write("/proc/self/setgroups", "deny", 4);
    _tiny_open_write("/proc/self/uid_map",   uid_map, ulen);
    _tiny_open_write("/proc/self/gid_map",   gid_map, glen);
}

/*
 * BEGIN_ISOLATED — unshare user + IPC + UTS namespaces, remap uid/gid.
 * Network and mount namespaces intentionally omitted from default:
 *   - CLONE_NEWNET breaks loopback (annoying for AED exercises)
 *   - CLONE_NEWNS  needs more setup to be useful
 * Add them manually with UNSHARE(CLONE_NEWNET|CLONE_NEWNS) inside body
 * if you need full isolation.
 */
#ifdef _TINY_NOSTDLIB
#define BEGIN_ISOLATED                                                         \
    __attribute__((noinline)) void _tiny_run(void) {                          \
    if (UNSHARE(CLONE_NEWUSER | CLONE_NEWIPC | CLONE_NEWUTS) == 0)            \
        _tiny_write_idmaps();
#else
#define BEGIN_ISOLATED                                                         \
    int main(void) {                                                           \
    if (UNSHARE(CLONE_NEWUSER | CLONE_NEWIPC | CLONE_NEWUTS) == 0)            \
        _tiny_write_idmaps();
#endif


/* ─── 19. bump allocator ─────────────────────────────────────────────────
 *
 * Simpler than slab. One brk call, pointer walks forward, no per-slot
 * bookkeeping. Free is O(1) but frees EVERYTHING at once (reset).
 *
 * Ideal for: building a graph, tree, or list of nodes in one pass,
 *            then discarding all of them together at the end.
 *            AED exercises that build-then-discard data structures.
 *
 * BumpAlloc  — the allocator state (lives on stack or static)
 * BUMP_INIT(b, total_bytes)  — claim total_bytes from brk. Returns -1 on fail.
 * BUMP_ALLOC(b, size)        — return aligned pointer, advance cursor. NULL if full.
 * BUMP_RESET(b)              — cursor = base. Logically frees everything. O(1).
 * BUMP_USED(b)               — bytes currently allocated (long)
 * BUMP_LEFT(b)               — bytes remaining (long)
 *
 * Unlike slab: slots can be different sizes. No free list. No fragmentation.
 * Unlike malloc: no headers per allocation, no coalescing, no thread safety.
 * ─────────────────────────────────────────────────────────────────────────── */

typedef struct {
    char *base;
    char *cur;
    char *end;
} BumpAlloc;

 int BUMP_INIT(BumpAlloc *b, long total_bytes) {
    total_bytes = ALIGN_UP(total_bytes);
    void *mem = SBRK(total_bytes);
    if (mem == (void*)-1L) return -1;
    b->base = (char*)mem;
    b->cur  = (char*)mem;
    b->end  = (char*)mem + total_bytes;
    return 0;
}

 void *BUMP_ALLOC(BumpAlloc *b, long size) {
    size = ALIGN_UP(size);
    if (b->cur + size > b->end) return (void*)0;
    void *ptr = (void*)b->cur;
    b->cur += size;
    return ptr;
}

#define BUMP_RESET(b)  ((b)->cur = (b)->base)
#define BUMP_USED(b)   ((long)((b)->cur  - (b)->base))
#define BUMP_LEFT(b)   ((long)((b)->end  - (b)->cur))


/* ─── 20. terminal & game loop ───────────────────────────────────────────
 *
 * Raw terminal mode: disable canonical processing and echo so every
 * keypress arrives immediately without waiting for Enter.
 * Essential for game loops, menus, interactive demos.
 *
 * TERM_RAW()     — enter raw mode. Save previous termios.
 *                  Disables: ICANON (line buffering), ECHO (character echo)
 *                  ISIG (Ctrl-C signals), IXON (Ctrl-S/Q flow control).
 *                  Sets VMIN=1, VTIME=0 (block until 1 char available).
 * TERM_RESET()   — restore saved termios. Call before EXIT or on cleanup.
 * TERM_NONBLOCK()— VMIN=0, VTIME=0: READ returns immediately, 0 if no key.
 *                  Use inside game loop after TERM_RAW.
 *
 * POLL_KEY(ms)   — returns 1 if a keypress is available within ms milliseconds.
 *                  0 = timeout (no key). -1 = error.
 *                  ms=0: pure non-blocking check.
 *                  ms=-1: block forever (like blocking READ).
 *
 * READ_KEY()     — read one char. Returns char as int, or -1 if none available.
 *                  Call TERM_RAW() + TERM_NONBLOCK() first for non-blocking use.
 *
 * Typical game loop:
 *   TERM_RAW();
 *   TERM_NONBLOCK();
 *   while (running) {
 *       if (POLL_KEY(16)) {           // ~60fps frame budget
 *           int k = READ_KEY();
 *           if (k == 'q') running = 0;
 *           if (k == 'w') player_y--;
 *       }
 *       // update + render
 *   }
 *   TERM_RESET();
 *
 * Arrow keys arrive as escape sequences: ESC '[' 'A'/'B'/'C'/'D'.
 * Use READ_KEY() three times when first char == 27 (ESC) to consume them.
 * ─────────────────────────────────────────────────────────────────────────── */

#define SYS_IOCTL    16
#define SYS_POLL     7
#define TCGETS       0x5401
#define TCSETS       0x5402
#define STDIN_FD     0

/* termios struct — 60 bytes on x86_64 Linux, matches kernel layout */
typedef struct {
    unsigned int  c_iflag;    /* input flags  */
    unsigned int  c_oflag;    /* output flags */
    unsigned int  c_cflag;    /* control flags */
    unsigned int  c_lflag;    /* local flags  */
    unsigned char c_line;     /* line discipline */
    unsigned char c_cc[19];   /* control chars */
    unsigned int  c_ispeed;   /* input speed  */
    unsigned int  c_ospeed;   /* output speed */
} Termios;

/* flag bits */
#define _ICANON  0000002u
#define _ECHO    0000010u
#define _ECHOE   0000020u
#define _ECHOK   0000040u
#define _ISIG    0000001u
#define _IXON    0002000u
#define _OPOST   0000001u
/* c_cc indices */
#define _VMIN    6
#define _VTIME   5

/* saved state for TERM_RESET */
static Termios _tiny_saved_termios;
static int     _tiny_term_saved = 0;

 int _tiny_tcgetset(int cmd, Termios *t) {
    register long _rax asm("rax") = SYS_IOCTL;
    register long _rdi asm("rdi") = STDIN_FD;
    register long _rsi asm("rsi") = (long)cmd;
    register long _rdx asm("rdx") = (long)t;
    asm volatile("syscall"
        : "+r"(_rax)
        : "r"(_rdi), "r"(_rsi), "r"(_rdx)
        : "rcx", "r11", "memory");
    return (int)_rax;
}

 int TERM_RAW(void) {
    if (_tiny_tcgetset(TCGETS, &_tiny_saved_termios) < 0) return -1;
    _tiny_term_saved = 1;

    Termios raw = _tiny_saved_termios;
    /* input: no flow control, no signal chars, no CR/NL translation */
    raw.c_iflag &= ~(_IXON);
    /* local: no echo, no canonical, no signals */
    raw.c_lflag &= ~(_ICANON | _ECHO | _ECHOE | _ECHOK | _ISIG);
    /* output: disable post-processing (optional — remove if output breaks) */
    /* raw.c_oflag &= ~_OPOST; */
    /* read: block until at least 1 byte, no timeout */
    raw.c_cc[_VMIN]  = 1;
    raw.c_cc[_VTIME] = 0;

    return _tiny_tcgetset(TCSETS, &raw);
}

 int TERM_RESET(void) {
    if (!_tiny_term_saved) return 0;
    return _tiny_tcgetset(TCSETS, &_tiny_saved_termios);
}

 int TERM_NONBLOCK(void) {
    Termios t;
    if (_tiny_tcgetset(TCGETS, &t) < 0) return -1;
    t.c_cc[_VMIN]  = 0;   /* return immediately even if no bytes */
    t.c_cc[_VTIME] = 0;
    return _tiny_tcgetset(TCSETS, &t);
}

/* pollfd struct — matches kernel ABI */
typedef struct {
    int   fd;
    short events;
    short revents;
} PollFd;

#define POLLIN_FLAG  0x0001
#define POLLOUT_FLAG 0x0004
#define POLLERR_FLAG 0x0008

/*
 * POLL_KEY(ms) — wait up to ms milliseconds for stdin to be readable.
 * Returns 1 if key available, 0 if timeout, -1 on error.
 * ms=0: instant check. ms=-1: block until key.
 */
 int POLL_KEY(int ms) {
    PollFd pfd = { .fd = STDIN_FD, .events = POLLIN_FLAG, .revents = 0 };
    register long _rax asm("rax") = SYS_POLL;
    register long _rdi asm("rdi") = (long)&pfd;
    register long _rsi asm("rsi") = 1L;
    register long _rdx asm("rdx") = (long)ms;
    asm volatile("syscall"
        : "+r"(_rax)
        : "r"(_rdi), "r"(_rsi), "r"(_rdx)
        : "rcx", "r11", "memory");
    if (_rax < 0) return -1;
    return (pfd.revents & POLLIN_FLAG) ? 1 : 0;
}

/*
 * READ_KEY() — read one raw byte from stdin.
 * Returns byte as int (0-255), or -1 if no data available.
 * Call TERM_RAW() + TERM_NONBLOCK() first.
 *
 * Arrow keys: first byte is 27 (ESC). If POLL_KEY returns 1 again
 * immediately after, read two more bytes: '[' then 'A'/'B'/'C'/'D'.
 * Defines below cover common cases.
 */
 int READ_KEY(void) {
    unsigned char c;
    register long _rax asm("rax") = SYS_READ;
    register long _rdi asm("rdi") = STDIN_FD;
    register long _rsi asm("rsi") = (long)&c;
    register long _rdx asm("rdx") = 1L;
    asm volatile("syscall"
        : "+r"(_rax)
        : "r"(_rdi), "r"(_rsi), "r"(_rdx)
        : "rcx", "r11", "memory");
    return (_rax == 1) ? (int)c : -1;
}

/* common key constants */
#define KEY_ESC     27
#define KEY_UP      'A'    /* read after ESC + '[' */
#define KEY_DOWN    'B'
#define KEY_RIGHT   'C'
#define KEY_LEFT    'D'
#define KEY_ENTER   '\n'
#define KEY_SPACE   ' '
#define KEY_CTRL(c) ((c) & 0x1F)   /* e.g. KEY_CTRL('c') == 3 */

/*
 * CONSUME_ESCAPE() — drain a full escape sequence after reading ESC (27).
 * Reads '[' + one more byte. Returns the final byte (A/B/C/D for arrows).
 * Returns -1 if sequence is incomplete or not an arrow.
 */
#define CONSUME_ESCAPE() ({            \
    int _e1 = READ_KEY();              \
    int _e2 = (_e1 == '[') ? READ_KEY() : -1; \
    _e2;                               \
})

/*
 * CLEAR_SCREEN() — ANSI escape: clear terminal + move cursor to 0,0.
 * No ncurses needed.
 */
#define CLEAR_SCREEN() PRINT_STR("\033[2J\033[H", 7)

/*
 * MOVE_CURSOR(row, col) — ANSI escape: move cursor to row,col (1-indexed).
 * Builds the sequence on the stack, no heap.
 */
#define MOVE_CURSOR(row, col) ({                                               \
    char _mc[24]; int _ml = 0;                                                 \
    _mc[_ml++] = '\033'; _mc[_ml++] = '[';                                     \
    int _tl = _itoa((long)(row), _mc + _ml); _ml += _tl;                      \
    _mc[_ml++] = ';';                                                          \
    _tl = _itoa((long)(col), _mc + _ml); _ml += _tl;                          \
    _mc[_ml++] = 'H';                                                          \
    _WRITE_IMPL(_mc, _ml);                                                     \
})

/* hide/show cursor — useful during render to avoid flicker */
#define CURSOR_HIDE() PRINT_STR("\033[?25l", 6)
#define CURSOR_SHOW() PRINT_STR("\033[?25h", 6)
