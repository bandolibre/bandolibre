/* Host unit tests for bellow_phys_step. Pure C, no HAL — compile and run
 * natively (see `just test`). Same framework as hysteresis_test.c. */

#include "bellow_phys.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                \
  do {                                                             \
    g_checks++;                                                    \
    if (!(cond)) {                                                 \
      g_failures++;                                                \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                              \
  } while (0)

/* Default params matching property_table.def defaults (dir_dead/dir_hyst
 * rescaled x16 for the BELLOW_INTENSITY_MAX=16384 pressure domain). */
static bellow_phys_params_t default_params(void)
{
  bellow_phys_params_t p = {
    .track_ms     = 40,
    .damping      = 256,   /* critical, zeta=1.0 */
    .impulse_gain = 64,
    .leak_quiet   = 64,
    .leak_per_key = 128,
    .dir_dead     = 1024,
    .dir_hyst     = 512,
  };
  return p;
}

/* Run N steps of dt_s seconds each at constant F with given keys. */
static void run_steps(bellow_phys_state_t *s, float F, float dt_s, uint16_t keys,
                      const bellow_phys_params_t *p, int n)
{
  for (int i = 0; i < n; i++)
    bellow_phys_step(s, F, dt_s, keys, p);
}

/* With no leakage, P must converge to F within 1% after 5 time constants.
 * omega = 1000/40 = 25 rad/s -> time constant 1/omega = 40ms.
 * 5 time constants at critical damping: ~200ms, so run 250 steps of 1ms. */
static void test_steady_state_no_leak(void)
{
  bellow_phys_params_t p = default_params();
  p.leak_quiet = 0;
  p.leak_per_key = 0;
  p.impulse_gain = 0;   /* isolate oscillator behaviour */
  bellow_phys_state_t s = {0};

  run_steps(&s, 500.0f, 0.001f, 0, &p, 500);

  /* P should have converged to within 1% of F=500. */
  CHECK(fabsf(s.p - 500.0f) < 5.0f);
}

/* With quiet leak, P settles below F: P_ss = F / (1 + 2*zeta*L/omega).
 * L = 64/256 = 0.25/s, omega = 25, zeta = 1: P_ss = 500 / (1 + 0.02) = 490.2.
 * Check P is below F and within the expected range. */
static void test_steady_state_with_leak(void)
{
  bellow_phys_params_t p = default_params();
  p.impulse_gain = 0;
  bellow_phys_state_t s = {0};

  run_steps(&s, 500.0f, 0.001f, 0, &p, 1000);

  CHECK(s.p < 500.0f);             /* leak causes droop below F */
  CHECK(s.p > 480.0f);             /* but not more than ~4% below F */
}

/* Impulse: a brief force spike stores energy that keeps P elevated after F=0.
 * omega=25 -> impulse window ~1/omega = 40ms. Run spike for 10ms, then remove
 * force; after 40ms P should still be well above a threshold. */
static void test_impulse_stores_energy(void)
{
  bellow_phys_params_t p = default_params();
  p.leak_quiet = 0;
  p.leak_per_key = 0;
  bellow_phys_state_t s = {0};

  /* 10ms spike at full force */
  run_steps(&s, (float)BELLOW_INTENSITY_MAX, 0.001f, 0, &p, 10);
  /* Force removed, let the stored momentum pump pressure for 40ms */
  run_steps(&s, 0.0f, 0.001f, 0, &p, 40);

  /* After the spike and 40ms decay with no force, substantial pressure remains. */
  CHECK(s.p > 50.0f);
}

/* Keys held increase the leak rate, so P settles lower than with no keys. */
static void test_key_leak_softens_pressure(void)
{
  bellow_phys_params_t p = default_params();
  p.impulse_gain = 0;
  bellow_phys_state_t s_no_keys = {0};
  bellow_phys_state_t s_with_keys = {0};

  run_steps(&s_no_keys,   300.0f, 0.001f, 0, &p, 1000);
  run_steps(&s_with_keys, 300.0f, 0.001f, 5, &p, 1000);

  CHECK(s_with_keys.p < s_no_keys.p);
}

/* P is clamped to ±BELLOW_INTENSITY_MAX even for extreme F input. F must be
 * far beyond the clamp (not just beyond the old 1024 ceiling) to actually
 * exercise it at the wider 16384 range within the step budget. */
static void test_pressure_clamps_at_max(void)
{
  bellow_phys_params_t p = default_params();
  p.leak_quiet = 0;
  bellow_phys_state_t s = {0};

  run_steps(&s, 100000.0f, 0.001f, 0, &p, 200);
  CHECK(s.p <= (float)BELLOW_INTENSITY_MAX);

  bellow_phys_state_t s2 = {0};
  run_steps(&s2, -100000.0f, 0.001f, 0, &p, 200);
  CHECK(s2.p >= -(float)BELLOW_INTENSITY_MAX);
}

/* Direction stays NEUTRAL when |P| is below the deadzone threshold.
 * dir_dead=1024, dir_hyst=512 -> push_edge = 0 - 512/2 - 1024/2 = -768.
 * With F=-10 and no impulse, P should stay well above -768 and direction
 * stay NEUTRAL. */
static void test_direction_deadzone_neutral(void)
{
  bellow_phys_params_t p = default_params();
  p.impulse_gain = 0;
  p.leak_quiet = 0;
  bellow_phys_state_t s = {0};

  /* Apply a gentle force that will settle P well below 768 in magnitude. */
  run_steps(&s, -10.0f, 0.001f, 0, &p, 500);

  CHECK(s.eff_dir == BELLOWS_NEUTRAL);
}

/* Direction commits to PUSH when |P| exceeds the deadzone threshold. */
static void test_direction_commits_to_push(void)
{
  bellow_phys_params_t p = default_params();
  p.leak_quiet = 0;
  p.impulse_gain = 0;
  bellow_phys_state_t s = {0};

  /* F=-2000 will settle P well past -768 (the push_edge threshold). */
  run_steps(&s, -2000.0f, 0.001f, 0, &p, 500);

  CHECK(s.eff_dir == BELLOWS_PUSH);
}

/* Direction hysteresis: once in PUSH, P must recover past the return threshold
 * before going NEUTRAL. push_edge = -768, return = push_edge + hyst = -768+512
 * = -256. So from PUSH, P must go above -256 to return to NEUTRAL. */
static void test_direction_hysteresis(void)
{
  bellow_phys_params_t p = default_params();
  p.leak_quiet = 0;
  p.leak_per_key = 0;
  p.impulse_gain = 0;
  bellow_phys_state_t s = {0};

  /* Drive into PUSH */
  run_steps(&s, -2000.0f, 0.001f, 0, &p, 300);
  CHECK(s.eff_dir == BELLOWS_PUSH);

  /* Reduce F to a value that will settle P around -400 (inside deadzone entry
   * but outside hysteresis return threshold of -256). Should stay PUSH. */
  run_steps(&s, -400.0f, 0.001f, 0, &p, 500);
  CHECK(s.eff_dir == BELLOWS_PUSH);

  /* Remove force entirely: P decays to 0, crossing -256 -> NEUTRAL. */
  run_steps(&s, 0.0f, 0.001f, 0, &p, 500);
  CHECK(s.eff_dir == BELLOWS_NEUTRAL);
}

/* With F=0 and any leak, P decays from a charged state to near zero.
 * This mirrors what happens when the naive model gates F=0 at rest. */
static void test_decays_to_zero_when_force_removed(void)
{
  bellow_phys_params_t p = default_params();
  p.impulse_gain = 0;
  bellow_phys_state_t s = {0};

  run_steps(&s, 500.0f, 0.001f, 0, &p, 1000);
  CHECK(s.p > 400.0f);

  run_steps(&s, 0.0f, 0.001f, 0, &p, 2000);
  CHECK(fabsf(s.p) < 5.0f);
}

int main(void)
{
  test_steady_state_no_leak();
  test_steady_state_with_leak();
  test_impulse_stores_energy();
  test_key_leak_softens_pressure();
  test_pressure_clamps_at_max();
  test_direction_deadzone_neutral();
  test_direction_commits_to_push();
  test_direction_hysteresis();
  test_decays_to_zero_when_force_removed();

  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
