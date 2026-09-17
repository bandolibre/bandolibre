#ifndef ONE_EURO_FILTER_H_
#define ONE_EURO_FILTER_H_

/* The 1-euro filter (Casiez, Roussel & Vogel, 2012,
 * https://inria.hal.science/hal-00670496): a speed-adaptive low-pass. Unlike a
 * flat-rate EMA or a fixed-width deadband, its cutoff frequency widens with the
 * estimated speed of the signal -- heavy smoothing while the signal is nearly
 * static (kills jitter at rest, where lag is imperceptible anyway), opening up
 * to little smoothing when it's moving fast (so a real, fast gesture isn't
 * lagged or clipped). The speed estimate is itself a small low-pass (dcutoff),
 * so it doesn't just track raw per-sample noise.
 *
 * Self-contained: this header knows nothing about any caller's value range,
 * MIDI, or CC concepts. It filters whatever float it's given and hands back a
 * filtered float; a caller wanting bounds applies them itself, before or after
 * calling one_euro_update(). HAL-free (the caller supplies "now" in
 * milliseconds, e.g. HAL_GetTick()), so this compiles and is unit-tested on the
 * host (see one_euro_filter_test.c) exactly like hysteresis.h.
 *
 * Tuning procedure (the paper's own recommended method):
 *   1) Set beta = 0.
 *   2) Lower mincutoff until jitter at rest is gone. Too low feels sluggish
 *      even at rest -- back off until it just barely settles.
 *   3) Raise beta until fast moves stop lagging or overshooting. Too high lets
 *      jitter back in during fast moves -- back off until it's just enough.
 *   dcutoff (the speed estimate's own cutoff) rarely needs tuning away from its
 *   default of 1.0 Hz. */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  float mincutoff;  /* cutoff (Hz) at zero speed -- lower smooths more at rest */
  float beta;       /* speed coefficient -- higher tracks fast moves with less lag */
  float dcutoff;    /* cutoff (Hz) for the internal speed estimate, typically 1.0 */
} one_euro_config_t;

typedef struct {
  bool     init;      /* false until the first sample seeds the state */
  float    x_prev;    /* last filtered value */
  float    dx_prev;   /* last filtered speed estimate */
  uint32_t last_tick;
} one_euro_state_t;

/* alpha(cutoff, dt) = 1 / (1 + tau/dt), tau = 1/(2*pi*cutoff) -- the standard
 * EMA coefficient for a first-order low-pass at the given cutoff and sample
 * interval. cutoff is assumed > 0 (callers keep mincutoff/dcutoff off zero). */
static inline float one_euro_alpha(float cutoff, float dt)
{
  float tau = 1.0f / (2.0f * 3.14159265358979323846f * cutoff);
  return 1.0f / (1.0f + tau / dt);
}

/* Filters one sample. now_ms follows hyst_update's convention (caller passes
 * HAL_GetTick()). dt is derived from now_ms - last_tick, clamped to [0.001,
 * 0.02]s: the floor avoids a divide-by-zero in one_euro_alpha if called twice
 * within the same millisecond tick, the ceiling guards against a stalled loop
 * blowing up the derivative estimate (the same guard bellow_physical_simulation()
 * already applies to its own dt_s). The first sample seeds x_prev=x, dx_prev=0
 * and returns x unfiltered, matching hyst_update's own "first sample always
 * emits" seeding. */
static inline float one_euro_update(one_euro_state_t *st, const one_euro_config_t *cfg,
                                    float x, uint32_t now_ms)
{
  if (!st->init)
  {
    st->init = true;
    st->x_prev = x;
    st->dx_prev = 0.0f;
    st->last_tick = now_ms;
    return x;
  }

  float dt = (float)(now_ms - st->last_tick) / 1000.0f;
  if (dt < 0.001f) dt = 0.001f;
  if (dt > 0.02f) dt = 0.02f;
  st->last_tick = now_ms;

  /* Speed estimate, itself low-pass filtered at dcutoff. */
  float dx = (x - st->x_prev) / dt;
  float edx = st->dx_prev + one_euro_alpha(cfg->dcutoff, dt) * (dx - st->dx_prev);
  st->dx_prev = edx;

  /* Signal low-pass, cutoff widening with the (magnitude of the) speed estimate. */
  float edx_abs = edx < 0.0f ? -edx : edx;
  float cutoff = cfg->mincutoff + cfg->beta * edx_abs;
  float x_filtered = st->x_prev + one_euro_alpha(cutoff, dt) * (x - st->x_prev);
  st->x_prev = x_filtered;

  return x_filtered;
}

#endif /* ONE_EURO_FILTER_H_ */
