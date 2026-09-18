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
 * this stage entirely (the anchor then tracks the sample exactly every sample)
 * -- a documented, supported configuration, not just an edge case.
 *
 * Pipeline per sample: directional backlash on the sample -> linear
 * scale+clamp to 0..out_max -> rate-limit that coalesces (never drops) a
 * pending change. Any upstream noise suppression (e.g. a 1-euro adaptive
 * low-pass, one_euro_filter.h) is the caller's concern -- this header only
 * ever sees the value it's handed and is the only place that then scales it
 * against in_min/in_max.
 *
 * Thresholds are in raw input units; the period is in milliseconds. HAL-free:
 * the caller passes the current time, so this header compiles and is
 * unit-tested on the host. */

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint16_t in_min, in_max;   /* raw range mapped to 0..out_max (clamped) */
  uint16_t out_max;          /* top output code, e.g. 127 or 16383 */
  uint16_t fwd_thresh;       /* raw units: play in the current direction (small);
                              * 0 with rev_thresh=0 disables the backlash entirely */
  uint16_t rev_thresh;       /* raw units: play required to reverse (larger) */
  uint16_t min_period_ms;    /* min ms between emits; 0 = no rate limit */
} hyst_config_t;

typedef struct {
  bool           init;       /* false until the first sample seeds the state */
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
  /* Seed on first sample so the anchor starts at the real value rather than
   * drifting up from zero. */
  if (!st->init)
  {
    st->init = true;
    st->anchor = sample;
    st->dir = 1;
    st->have_out = false;
  }

  /* 1. Directional backlash on the sample. Moving with dir only needs to clear
   * fwd_thresh; reversing must clear the larger rev_thresh and flips dir. */
  if (st->dir >= 0)
  {
    if (sample > st->anchor + cfg->fwd_thresh)        st->anchor = sample - cfg->fwd_thresh;
    else if (sample + cfg->rev_thresh < st->anchor) { st->anchor = sample + cfg->rev_thresh; st->dir = -1; }
  }
  else
  {
    if (sample + cfg->fwd_thresh < st->anchor)        st->anchor = sample + cfg->fwd_thresh;
    else if (sample > st->anchor + cfg->rev_thresh) { st->anchor = sample - cfg->rev_thresh; st->dir = 1; }
  }

  /* 2. Quantize the anchor to the output range. */
  uint16_t value = hyst_scale(st->anchor, cfg->in_min, cfg->in_max, cfg->out_max);
  *out = value;

  /* 3. Rate-limit without dropping: a change stays pending across polls (the
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
