#include "bellow_curve.h"

/* Evaluates one component (X or Y) of a cubic Bezier at parameter t, given
 * that component's two control coordinates (p1, p2) -- P0=0, P3=1 fixed. */
static float bezier_component(float t, float p1, float p2)
{
  float u = 1.0f - t;
  return 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t;
}

/* Derivative of bezier_component with respect to t. */
static float bezier_component_deriv(float t, float p1, float p2)
{
  float u = 1.0f - t;
  return 3.0f * u * u * p1 + 6.0f * u * t * (p2 - p1) + 3.0f * t * t * (1.0f - p2);
}

static float fabs_f(float v)
{
  return v < 0.0f ? -v : v;
}

/* Solves x(t)=x for t in [0,1] via Newton-Raphson from the identity-curve
 * initial guess (t=x), falling back to bisection if a step's derivative is
 * too small to trust. cx1,cx2 confined to [0,1] (see bellow_curve.h)
 * guarantees x(t) is monotonic non-decreasing, so the root is unique. */
static float solve_t_for_x(float x, float cx1, float cx2)
{
  float t = x;
  for (int i = 0; i < 8; i++)
  {
    float err = bezier_component(t, cx1, cx2) - x;
    if (fabs_f(err) < 1e-4f) return t;
    float d = bezier_component_deriv(t, cx1, cx2);
    if (fabs_f(d) < 1e-6f) break;
    t -= err / d;
    if (t < 0.0f) t = 0.0f;
    else if (t > 1.0f) t = 1.0f;
  }

  float lo = 0.0f, hi = 1.0f;
  for (int i = 0; i < 24; i++)
  {
    t = 0.5f * (lo + hi);
    if (bezier_component(t, cx1, cx2) < x) lo = t;
    else hi = t;
  }
  return t;
}

float bellow_curve_apply(float intensity,
                         uint16_t cx1, uint16_t cy1,
                         uint16_t cx2, uint16_t cy2)
{
  float p1x = (float)cx1 / 256.0f, p1y = (float)cy1 / 256.0f;
  float p2x = (float)cx2 / 256.0f, p2y = (float)cy2 / 256.0f;

  float t = solve_t_for_x(intensity, p1x, p2x);
  float y = bezier_component(t, p1y, p2y);

  if (y < 0.0f) y = 0.0f;
  else if (y > 1.0f) y = 1.0f;

  return y;
}
