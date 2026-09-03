#include <stdio.h>

#include "tiny.h"

static int tests_run;
static int tests_failed;

#define CHECK(description, condition)                                          \
  do {                                                                         \
    tests_run++;                                                               \
    if (condition) {                                                           \
      printf("ok %d - %s\n", tests_run, description);                          \
    } else {                                                                   \
      printf("not ok %d - %s (%s:%d)\n", tests_run, description, __FILE__,     \
             __LINE__);                                                        \
      tests_failed++;                                                          \
    }                                                                          \
  } while (0)

static int double_eq(double left, double right) {
  double difference = left - right;
  if (difference < 0.0)
    difference = -difference;
  return difference < 1e-9;
}

static double square(double value) { return value * value; }
static double add_double(double left, double right) { return left + right; }
static long double_long(long value) { return value * 2; }
static int is_positive(const double *value) { return *value > 0.0; }

static void test_alignment_and_pointers(void) {
  double values[] = {1.0, 2.0, 3.0};

  CHECK("ALIGN_UP rounds zero", ALIGN_UP(0) == 0);
  CHECK("ALIGN_UP rounds to eight bytes", ALIGN_UP(9) == 16);
  CHECK("ALIGN_DOWN rounds to eight bytes", ALIGN_DOWN(15) == 8);
  CHECK("OFFSET advances by bytes",
        OFFSET(values, sizeof(double)) == (void *)&values[1]);
  CHECK("INDEX advances by doubles", INDEX(values, 2) == (void *)&values[2]);
  CHECK("DEREF reads a double", double_eq(DEREF(&values[1]), 2.0));
}

static void test_double_arithmetic(void) {
  double left = 6.0;
  double right = 4.0;
  double result = 0.0;

  SUM(&result, &left, &right);
  CHECK("SUM adds operands", double_eq(result, 10.0));
  SUB(&result, &left, &right);
  CHECK("SUB subtracts operands", double_eq(result, 2.0));
  MUL(&result, &left, &right);
  CHECK("MUL multiplies operands", double_eq(result, 24.0));
  DIV(&result, &left, &right);
  CHECK("DIV divides operands", double_eq(result, 1.5));
  ROOT(&result, &right);
  CHECK("ROOT calculates square root", double_eq(result, 2.0));
  MIN(&result, &left, &right);
  CHECK("MIN selects the smaller operand", double_eq(result, 4.0));
  MAX(&result, &left, &right);
  CHECK("MAX selects the larger operand", double_eq(result, 6.0));
  AVG(&result, &left, &right);
  CHECK("AVG calculates the mean", double_eq(result, 5.0));
}

static void test_comparisons_and_arrays(void) {
  double low = 2.0;
  double high = 5.0;
  double result = 0.0;
  double values[] = {3.0, 1.0, 2.0};
  double sum = 0.0;

  IS_LT(&result, &low, &high);
  CHECK("IS_LT returns a true double flag", double_eq(result, 1.0));
  IS_GT(&result, &low, &high);
  CHECK("IS_GT returns a false double flag", double_eq(result, 0.0));
  CMP(&result, &high, &low);
  CHECK("CMP returns positive ordering", double_eq(result, 1.0));

  ARRAY_SUM(&sum, values, 3);
  CHECK("ARRAY_SUM totals values", double_eq(sum, 6.0));
  ARRAY_SORT(values, 3);
  CHECK("ARRAY_SORT orders ascending",
        double_eq(values[0], 1.0) && double_eq(values[1], 2.0) &&
            double_eq(values[2], 3.0));
  CHECK("ARRAY_FIND locates a value", ARRAY_FIND(values, 3, 2.0) == 1);
}

static void test_integer_helpers(void) {
  CHECK("IS_POW2 accepts powers of two", IS_POW2(16));
  CHECK("IS_POW2 rejects zero", !IS_POW2(0));
  CHECK("IABS returns magnitude", IABS(-7) == 7);
  CHECK("IMIN selects the smaller integer", IMIN(3, 5) == 3);
  CHECK("IMAX selects the larger integer", IMAX(3, 5) == 5);
  CHECK("NEXT_POW2 rounds upward", NEXT_POW2(9) == 16);
  CHECK("POPCOUNT counts set bits", POPCOUNT(0xff) == 8);
  CHECK("TRAILING_ZEROS counts low zero bits", TRAILING_ZEROS(16) == 4);
}

static void test_strings_and_parsing(void) {
  char destination[8] = {0};
  const char *end = 0;
  tiny_str_t inline_string = S("tiny");
  tiny_str_t view = S_VIEW("tiny header", 11);

  CHECK("STRLEN handles literals", STRLEN("tiny") == 4);
  MEM_COPY(destination, "tiny", 4);
  CHECK("MEM_COPY copies bytes", destination[0] == 't' &&
                                         destination[3] == 'y');
  CHECK("ATOI parses signed integers", ATOI("-42", &end) == -42 && *end == 0);
  CHECK("ATOF parses decimals", double_eq(ATOF("3.5", &end), 3.5) && *end == 0);
  CHECK("short strings are inlined",
        STR_IS_INLINED(inline_string) && STR_LEN(inline_string) == 4);
  CHECK("string views preserve length",
        !STR_IS_INLINED(view) && STR_LEN(view) == 11);
  CHECK("STR_EQ_LIT compares content", STR_EQ_LIT(inline_string, "tiny"));
  CHECK("STR_STARTS_WITH matches prefixes", STR_STARTS_WITH(view, "tiny"));
  CHECK("STR_FIND_BYTE returns an index", STR_FIND_BYTE(view, 'h') == 5);
}

static void test_numeric_and_functional_helpers(void) {
  double mapped[] = {1.0, 2.0, 3.0};
  double filtered[3] = {0};
  long filtered_length = 0;
  long longs[] = {1, 2, 3};

  CHECK("CLAMP limits low values", double_eq(CLAMP(-1.0, 0.0, 10.0), 0.0));
  CHECK("LERP interpolates values", double_eq(LERP(0.0, 10.0, 0.5), 5.0));
  CHECK("INT_POW handles positive exponents", INT_POW(2, 8) == 256);
  CHECK("FACTORIAL handles five", FACTORIAL(5) == 120);
  CHECK("IS_LEAP follows century rules", IS_LEAP(2000) && !IS_LEAP(1900));

  MAP_D(mapped, 3, square);
  CHECK("MAP_D transforms in place", double_eq(mapped[2], 9.0));
  CHECK("REDUCE_D folds values",
        double_eq(REDUCE_D(mapped, 3, 0.0, add_double), 14.0));
  MAP_L(longs, 3, double_long);
  CHECK("MAP_L transforms integers", longs[0] == 2 && longs[2] == 6);

  {
    double values[] = {-1.0, 2.0, -3.0};
    FILTER_D(values, 3, filtered, &filtered_length, is_positive);
  }
  CHECK("FILTER_D keeps matching values",
        filtered_length == 1 && double_eq(filtered[0], 2.0));
}

int main(void) {
  puts("TAP version 13");
  test_alignment_and_pointers();
  test_double_arithmetic();
  test_comparisons_and_arrays();
  test_integer_helpers();
  test_strings_and_parsing();
  test_numeric_and_functional_helpers();
  printf("1..%d\n", tests_run);

  if (tests_failed != 0)
    fprintf(stderr, "%d of %d tests failed\n", tests_failed, tests_run);
  return tests_failed == 0 ? 0 : 1;
}