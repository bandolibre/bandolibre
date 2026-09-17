#include "bellow_phys.h"

#include <math.h>

void bellow_phys_step(bellow_phys_state_t *state, float F, float dt_s,
                      uint16_t keys, const bellow_phys_params_t *params)
{
  float omega = 1000.0f / (float)params->track_ms;
  float zeta  = (float)params->damping / 256.0f;

  /* Velocity: explicit impulse kick from the flexion speed (catches a sharp
   * impulse a sluggish oscillator would miss), then the damped restoring
   * drive toward F. Semi-implicit Euler: P below is pumped by this new v. */
  float impulse = (float)params->impulse_gain / 256.0f * (F - state->f_prev);
  if (!params->impulse_directional ||
      (state->p >= 0.0f && impulse >= 0.0f) ||
      (state->p < 0.0f && impulse < 0.0f))
  {
    state->v += impulse;
  }
  state->v += (omega * omega * (F - state->p) - 2.0f * zeta * omega * state->v) * dt_s;
  state->f_prev = F;

  state->p += state->v * dt_s;

  /* Pressure leaks through open pallets: faster the more keys are held. Air
   * escapes whether or not the reed sounds (e.g. keys held at neutral before
   * an impulse). Clamped Euler (factor floored at 0) is stable for any step. */
  float leak = ((float)params->leak_quiet +
                (float)params->leak_per_key * (float)keys) / 256.0f;
  float decay = 1.0f - leak * dt_s;
  if (decay < 0.0f) decay = 0.0f;
  state->p *= decay;

  if (state->p >  (float)BELLOW_INTENSITY_MAX) state->p =  (float)BELLOW_INTENSITY_MAX;
  if (state->p < -(float)BELLOW_INTENSITY_MAX) state->p = -(float)BELLOW_INTENSITY_MAX;

  /* Commit intensity (magnitude, always responsive) and direction (gated
   * through deadzone + hysteresis so push<->pull can't chatter). */
  state->eff_intensity = (uint16_t)(fabsf(state->p) + 0.5f);
  state->eff_dir = bellow_classify(state->eff_dir, (int32_t)state->p, 0,
                                   params->dir_dead, params->dir_hyst, 0, 0).direction;
}
