#ifndef APP_BELLOW_CURVE_H
#define APP_BELLOW_CURVE_H

#include <stdint.h>

/* Cubic-Bezier response shape, reshaping an already-classified
 * bellow_classify() intensity (0..1024) through a curve from (0,0) to
 * (1,1) with two fully free 2D control points P1=(cx1,cy1), P2=(cx2,cy2),
 * each coordinate 0..256 (Q8, i.e. 0..1). This is the same construction
 * as CSS's cubic-bezier() timing function / cubic-bezier.com: dragging a
 * control point sideways changes how suddenly the response arrives at a
 * given output, independent of how high the bulge (cyN) is.
 *
 * Because the input here (a measured bellows travel amount) plays the
 * role of the curve's X axis, and X is no longer pinned to make x(t)=t
 * (as an earlier, simpler version of this module did), evaluating the
 * curve for a given input first requires solving x(t)=input for t, then
 * evaluating y(t). This is the same problem browsers solve to run a CSS
 * cubic-bezier() timing function frame-by-frame (e.g. WebKit's
 * UnitBezier::solveCurveX): Newton-Raphson from a good initial guess,
 * falling back to bisection if the derivative is ever too small to trust
 * (a flat stretch of the curve). Done in float: the STM32G474's hardware
 * FPU makes this cheap (measured: 2-3 Newton iterations for the vast
 * majority of (control point, input) combinations, worst case single
 * digits, bisection fallback never triggered in an exhaustive parameter
 * sweep), and a fixed-point equivalent would need a software-emulated
 * division every iteration (no hardware 64-bit divide on this chip),
 * unlike the earlier closed-form design this replaces which only ever
 * needed a power-of-two shift.
 *
 * Monotonicity: with all four coordinates confined to [0,256], BOTH x(t)
 * and y(t) are individually guaranteed monotonic non-decreasing (each is
 * a cubic Bezier whose own hodograph -- its derivative, itself a Bezier
 * curve -- is a convex combination of control-point-derived values that
 * are provably non-negative whenever that curve's own two control
 * coordinates stay in [0,1]; see the derivation this module's design
 * discussion arrived at). x(t) monotonic makes it invertible, and
 * composing it with a monotonic y(t) keeps the overall y(x) relationship
 * monotonic non-decreasing for the ENTIRE parameter space -- no corner of
 * the four-parameter range can make expression run backwards as the
 * bellows is pushed harder.
 *
 * Tradeoff versus the earlier closed-form design: that version was
 * bit-exact to the pre-existing linear ramp at its shared default
 * (cy1=85, cy2=171). This iterative solve is only exact in the
 * mathematical limit; in practice it lands within a fraction of a part in
 * 1024 of linear at the equivalent default (cx1=cy1=85, cx2=cy2=171, i.e.
 * both control points sitting on the diagonal), not a hard guarantee. */
uint16_t bellow_curve_apply(uint16_t intensity,
                             uint16_t cx1, uint16_t cy1,
                             uint16_t cx2, uint16_t cy2);

#endif /* APP_BELLOW_CURVE_H */
