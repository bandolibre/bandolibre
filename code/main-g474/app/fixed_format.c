#include "fixed_format.h"

#include <stdint.h>

/* Copies s into buf, truncated to fit; buf is always NUL-terminated when
 * size > 0. */
static char *copy_out(char *buf, size_t size, const char *s)
{
  if (size == 0) return buf;
  size_t i = 0;
  for (; s[i] != '\0' && i + 1 < size; i++) buf[i] = s[i];
  buf[i] = '\0';
  return buf;
}

char *format_fixed(char *buf, size_t size, float v, unsigned decimals, bool plus)
{
  static const uint32_t k_scale[FORMAT_FIXED_MAX_DECIMALS + 1] = {1, 10, 100, 1000, 10000};

  if (decimals > FORMAT_FIXED_MAX_DECIMALS) decimals = FORMAT_FIXED_MAX_DECIMALS;
  if (v != v) return copy_out(buf, size, "nan");

  bool negative = v < 0.0f;
  float a = negative ? -v : v;
  /* Below 4e9 the integer part, plus a carry from rounding, fits a uint32_t. */
  if (!(a < 4.0e9f)) return copy_out(buf, size, negative ? "-ovf" : plus ? "+ovf" : "ovf");

  /* Integer and fractional parts separately: a - whole is exact in float, so
   * the digits are as precise as v itself, which scaling v first would lose
   * once v * 10^decimals passes 2^24. */
  uint32_t scale = k_scale[decimals];
  uint32_t whole = (uint32_t)a;
  uint32_t frac = (uint32_t)((a - (float)whole) * (float)scale + 0.5f);
  if (frac >= scale) { frac -= scale; whole++; }  /* e.g. 0.9996 at 3 decimals */

  /* Built right to left: fractional digits, point, integer digits, sign. */
  char tmp[FORMAT_FIXED_BUF_SIZE];
  char *p = tmp + sizeof tmp;
  *--p = '\0';
  bool zero = (whole == 0 && frac == 0);
  for (unsigned i = 0; i < decimals; i++) { *--p = (char)('0' + frac % 10); frac /= 10; }
  if (decimals > 0) *--p = '.';
  do { *--p = (char)('0' + whole % 10); whole /= 10; } while (whole != 0);
  if (negative && !zero) *--p = '-';
  else if (plus) *--p = '+';
  return copy_out(buf, size, p);
}
