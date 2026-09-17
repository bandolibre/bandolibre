#ifndef APP_BELLOW_PHYS_H
#define APP_BELLOW_PHYS_H

#include <stdbool.h>
#include <stdint.h>
#include "bellow_classify.h"

/* Parameters for one step of the physical bellows simulation. All /256
 * fixed-point values use 256 = 1.0. */
typedef struct {
  uint16_t track_ms;        /* pressure-tracks-force time (omega = 1000/track_ms) */
  uint16_t damping;         /* damping ratio zeta /256 (256 = critical, no bounce) */
  uint16_t impulse_gain;    /* flexion-speed feed-forward /256 (0 = off) */
  uint16_t leak_quiet;      /* pressure bleed rate with no keys, /256 per second */
  uint16_t leak_per_key;    /* extra bleed per pressed key, /256 per second */
  uint16_t dir_dead;        /* direction deadzone half-width, pressure units */
  uint16_t dir_hyst;        /* direction hysteresis margin, pressure units */
  bool impulse_directional; /* if true, only apply impulse when it reinforces current direction */
} bellow_phys_params_t;

/* Mutable state kept between steps. Zero-initialise on first use. */
typedef struct {
  float     v;            /* bellows velocity (carries momentum) */
  float     p;            /* chamber pressure (signed), the model output */
  float     f_prev;       /* F from the previous step, for impulse feed-forward */
  uint16_t  eff_intensity; /* |P| rounded, 0..BELLOW_INTENSITY_MAX */
  bellows_t eff_dir;      /* direction committed through deadzone + hysteresis */
} bellow_phys_state_t;

/* Integrates one step of the virtual-bellows model.
 *
 *   F      — signed player force, ±BELLOW_INTENSITY_MAX = full travel (same scale as intensity)
 *   dt_s   — time since last step in seconds (caller clamps; pass ≤ 0.02)
 *   keys   — number of keys currently pressed (open pallets, any direction)
 *   params — tuning parameters (read-only)
 *
 * At steady force with no keys, P converges to F * 1/(1 + 2ζL/ω) where
 * L = leak_quiet/256 and ω = 1000/track_ms (≈ 2% below F with defaults).
 * Set leak_quiet = 0 for exact convergence toward the naive model. */
void bellow_phys_step(bellow_phys_state_t *state, float F, float dt_s,
                      uint16_t keys, const bellow_phys_params_t *params);

#endif /* APP_BELLOW_PHYS_H */
