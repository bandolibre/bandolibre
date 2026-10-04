/* Host unit tests for fixed_format. Pure C, no HAL — compile and run
 * natively (see `just test`). Same framework as bellow_curve_test.c. */

#include "fixed_format.h"

#include <stdio.h>
#include <string.h>

static int g_checks;
static int g_failures;

/* Formats v and compares against the expected text, printing both on a
 * mismatch so a failure shows what came out. */
static void check_fmt(int line, float v, unsigned decimals, bool plus, const char *expected)
{
  char buf[FORMAT_FIXED_BUF_SIZE];
  g_checks++;
  const char *got = format_fixed(buf, sizeof buf, v, decimals, plus);
  if (got != buf || strcmp(got, expected) != 0) {
    g_failures++;
    printf("FAIL %s:%d: format_fixed(%g, %u, %d) = \"%s\", expected \"%s\"\n",
           __FILE__, line, (double)v, decimals, plus, got, expected);
  }
}
#define CHECK_FMT(v, d, plus, expected) check_fmt(__LINE__, (v), (d), (plus), (expected))

/* The format strings bellow.c used before: %.3f, %.1f, %.2f, %.4f. */
static void test_printf_equivalents(void)
{
  CHECK_FMT(0.0f, 3, false, "0.000");
  CHECK_FMT(1.0f, 3, false, "1.000");
  CHECK_FMT(0.25f, 3, false, "0.250");
  CHECK_FMT(-0.25f, 3, false, "-0.250");
  CHECK_FMT(28972.5f, 1, false, "28972.5");
  CHECK_FMT(3.125f, 2, false, "3.13");       /* exact tie, rounds away from zero */
  CHECK_FMT(0.0078125f, 4, false, "0.0078"); /* beta = 512/65536 */
  CHECK_FMT(127.9921875f, 2, false, "127.99");/* cc11 16383/128 */
  CHECK_FMT(42.0f, 0, false, "42");          /* no decimals, no point */
}

/* printf's "%+" flag, which bellow.c's force readout used. */
static void test_plus(void)
{
  CHECK_FMT(0.5f, 3, true, "+0.500");
  CHECK_FMT(-0.5f, 3, true, "-0.500");
  CHECK_FMT(0.0f, 3, true, "+0.000");
}

/* Carries out of the fractional part, and leading zeros inside it. */
static void test_rounding(void)
{
  CHECK_FMT(0.9996f, 3, false, "1.000");
  CHECK_FMT(9.96f, 1, false, "10.0");
  CHECK_FMT(-9.96f, 1, false, "-10.0");
  CHECK_FMT(1.05f * 1.0f, 3, false, "1.050");
  CHECK_FMT(2.007f, 3, false, "2.007");
  CHECK_FMT(0.001f, 3, false, "0.001");
}

/* A negative value that rounds to zero loses its sign. */
static void test_negative_zero(void)
{
  CHECK_FMT(-0.0001f, 3, false, "0.000");
  CHECK_FMT(-0.0f, 2, false, "0.00");
  CHECK_FMT(-0.0001f, 3, true, "+0.000");
}

static void test_special_values(void)
{
  float zero = 0.0f;
  CHECK_FMT(zero / zero, 2, false, "nan");
  CHECK_FMT(1.0f / zero, 2, false, "ovf");
  CHECK_FMT(-1.0f / zero, 2, false, "-ovf");
  CHECK_FMT(5.0e9f, 1, true, "+ovf");
  CHECK_FMT(3.0e9f, 4, false, "3000000000.0000");  /* full range at any decimals */
  CHECK_FMT(399999.0f, 4, false, "399999.0000");   /* no precision lost to scaling */
  CHECK_FMT(5.0f, 9, false, "5.0000");             /* decimals clamped to the max */
}

/* The result is truncated to fit and always terminated. */
static void test_truncation(void)
{
  char buf[4];
  g_checks++;
  format_fixed(buf, sizeof buf, 123.456f, 2, false);
  if (strcmp(buf, "123") != 0) { g_failures++; printf("FAIL %s:%d: got \"%s\"\n", __FILE__, __LINE__, buf); }

  char one = 'x';
  g_checks++;
  format_fixed(&one, 1, 7.0f, 0, false);
  if (one != '\0') { g_failures++; printf("FAIL %s:%d: size 1 not terminated\n", __FILE__, __LINE__); }
}

int main(void)
{
  test_printf_equivalents();
  test_plus();
  test_rounding();
  test_negative_zero();
  test_special_values();
  test_truncation();

  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures != 0;
}
