/*
 * test.c — tiny.h full test suite
 * Compile: make test
 * Output:  PASS/FAIL per test, summary at end. EXIT(1) if any fail.
 *
 * §15 (interactive I/O) and §20 (terminal) require a real TTY —
 * skipped automatically when running non-interactively.
 * §18 (namespaces) skipped if kernel denies unprivileged userns.
 */

#include "tiny.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Test runner
 * ═══════════════════════════════════════════════════════════════════════════
 */
static int _t_pass = 0;
static int _t_fail = 0;
static int _t_skip = 0;

static inline void _t_result(const char *name, int ok) {
  long nlen = STRLEN(name);
  PRINT_STR(name, nlen);
  for (long i = nlen; i < 48; i++)
    PRINT_STR(".", 1);
  if (ok) {
    PRINTLN_STR(" PASS", 5);
    _t_pass++;
  } else {
    PRINTLN_STR(" FAIL", 5);
    _t_fail++;
  }
}

static inline void _t_skip_msg(const char *name, const char *reason) {
  long nlen = STRLEN(name);
  PRINT_STR(name, nlen);
  for (long i = nlen; i < 48; i++)
    PRINT_STR(".", 1);
  PRINT_STR(" SKIP (", 7);
  PRINT_STR(reason, STRLEN(reason));
  PRINTLN_STR(")", 1);
  _t_skip++;
}

#define TEST(name, expr) _t_result((name), (int)(expr))
#define SKIP(name, reason) _t_skip_msg((name), (reason))

static inline void _section(const char *s) {
  PRINTLN_STR("", 0);
  PRINT_STR("── ", 3);
  PRINTLN_STR(s, STRLEN(s));
}

/* epsilon float compare */
static inline int _feq(double a, double b) {
  double d = a - b;
  if (d < 0.0)
    d = -d;
  return d < 1e-9;
}
static inline int _feq3(double a, double b) {
  double d = a - b;
  if (d < 0.0)
    d = -d;
  return d < 0.001;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §1 — Syscall numbers
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s1(void) {
  _section("§1  syscall numbers");
  TEST("SYS_READ  == 0", SYS_READ == 0);
  TEST("SYS_WRITE == 1", SYS_WRITE == 1);
  TEST("SYS_EXIT  == 60", SYS_EXIT == 60);
  TEST("SYS_BRK   == 12", SYS_BRK == 12);
  TEST("STDIN     == 0", STDIN == 0);
  TEST("STDOUT    == 1", STDOUT == 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §2 — Alignment & pointer utils
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s2(void) {
  _section("§2  alignment & pointer utils");
  TEST("ALIGN_UP(0)==0", ALIGN_UP(0) == 0);
  TEST("ALIGN_UP(1)==8", ALIGN_UP(1) == 8);
  TEST("ALIGN_UP(7)==8", ALIGN_UP(7) == 8);
  TEST("ALIGN_UP(8)==8", ALIGN_UP(8) == 8);
  TEST("ALIGN_UP(9)==16", ALIGN_UP(9) == 16);
  TEST("ALIGN_DOWN(7)==0", ALIGN_DOWN(7) == 0);
  TEST("ALIGN_DOWN(8)==8", ALIGN_DOWN(8) == 8);
  TEST("ALIGN_DOWN(15)==8", ALIGN_DOWN(15) == 8);

  double val = 3.14;
  void *base = &val;
  TEST("OFFSET+0 == base", OFFSET(base, 0) == base);
  TEST("OFFSET+8 == base+8", (char *)OFFSET(base, 8) == (char *)base + 8);

  double arr[3] = {1.0, 2.0, 3.0};
  TEST("INDEX(arr,0)==&arr[0]", INDEX(arr, 0) == (void *)&arr[0]);
  TEST("INDEX(arr,2)==&arr[2]", INDEX(arr, 2) == (void *)&arr[2]);
  TEST("DEREF(&val)==3.14", _feq(DEREF(&val), 3.14));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §3 — Stack frame helpers
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void __attribute__((noinline)) test_s3(void) {
  _section("§3  stack frame helpers");
  void *a = STACK(8);
  void *b = STACK(16);
  TEST("STACK(8) != NULL", a != (void *)0);
  TEST("STACK(16) != NULL", b != (void *)0);
  TEST("STACK(8) != STACK(16)", a != b);
  TEST("STACK slots 8 bytes apart",
       (char *)a - (char *)b == 8 || (char *)b - (char *)a == 8);
  *(double *)STACK(8) = 99.0;
  TEST("write+read STACK slot", _feq(*(double *)STACK(8), 99.0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §4 — XMM scalar f64 arithmetic
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void __attribute__((noinline)) test_s4(void) {
  _section("§4  xmm scalar f64 arithmetic");
  double a, b, out;

  DEF(&a, 3.0);
  TEST("DEF 3.0", _feq(a, 3.0));
  DEF(&b, 4.0);
  TEST("DEF 4.0", _feq(b, 4.0));
  COPY(&out, &a);
  TEST("COPY", _feq(out, 3.0));

  SWAP(&a, &b);
  TEST("SWAP a==4", _feq(a, 4.0));
  TEST("SWAP b==3", _feq(b, 3.0));
  SWAP(&a, &b);

  INC(&a);
  TEST("INC 3→4", _feq(a, 4.0));
  DEC(&a);
  TEST("DEC 4→3", _feq(a, 3.0));

  SUM(&out, &a, &b);
  TEST("SUM 3+4=7", _feq(out, 7.0));
  SUB(&out, &b, &a);
  TEST("SUB 4-3=1", _feq(out, 1.0));
  MUL(&out, &a, &b);
  TEST("MUL 3*4=12", _feq(out, 12.0));
  DIV(&out, &b, &a);
  TEST("DIV 4/3", _feq3(out, 1.333));

  double nine = 9.0;
  ROOT(&out, &nine);
  TEST("ROOT(9)==3", _feq(out, 3.0));

  double neg = -5.0;
  // ABS(out, neg);
  // TEST("ABS(-5)==5", _feq(out, 5.0));
  // ABS(&out, &b);
  // TEST("ABS(4)==4", _feq(out, 4.0));
  // NEG(&out, &b);
  // TEST("NEG(4)==-4", _feq(out, -4.0));
  // NEG(&out, &neg);
  // TEST("NEG(-5)==5", _feq(out, 5.0));
  double x = 2.0, y = 8.0;
  MIN(&out, &x, &y);
  TEST("MIN(2,8)==2", _feq(out, 2.0));
  MAX(&out, &x, &y);
  TEST("MAX(2,8)==8", _feq(out, 8.0));
  AVG(&out, &x, &y);
  TEST("AVG(2,8)==5", _feq(out, 5.0));

  double m = 10.0, n = 3.0;
  MOD(&out, &m, &n);
  TEST("MOD(10,3)==1", _feq(out, 1.0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §5 — f64 comparisons
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void __attribute__((noinline)) test_s5(void) {
  _section("§5  f64 comparisons");
  double a = 3.0, b = 5.0, c = 3.0, out;

  IS_GT(&out, &b, &a);
  TEST("IS_GT(5,3)==1", _feq(out, 1.0));
  IS_GT(&out, &a, &b);
  TEST("IS_GT(3,5)==0", _feq(out, 0.0));
  IS_LT(&out, &a, &b);
  TEST("IS_LT(3,5)==1", _feq(out, 1.0));
  IS_LT(&out, &b, &a);
  TEST("IS_LT(5,3)==0", _feq(out, 0.0));
  IS_GE(&out, &a, &c);
  TEST("IS_GE(3,3)==1", _feq(out, 1.0));
  IS_GE(&out, &a, &b);
  TEST("IS_GE(3,5)==0", _feq(out, 0.0));
  IS_LE(&out, &a, &c);
  TEST("IS_LE(3,3)==1", _feq(out, 1.0));
  IS_LE(&out, &b, &a);
  TEST("IS_LE(5,3)==0", _feq(out, 0.0));
  IS_EQ(&out, &a, &c);
  TEST("IS_EQ(3,3)==1", _feq(out, 1.0));
  IS_EQ(&out, &a, &b);
  TEST("IS_EQ(3,5)==0", _feq(out, 0.0));
  IS_NE(&out, &a, &b);
  TEST("IS_NE(3,5)==1", _feq(out, 1.0));
  IS_NE(&out, &a, &c);
  TEST("IS_NE(3,3)==0", _feq(out, 0.0));
  CMP(&out, &a, &b);
  TEST("CMP(3,5)==-1", _feq(out, -1.0));
  CMP(&out, &b, &a);
  TEST("CMP(5,3)==1", _feq(out, 1.0));
  CMP(&out, &a, &c);
  TEST("CMP(3,3)==0", _feq(out, 0.0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §6 — f64 control flow
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void __attribute__((noinline)) test_s6(void) {
  _section("§6  f64 control flow");
  double idx, count;

  count = 0.0;
  FOR_RANGE(&idx, 0.0, 5.0) { count += 1.0; }
  TEST("FOR_RANGE runs 5 times", _feq(count, 5.0));
  TEST("FOR_RANGE idx ends at 5", _feq(idx, 5.0));

  count = 0.0;
  FOR_DOWN(&idx, 4.0, -1.0) { count += 1.0; }
  TEST("FOR_DOWN runs 5 times", _feq(count, 5.0));

  double flag = 3.0;
  count = 0.0;
  WHILE_NZ(&flag) {
    count += 1.0;
    flag -= 1.0;
  }
  TEST("WHILE_NZ ran 3 times", _feq(count, 3.0));

  double hit = 0.0, big = 10.0, small = 2.0;
  IF_GT(&big, &small) { hit = 1.0; }
  TEST("IF_GT taken", _feq(hit, 1.0));
  hit = 0.0;
  IF_GT(&small, &big) { hit = 1.0; }
  TEST("IF_GT not taken", _feq(hit, 0.0));

  double eq1 = 7.0, eq2 = 7.0;
  hit = 0.0;
  IF_EQ(&eq1, &eq2) { hit = 1.0; }
  TEST("IF_EQ taken", _feq(hit, 1.0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §7 — f64 array helpers
 * ═══════════════════════════════════════════════════════════════════════════
 */
static int _gt6(void *p) { return *(double *)p > 6.0; }
static int _never(void *p) {
  (void)p;
  return 0;
}

static void __attribute__((noinline)) test_s7(void) {
  _section("§7  f64 array helpers");
  double arr[5] = {0};

  FILL(arr, 5, 9.0);
  TEST("FILL all 9.0", _feq(arr[0], 9.0) && _feq(arr[4], 9.0));
  ZERO(&arr[2]);
  TEST("ZERO arr[2]", _feq(arr[2], 0.0));

  double src = 42.0, dst = 0.0;
  ARRAY_SET(arr, 1, &src);
  TEST("ARRAY_SET arr[1]", _feq(arr[1], 42.0));
  ARRAY_GET(arr, 1, &dst);
  TEST("ARRAY_GET arr[1]", _feq(dst, 42.0));

  const char *days[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
  TEST("NTH(2)==WED", NTH(days, 2, 7) == (void *)days[2]);
  TEST("NTH(7)==NULL", NTH(days, 7, 7) == (void *)0);
  TEST("NTH(-1)==NULL", NTH(days, -1, 7) == (void *)0);

  // [thing] [action] [reason]
  double nums[] = {1.0, 3.0, 7.0, 9.0};
  double *res = FIND_IF(nums, 4, sizeof(double), _gt6);
  TEST("FIND_IF 7.0", res && *res == 7.0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §8 — Integer bit ops
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s8(void) {
  _section("§8  integer bit ops");
  TEST("IS_POW2(1)", IS_POW2(1));
  TEST("IS_POW2(16)", IS_POW2(16));
  TEST("IS_POW2(0)==0", !IS_POW2(0));
  TEST("IS_POW2(3)==0", !IS_POW2(3));
  TEST("IS_OPP_SIGN(1,-1)", IS_OPP_SIGN(1, -1));
  TEST("IS_OPP_SIGN(2,3)==0", !IS_OPP_SIGN(2, 3));
  TEST("SIGN(5)==1", SIGN_IDX(5) == 1);
  TEST("SIGN(-3)==-1", SIGN_IDX(-3) == -1);
  TEST("SIGN(0)==0", SIGN_IDX(0) == 0);
  TEST("IABS(-7)==7", IABS(-7) == 7);
  TEST("IABS(7)==7", IABS(7) == 7);
  TEST("IMIN(3,5)==3", IMIN(3, 5) == 3);
  TEST("IMIN(5,3)==3", IMIN(5, 3) == 3);
  TEST("IMAX(3,5)==5", IMAX(3, 5) == 5);
  TEST("IAVG(2,8)==5", IAVG(2, 8) == 5);
  TEST("IAVG(0,0)==0", IAVG(0, 0) == 0);
  TEST("MERGE(0xF0,0x0F,0xFF)==0x0F", MERGE(0xF0, 0x0F, 0xFF) == 0x0F);
  TEST("MERGE(0xF0,0x0F,0x00)==0xF0", MERGE(0xF0, 0x0F, 0x00) == 0xF0);
  TEST("NEXT_POW2(3)==4", NEXT_POW2(3) == 4);
  TEST("NEXT_POW2(8)==8", NEXT_POW2(8) == 8);
  TEST("NEXT_POW2(9)==16", NEXT_POW2(9) == 16);
  TEST("POPCOUNT(0)==0", POPCOUNT(0) == 0);
  TEST("POPCOUNT(0xFF)==8", POPCOUNT(0xFF) == 8);
  TEST("POPCOUNT(7)==3", POPCOUNT(7) == 3);
  TEST("TRAILING_ZEROS(1)==0", TRAILING_ZEROS(1) == 0);
  TEST("TRAILING_ZEROS(8)==3", TRAILING_ZEROS(8) == 3);
  TEST("TRAILING_ZEROS(16)==4", TRAILING_ZEROS(16) == 4);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9 — String & memory ops
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s9(void) {
  _section("§9  string & memory ops");
  TEST("STRLEN empty", STRLEN("") == 0);
  TEST("STRLEN hello==5", STRLEN("hello") == 5);
  TEST("STRLEN 16 chars", STRLEN("0123456789abcdef") == 16);

  char dst[32] = {0};
  MEM_COPY(dst, "HELLO", 5);
  TEST("MEM_COPY 5 bytes", dst[0] == 'H' && dst[1] == 'E' && dst[2] == 'L' &&
                               dst[3] == 'L' && dst[4] == 'O');
  // ZERO(dst, 5);
  TEST("MEM_ZERO clears", dst[0] == 0 && dst[4] == 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §10 — DTOA / ATOF / ATOI
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s10(void) {
  _section("§10 DTOA / ATOF / ATOI");
  char buf[32];
  int len;

  len = DTOA(0.0, buf);
  TEST("DTOA(0.0)→'0'", len == 1 && buf[0] == '0');
  len = DTOA(42.0, buf);
  TEST("DTOA(42.0) len==2", len == 2 && buf[0] == '4' && buf[1] == '2');
  len = DTOA(-3.14, buf);
  TEST("DTOA(-3.14) starts '-'", buf[0] == '-' && len > 1);
  len = DTOA(1.5, buf);
  TEST("DTOA(1.5) has '.'", len == 3 && buf[1] == '.');

  TEST("ATOI '0'==0", ATOI("0", 0) == 0);
  TEST("ATOI '42'==42", ATOI("42", 0) == 42);
  TEST("ATOI '-7'==-7", ATOI("-7", 0) == -7);
  TEST("ATOI '  5'==5", ATOI("  5", 0) == 5);

  TEST("ATOF '0.0'≈0", _feq(ATOF("0.0", 0), 0.0));
  TEST("ATOF '3.14'≈3.14", _feq3(ATOF("3.14", 0), 3.14));
  TEST("ATOF '-1.5'≈-1.5", _feq(ATOF("-1.5", 0), -1.5));
  TEST("ATOF '42'≈42", _feq(ATOF("42", 0), 42.0));

  const char *end;
  long v = ATOI("123abc", &end);
  TEST("ATOI end_ptr stops at 'a'", v == 123 && *end == 'a');
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §11 — I/O smoke
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s11(void) {
  _section("§11 I/O smoke (check stdout)");
  double pi = 3.14159;
  PRINT(&pi);
  PRINTLN_STR(" <- PRINT f64", 13);
  PRINT_INT(-99);
  PRINTLN_STR(" <- PRINT_INT", 13);
  PRINTLN_STR("PRINT_STR ok", 12);
  STR_PRINTLN(S_PTR("STR_PRINTLN ok", 14));
  TEST("I/O smoke no crash", 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §12 — Slab allocator
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s12(void) {
  _section("§12 slab allocator");
  SlabPool pool;
  int r = SLAB_INIT(&pool, sizeof(double), 8);
  TEST("SLAB_INIT returns 0", r == 0);
  TEST("slot_size aligned", (pool.slot_size & 7) == 0);

  double *a = SLAB_ALLOC(&pool);
  double *b = SLAB_ALLOC(&pool);
  TEST("SLAB_ALLOC != NULL", a != (void *)0 && b != (void *)0);
  TEST("slots differ", a != b);

  *a = 111.0;
  *b = 222.0;
  TEST("write a=111", _feq(*a, 111.0));
  TEST("write b=222", _feq(*b, 222.0));

  SLAB_FREE(&pool, a);
  double *c = SLAB_ALLOC(&pool);
  TEST("free then alloc reuses", c == a);

  SlabPool small;
  SLAB_INIT(&small, 8, 2);
  SLAB_ALLOC(&small);
  SLAB_ALLOC(&small);
  TEST("exhausted → NULL", SLAB_ALLOC(&small) == (void *)0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §13 — tiny_str_t
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s13(void) {
  _section("§13 tiny_str_t");

  tiny_str_t s1 = S("HELLO");
  TEST("S() len==5", STR_LEN(s1) == 5);
  TEST("S() inlined", STR_IS_INLINED(s1));
  TEST("S() data[0]=='H'", STR_DATA(s1)[0] == 'H');

  tiny_str_t s12 = S("123456789012");
  TEST("S(12) inlined", STR_IS_INLINED(s12) && STR_LEN(s12) == 12);

  tiny_str_t s2 = S_PTR("HELLO_WORLD_LONG", 16);
  TEST("S_PTR() len==16", STR_LEN(s2) == 16);
  TEST("S_PTR() not inlined", !STR_IS_INLINED(s2));
  TEST("S_PTR() data[0]=='H'", STR_DATA(s2)[0] == 'H');

  tiny_str_t s3 = S("HELLO");
  TEST("STR_EQ same", STR_EQ(s1, s3));
  TEST("STR_EQ diff", !STR_EQ(s1, S("WORLD")));
  TEST("STR_EQ_LIT match", STR_EQ_LIT(s1, "HELLO"));
  TEST("STR_EQ_LIT no match", !STR_EQ_LIT(s1, "WORLD"));

  TEST("STR_STARTS_WITH match", STR_STARTS_WITH(s2, "HELL"));
  TEST("STR_STARTS_WITH no match", !STR_STARTS_WITH(s2, "WORLD"));

  tiny_str_t sl = STR_SLICE(s2, 6, 5);
  TEST("STR_SLICE len==5", STR_LEN(sl) == 5);
  TEST("STR_SLICE not inlined", !STR_IS_INLINED(sl));
  TEST("STR_SLICE data[0]=='W'", STR_DATA(sl)[0] == 'W');
  TEST("STR_SLICE data[4]=='D'", STR_DATA(sl)[4] == 'D');

  tiny_str_t oor = STR_SLICE(s2, 99, 5);
  TEST("STR_SLICE OOB len==0", STR_LEN(oor) == 0);

  TEST("STR_FIND_BYTE '_'==5", STR_FIND_BYTE(s2, '_') == 5);
  TEST("STR_FIND_BYTE 'Z'==-1", STR_FIND_BYTE(s2, 'Z') == -1);
  TEST("STR_FIND_BYTE first==0", STR_FIND_BYTE(s2, 'H') == 0);

  char cbuf[] = "TINY";
  tiny_str_t sf = STR_FROM_BUF(cbuf, 4);
  TEST("STR_FROM_BUF short inline", STR_IS_INLINED(sf) && STR_LEN(sf) == 4);
  TEST("STR_FROM_BUF data[0]=='T'", STR_DATA(sf)[0] == 'T');

  char longbuf[] = "THIS_IS_A_LONGER_STRING";
  tiny_str_t sl2 = STR_FROM_BUF(longbuf, 23);
  TEST("STR_FROM_BUF long heap", !STR_IS_INLINED(sl2) && STR_LEN(sl2) == 23);

  TEST("STR_FROM_DOUBLE len>=2", STR_LEN(STR_FROM_DOUBLE(42.0)) >= 2);
  TEST("STR_FROM_INT len==3", STR_LEN(STR_FROM_INT(123)) == 3);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §14 — Math constants & numeric helpers
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s14(void) {
  _section("§14 math & numeric helpers");
  TEST("PI≈3.14159", _feq3(PI, 3.14159));
  TEST("TAU≈6.28318", _feq3(TAU, 6.28318));
  TEST("EULER≈2.718", _feq3(EULER, 2.718));

  TEST("CLAMP(5,0,10)==5", _feq(CLAMP(5.0, 0.0, 10.0), 5.0));
  TEST("CLAMP(-1,0,10)==0", _feq(CLAMP(-1.0, 0.0, 10.0), 0.0));
  TEST("CLAMP(11,0,10)==10", _feq(CLAMP(11.0, 0.0, 10.0), 10.0));

  TEST("LERP(0,10,0.5)==5", _feq(LERP(0.0, 10.0, 0.5), 5.0));
  TEST("LERP(0,10,0.0)==0", _feq(LERP(0.0, 10.0, 0.0), 0.0));
  TEST("LERP(0,10,1.0)==10", _feq(LERP(0.0, 10.0, 1.0), 10.0));

  TEST("SIGN_IDX(-5)==0", SIGN_IDX(-5.0) == 0);
  TEST("SIGN_IDX(0)==1", SIGN_IDX(0.0) == 1);
  TEST("SIGN_IDX(5)==2", SIGN_IDX(5.0) == 2);

  TEST("INT_POW(2,0)==1", INT_POW(2, 0) == 1);
  TEST("INT_POW(2,8)==256", INT_POW(2, 8) == 256);
  TEST("INT_POW(3,3)==27", INT_POW(3, 3) == 27);

  TEST("GAUSS(0)==0", GAUSS(0) == 0);
  TEST("GAUSS(10)==55", GAUSS(10) == 55);
  TEST("GAUSS(100)==5050", GAUSS(100) == 5050);

  TEST("FACTORIAL(0)==1", FACTORIAL(0) == 1);
  TEST("FACTORIAL(5)==120", FACTORIAL(5) == 120);
  TEST("FACTORIAL(10)==3628800", FACTORIAL(10) == 3628800);

  TEST("IS_LEAP(2000)==1", IS_LEAP(2000) == 1);
  TEST("IS_LEAP(1900)==0", IS_LEAP(1900) == 0);
  TEST("IS_LEAP(2024)==1", IS_LEAP(2024) == 1);
  TEST("IS_LEAP(2023)==0", IS_LEAP(2023) == 0);

  char ca = 'A', cb = 'B';
  SWAP_CHARS(ca, cb);
  TEST("SWAP_CHARS A↔B", ca == 'B' && cb == 'A');
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §15 — Interactive helpers (TTY required → skip)
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s15(void) {
  _section("§15 interactive helpers");
  SKIP("LEER_NUMERO", "requires TTY");
  SKIP("LEER_LETRA", "requires TTY");
  PRINT_LABEL_D("label_d: ", 3.14);
  PRINT_LABEL_I("label_i: ", 42);
  PRINT_LABEL_S("label_s: ", "ok", 2);
  TEST("PRINT_LABEL smoke", 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §16 — BEGIN / END
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s16(void) {
  _section("§16 BEGIN / END");
  TEST("compiled and running", 1);
#ifdef _TINY_NOSTDLIB
  TEST("_TINY_NOSTDLIB active", 1);
#else
  TEST("libc mode active", 1);
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §17 — Functional toolkit
 * ═══════════════════════════════════════════════════════════════════════════
 */
static double _sq(double x) { return x * x; }
static double _add_d(double a, double b) { return a + b; }
static double _mul_d(double a, double b) { return a * b; }
static double _dmax(double a, double b) { return a > b ? a : b; }
static int _pos_d(const double *p) { return *p > 0.0; }
static long _dbl_l(long x) { return x * 2; }
static long _add_l(long a, long b) { return a + b; }
static int _even_l(const long *p) { return (*p & 1) == 0; }
static double _add_zip(double a, double b) { return a + b; }

static int _fe_count;
static void _fe_counter(double x) {
  (void)x;
  _fe_count++;
}

static void test_s17(void) {
  _section("§17 functional toolkit");

  double arr[4] = {1.0, 2.0, 3.0, 4.0};
  MAP_D(arr, 4, _sq);
  TEST("MAP_D [1,4,9,16]", _feq(arr[0], 1.0) && _feq(arr[1], 4.0) &&
                               _feq(arr[2], 9.0) && _feq(arr[3], 16.0));

  double src[3] = {1.0, 2.0, 3.0}, dst2[3] = {0};
  MAP_D_INTO(src, dst2, 3, _sq);
  TEST("MAP_D_INTO src unchanged", _feq(src[0], 1.0));
  TEST("MAP_D_INTO dst[2]==9", _feq(dst2[2], 9.0));

  long larr[3] = {1, 2, 3};
  MAP_L(larr, 3, _dbl_l);
  TEST("MAP_L [2,4,6]", larr[0] == 2 && larr[1] == 4 && larr[2] == 6);

  double farr[5] = {-1.0, 2.0, -3.0, 4.0, 5.0}, fout[5];
  long flen;
  FILTER_D(farr, 5, fout, &flen, _pos_d);
  TEST("FILTER_D keeps 3", flen == 3);
  TEST("FILTER_D [0]==2.0", _feq(fout[0], 2.0));
  TEST("FILTER_D [2]==5.0", _feq(fout[2], 5.0));

  long ifarr[6] = {1, 2, 3, 4, 5, 6}, ifout[6];
  long iflen;
  FILTER_L(ifarr, 6, ifout, &iflen, _even_l);
  TEST("FILTER_L keeps 3", iflen == 3);
  TEST("FILTER_L [0]==2", ifout[0] == 2);

  double rarr[4] = {1.0, 2.0, 3.0, 4.0};
  TEST("REDUCE_D sum==10", _feq(REDUCE_D(rarr, 4, 0.0, _add_d), 10.0));
  TEST("REDUCE_D prod==24", _feq(REDUCE_D(rarr, 4, 1.0, _mul_d), 24.0));
  TEST("REDUCE_D max==4", _feq(REDUCE_D(rarr, 4, rarr[0], _dmax), 4.0));

  long rlarr[4] = {1, 2, 3, 4};
  TEST("REDUCE_L sum==10", REDUCE_L(rlarr, 4, 0, _add_l) == 10);

  _fe_count = 0;
  double fearr[3] = {1.0, 1.0, 1.0};
  FOR_EACH_D(fearr, 3, _fe_counter);
  TEST("FOR_EACH_D ran 3x", _fe_count == 3);

  double za[3] = {1.0, 2.0, 3.0}, zb[3] = {4.0, 5.0, 6.0}, zout[3];
  ZIP_D(za, zb, zout, 3, _add_zip);
  TEST("ZIP_D [5,7,9]",
       _feq(zout[0], 5.0) && _feq(zout[1], 7.0) && _feq(zout[2], 9.0));

  TEST("STR_FROM_DOUBLE>=1", STR_LEN(STR_FROM_DOUBLE(1.0)) >= 1);
  TEST("STR_FROM_INT==2", STR_LEN(STR_FROM_INT(99)) == 2);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §18 — Namespaces
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s18(void) {
  _section("§18 namespaces");
  long uid = GETUID(), gid = GETGID();
  TEST("GETUID >= 0", uid >= 0);
  TEST("GETGID >= 0", gid >= 0);

  long r = UNSHARE(CLONE_NEWUSER);
  if (r == 0) {
    TEST("UNSHARE CLONE_NEWUSER ok", 1);
    TEST("GETUID after userns >= 0", GETUID() >= 0);
  } else {
    SKIP("UNSHARE CLONE_NEWUSER", "kernel denied");
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §19 — Bump allocator
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s19(void) {
  _section("§19 bump allocator");
  BumpAlloc b;
  int r = BUMP_INIT(&b, 256);
  TEST("BUMP_INIT returns 0", r == 0);
  TEST("BUMP_USED after init==0", BUMP_USED(&b) == 0);
  TEST("BUMP_LEFT after init>=248", BUMP_LEFT(&b) >= 248);

  void *p1 = BUMP_ALLOC(&b, 8);
  void *p2 = BUMP_ALLOC(&b, 8);
  TEST("BUMP_ALLOC != NULL", p1 != (void *)0 && p2 != (void *)0);
  TEST("BUMP_ALLOC p1 != p2", p1 != p2);
  TEST("BUMP_ALLOC aligned", ((long)p1 & 7) == 0);
  TEST("BUMP_USED after 2x8==16", BUMP_USED(&b) == 16);

  *(long *)p1 = 0xDEAD;
  *(long *)p2 = 0xBEEF;
  TEST("BUMP slot p1 write", *(long *)p1 == 0xDEAD);
  TEST("BUMP slot p2 write", *(long *)p2 == 0xBEEF);

  BUMP_RESET(&b);
  TEST("BUMP_RESET used==0", BUMP_USED(&b) == 0);
  void *p3 = BUMP_ALLOC(&b, 8);
  TEST("after reset reuse p1", p3 == p1);

  BumpAlloc tiny_b;
  BUMP_INIT(&tiny_b, 8);
  BUMP_ALLOC(&tiny_b, 8);
  TEST("BUMP exhausted → NULL", BUMP_ALLOC(&tiny_b, 8) == (void *)0);

  /* variable sizes — all aligned */
  BumpAlloc vb;
  BUMP_INIT(&vb, 128);
  void *va = BUMP_ALLOC(&vb, 3);
  void *vb2 = BUMP_ALLOC(&vb, 17);
  TEST("BUMP variable != NULL", va != (void *)0 && vb2 != (void *)0);
  TEST("BUMP variable aligned", ((long)vb2 & 7) == 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §20 — Terminal & game loop
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_s20(void) {
  _section("§20 terminal & game loop");

  int pk = POLL_KEY(0);
  TEST("POLL_KEY(0) returns 0 or 1", pk == 0 || pk == 1);

  int tr = TERM_RAW();
  if (tr == 0) {
    TEST("TERM_RAW on TTY ok", 1);
    TERM_RESET();
    TEST("TERM_RESET ok", 1);
  } else {
    SKIP("TERM_RAW", "not a TTY");
    SKIP("TERM_RESET", "not a TTY");
  }

  CLEAR_SCREEN();
  MOVE_CURSOR(1, 1);
  CURSOR_HIDE();
  CURSOR_SHOW();
  TEST("ANSI macros no crash", 1);

  TEST("KEY_ESC==27", KEY_ESC == 27);
  TEST("KEY_CTRL('c')==3", KEY_CTRL('c') == 3);
  TEST("KEY_ENTER=='\\n'", KEY_ENTER == '\n');
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Summary & entry
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void print_summary(void) {
  int total = _t_pass + _t_fail + _t_skip;
  PRINTLN_STR("", 0);
  PRINTLN_STR("════════════════════════════════════════════════════", 51);
  PRINT_STR("  TOTAL  ", 9);
  PRINTLN_INT(total);
  PRINT_STR("  PASS   ", 9);
  PRINTLN_INT(_t_pass);
  PRINT_STR("  FAIL   ", 9);
  PRINTLN_INT(_t_fail);
  PRINT_STR("  SKIP   ", 9);
  PRINTLN_INT(_t_skip);
  PRINTLN_STR("════════════════════════════════════════════════════", 51);
  if (_t_fail == 0)
    PRINTLN_STR("  ALL TESTS PASSED", 18);
  else
    PRINTLN_STR("  FAILURES DETECTED", 19);
  PRINTLN_STR("", 0);
}

BEGIN
STR_PRINTLN(S_PTR("tiny.h test suite", 17));
PRINTLN_STR("════════════════════════════════════════════════════", 51);

test_s1();
test_s2();
test_s3();
test_s4();
test_s5();
test_s6();
test_s7();
test_s8();
test_s9();
test_s10();
test_s11();
test_s12();
test_s13();
test_s14();
test_s15();
test_s16();
test_s17();
test_s18();
test_s19();
test_s20();

print_summary();
if (_t_fail > 0)
  EXIT(1);
END
