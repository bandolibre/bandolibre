#ifndef APP_FIXED_FORMAT_H
#define APP_FIXED_FORMAT_H

#include <stdbool.h>
#include <stddef.h>

/* Float-to-text for console output, without printf's %f.
 *
 * newlib-nano's printf only handles %f when the float formatter is linked in
 * (-u _printf_float), and that formatter brings newlib's double-precision dtoa,
 * its bignum helpers and the software double arithmetic they run on: about
 * 10 KB of flash, an eighth of the Release image, for a few diagnostic lines.
 * format_fixed() rounds to a fixed number of decimals in 32-bit integers
 * instead, and the caller prints the text with %s (%5s where %5.2f gave the
 * field width).
 *
 * Without _printf_float a %f in a format string compiles cleanly and prints
 * nothing, so every float on the console has to come through here.
 *
 * Writes v rounded half away from zero to `decimals` places (at most
 * FORMAT_FIXED_MAX_DECIMALS), e.g. -0.25f with 3 decimals -> "-0.250". With
 * `plus`, a non-negative result gets a leading '+', as printf's "%+" flag did.
 * Rounding is done in float, so a value within float precision of a tie can
 * land one unit off in the last digit from printf (about 2 in 100000 values
 * against glibc), and exact ties go away from zero where printf rounds to even.
 * A value that rounds to zero prints unsigned ("0.000", not "-0.000"). NaN
 * prints as "nan", and a magnitude of 4e9 or more (infinity included) as "ovf".
 * Output longer than `size` is truncated. Returns buf, so the call can sit
 * directly in a printf argument list. */
#define FORMAT_FIXED_MAX_DECIMALS 4
#define FORMAT_FIXED_BUF_SIZE     20  /* longest output is 16 chars, e.g. "-3999999744.0000" */

char *format_fixed(char *buf, size_t size, float v, unsigned decimals, bool plus);

#endif /* APP_FIXED_FORMAT_H */
