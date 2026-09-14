/* Host unit tests for bellow_curve. Pure C, no HAL — compile and run
 * natively (see `just test`). Same framework as bellow_classify_test.c. */

#include "bellow_curve.h"

#include <stdio.h>
#include <stdint.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                \
  do {                                                             \
    g_checks++;                                                    \
    if (!(cond)) {                                                 \
      g_failures++;                                                \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                               \
  } while (0)

/* P0=(0,0) and P3=(1,1) are fixed regardless of the two free control
 * points, so the endpoints are pinned for any legal (cx1,cy1,cx2,cy2). */
static void test_endpoints(void)
{
  CHECK(bellow_curve_apply(0, 85, 85, 171, 171) == 0);
  CHECK(bellow_curve_apply(1024, 85, 85, 171, 171) == 1024);
  CHECK(bellow_curve_apply(0, 0, 256, 256, 0) == 0);
  CHECK(bellow_curve_apply(1024, 0, 256, 256, 0) == 1024);
  CHECK(bellow_curve_apply(0, 0, 0, 256, 256) == 0);
  CHECK(bellow_curve_apply(1024, 0, 0, 256, 256) == 1024);
}

/* At the default (both control points on the diagonal: cx1=cy1=85,
 * cx2=cy2=171, the exact same numbers the earlier closed-form design
 * used), the curve degenerates to the line y=x -- any point on the
 * diagonal makes a Bezier collinear, hence straight. Verified by hand
 * computation (see the planning discussion) to land within rounding of
 * linear at every one of the 1025 possible inputs; allow +-1 since this
 * design solves iteratively rather than via the old closed form, so it is
 * not guaranteed bit-exact the way that one was. */
static void test_near_linear_at_default(void)
{
  for (uint32_t u = 0; u <= 1024; u++)
  {
    uint16_t v = bellow_curve_apply((uint16_t)u, 85, 85, 171, 171);
    int32_t dev = (int32_t)v - (int32_t)u;
    CHECK(dev >= -1 && dev <= 1);
  }
}

/* A hand-traceable case: cx1=cx2=128 puts the control points' X exactly at
 * the domain midpoint, which (like the old design) makes x(t)=t exactly --
 * so this is really just the old 2-control-point-Y curve in disguise, with
 * cy1=0 (slow start), cy2=256 (steep finish). Values below computed
 * independently in Python with the same algorithm. */
static void test_hand_traceable_case(void)
{
  CHECK(bellow_curve_apply(0, 128, 0, 128, 256) == 0);
  CHECK(bellow_curve_apply(256, 128, 0, 128, 256) == 108);
  CHECK(bellow_curve_apply(512, 128, 0, 128, 256) == 512);
  CHECK(bellow_curve_apply(768, 128, 0, 128, 256) == 916);
  CHECK(bellow_curve_apply(1024, 128, 0, 128, 256) == 1024);
}

/* Degenerate corner: cx1=cx2=0 makes x(t)=t^3, so the solver's derivative
 * at the very start of the curve (t near 0) is close to zero -- exactly
 * the case the bisection fallback exists for. Confirms no crash/NaN and
 * that the result stays monotonic through the degenerate region rather
 * than stalling or oscillating. */
static void test_degenerate_flat_start(void)
{
  static const uint32_t expect[] = {0, 138, 268, 482, 643, 764, 861, 1024};
  static const uint32_t inputs[] = {0, 1, 10, 100, 300, 512, 700, 1024};
  uint16_t prev = 0;
  for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++)
  {
    uint16_t v = bellow_curve_apply((uint16_t)inputs[i], 0, 128, 0, 128);
    CHECK(v == expect[i]);
    CHECK(v >= prev);
    prev = v;
  }
}

/* Monotonicity is proven analytically for the entire (cx1,cy1,cx2,cy2) in
 * [0,256]^4 range (see bellow_curve.h): x(t) and y(t) are each guaranteed
 * monotonic on their own, and a monotonic function of a monotonic function
 * is monotonic. This sweeps a grid of control points and asserts the
 * *coded* solver never produces a decrease as the input rises -- a
 * regression net against implementation bugs (solver mis-convergence,
 * sign errors) that the proof of the intended math wouldn't catch. */
static void test_monotonic_sweep(void)
{
  static const uint16_t vals[] = {0, 64, 128, 192, 256};
  size_t n = sizeof(vals) / sizeof(vals[0]);
  for (size_t a = 0; a < n; a++)
    for (size_t b = 0; b < n; b++)
      for (size_t c = 0; c < n; c++)
        for (size_t d = 0; d < n; d++)
        {
          uint16_t prev = 0;
          for (uint32_t u = 0; u <= 1024; u += 32)
          {
            uint16_t v = bellow_curve_apply((uint16_t)u, vals[a], vals[b], vals[c], vals[d]);
            CHECK(v >= prev);
            prev = v;
          }
        }
}

int main(void)
{
  test_endpoints();
  test_near_linear_at_default();
  test_hand_traceable_case();
  test_degenerate_flat_start();
  test_monotonic_sweep();

  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures != 0;
}
