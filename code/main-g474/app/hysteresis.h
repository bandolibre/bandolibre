#ifndef HYSTERESIS_H_
#define HYSTERESIS_H_

/* Directional ("backlash" / play-operator) hysteresis for turning a noisy,
 * high-resolution sample into a clean low-resolution output (e.g. a MIDI CC),
 * with a per-call decision on whether the output is worth emitting.
 *
 * Why not a symmetric delta: a plain "only change once the sample has moved by
 * +/-delta" dead-band suppresses a clean sample that has genuinely reached the
 * next output code, just because it sits inside the band -> latency for no noise
 * reason. Here the dead zone is asymmetric around an anchor and tied to motion
 * direction: moving WITH the current direction only needs to clear a small
 * forward play (near-zero added latency), while reversing must overcome a larger
 * reverse play; crossing it flips the direction so the thresholds swap. This is
 * the textbook backlash/play operator. Setting fwd_thresh=rev_thresh=0 disables
 * this stage entirely (the anchor then tracks the filtered value exactly every
 * sample) -- a documented, supported configuration, not just an edge case.
 *
 * Pipeline per sample: 1-euro adaptive low-pass (one_euro_filter.h) ->
 * directional backlash on the filtered value -> linear scale+clamp to
 * 0..out_max -> rate-limit that coalesces (never drops) a pending change.
 *
 * Thresholds are in raw input units; the period is in milliseconds. HAL-free:
 * the caller passes the current time, so this header compiles and is
 * unit-tested on the host. hyst_update() is static inline, composing
 * one_euro_update() (also static inline) rather than absorbing its fields or
 * its math -- the filter stays independently meaningful and independently
 * tested (one_euro_filter_test.c); this header only ever passes it a plain
 * float in, reads a plain float back, and is the only place that then scales
 * that value against in_min/in_max. A config built from g_properties at the
 * call site scalarizes away at compile time (still true for the integer
 * fields; the 1-euro stage does real per-sample float work, cheap on the
 * G474's hardware FPU -- see bellow_curve.c's doc comment for the same
 * argument about its own float use). */

#include <stdint.h>
#include <stdbool.h>

#include "one_euro_filter.h"

typedef struct {
  uint16_t in_min, in_max;   /* raw range mapped to 0..out_max (clamped) */
  uint16_t out_max;          /* top output code, e.g. 127 or 16383 */
  uint16_t fwd_thresh;       /* raw units: play in the current direction (small);
                              * 0 with rev_thresh=0 disables the backlash entirely */
  uint16_t rev_thresh;       /* raw units: play required to reverse (larger) */
  one_euro_config_t oe;      /* pre-backlash adaptive low-pass; see one_euro_filter.h */
  uint16_t min_period_ms;    /* min ms between emits; 0 = no rate limit */
} hyst_config_t;

typedef struct {
  bool           init;       /* false until the first sample seeds the state */
  one_euro_state_t oe;       /* 1-euro filter state */
  uint32_t       anchor;     /* backlash anchor in raw units */
  int8_t         dir;        /* +1 rising / -1 falling */
  uint16_t       last_out;   /* last value reported as "push" (valid if have_out) */
  bool           have_out;
  uint32_t       last_emit_ms;
} hyst_state_t;

/* Linear interpolation of a raw value over [min,max] to 0..out_max, clamped at
 * both ends. Returns 0 for a degenerate (max <= min) range. */
static inline uint16_t hyst_scale(uint32_t value, uint16_t min, uint16_t max, uint16_t out_max)
{
  if (max <= min || value <= min) return 0;
  if (value >= max) return out_max;
  return (uint16_t)(((value - min) * (uint32_t)out_max) / (max - min));
}

/* Feed one raw sample. Updates *st, writes the scaled 0..out_max value to *out,
 * and returns true when the caller should push *out over MIDI. */
static inline bool hyst_update(hyst_state_t *st, const hyst_config_t *cfg,
                               uint32_t sample, uint32_t now_ms, uint16_t *out)
{
  /* Seed on first sample so the filter/anchor start at the real value rather
   * than drifting up from zero. */
  if (!st->init)
  {
    st->init = true;
    st->anchor = sample;
    st->dir = 1;
    st->have_out = false;
  }

  /* 1. 1-euro adaptive low-pass. The filter is a separate, self-contained
   * module (one_euro_filter.h): it knows nothing about in_min/in_max/out_max,
   * it just filters the float it's given. */
  float xf = one_euro_update(&st->oe, &cfg->oe, (float)sample, now_ms);
  uint32_t x = (uint32_t)(xf < 0.0f ? 0.0f : xf + 0.5f);

  /* 2. Directional backlash on x. Moving with dir only needs to clear fwd_thresh;
   * reversing must clear the larger rev_thresh and flips dir. */
  if (st->dir >= 0)
  {
    if (x > st->anchor + cfg->fwd_thresh)        st->anchor = x - cfg->fwd_thresh;
    else if (x + cfg->rev_thresh < st->anchor) { st->anchor = x + cfg->rev_thresh; st->dir = -1; }
  }
  else
  {
    if (x + cfg->fwd_thresh < st->anchor)        st->anchor = x + cfg->fwd_thresh;
    else if (x > st->anchor + cfg->rev_thresh) { st->anchor = x - cfg->rev_thresh; st->dir = 1; }
  }

  /* 3. Quantize the anchor to the output range. */
  uint16_t value = hyst_scale(st->anchor, cfg->in_min, cfg->in_max, cfg->out_max);
  *out = value;

  /* 4. Rate-limit without dropping: a change stays pending across polls (the
   * anchor keeps tracking, so we always emit the latest value), and fires as
   * soon as the period has elapsed. The first emit is never delayed. */
  bool changed = !st->have_out || value != st->last_out;
  bool period_ok = !st->have_out || (now_ms - st->last_emit_ms) >= cfg->min_period_ms;
  if (!(changed && period_ok)) return false;

  st->last_out = value;
  st->last_emit_ms = now_ms;
  st->have_out = true;
  return true;
}

#endif /* HYSTERESIS_H_ */
