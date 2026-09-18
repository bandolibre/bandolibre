/* Host unit tests for the directional-hysteresis helper. Pure C, no HAL --
 * compile and run natively (see `just test`). Framework-free: a tiny assert
 * macro that counts failures and reports a final summary. */

#include "hysteresis.h"

#include <stdio.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                               \
  do {                                                            \
    g_checks++;                                                   \
    if (!(cond)) {                                                \
      g_failures++;                                               \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
    }                                                             \
  } while (0)

/* A roomy default config: 0..1000 raw -> 0..100 (10 raw per output step),
 * small forward play, larger reverse play, no rate limit. */
static hyst_config_t base_cfg(void)
{
  hyst_config_t c = {
    .in_min = 0, .in_max = 1000, .out_max = 100,
    .fwd_thresh = 2, .rev_thresh = 30,
    .min_period_ms = 0,
  };
  return c;
}

/* The first sample always emits, seeding the state at its scaled value. */
static void test_first_sample_emits(void)
{
  hyst_config_t cfg = base_cfg();
  hyst_state_t st = {0};
  uint16_t v = 0xFFFF;
  CHECK(hyst_update(&st, &cfg, 500, 0, &v));   /* 500/1000 * 100 = 50 */
  CHECK(v == 50);
}

/* With no forward play, a clean forward move into the next output step emits
 * immediately with zero latency -- the "little to no threshold to move forward"
 * goal. (Forward play, when set, intentionally lags the output by that many raw
 * units; see test_forward_play_lags.) */
static void test_forward_advances_immediately(void)
{
  hyst_config_t cfg = base_cfg();
  cfg.fwd_thresh = 0;
  hyst_state_t st = {0};
  uint16_t v;
  hyst_update(&st, &cfg, 500, 0, &v);          /* seed at 50 */
  CHECK(hyst_update(&st, &cfg, 510, 1, &v));   /* exactly one step up -> 51 */
  CHECK(v == 51);
}

/* Forward play lags forward motion by its width: with fwd=2, input 511 maps to
 * anchor 509, still step 50; it takes input 512 (anchor 510) to reach 51. */
static void test_forward_play_lags(void)
{
  hyst_config_t cfg = base_cfg();             /* fwd_thresh = 2 */
  hyst_state_t st = {0};
  uint16_t v;
  hyst_update(&st, &cfg, 500, 0, &v);          /* seed at 50 */
  CHECK(!hyst_update(&st, &cfg, 511, 1, &v));  /* anchor 509 -> still 50 */
  CHECK(v == 50);
  CHECK(hyst_update(&st, &cfg, 512, 2, &v));   /* anchor 510 -> 51 */
  CHECK(v == 51);
}

/* A reversal smaller than rev_thresh does not move the output (anchor held). */
static void test_small_reversal_held(void)
{
  hyst_config_t cfg = base_cfg();
  hyst_state_t st = {0};
  uint16_t v;
  hyst_update(&st, &cfg, 500, 0, &v);          /* seed at 50, dir +1 */
  CHECK(!hyst_update(&st, &cfg, 480, 1, &v));  /* -20 raw < rev_thresh 30: held */
  CHECK(v == 50);
}

/* A reversal beyond rev_thresh flips direction and emits the new value. */
static void test_large_reversal_flips(void)
{
  hyst_config_t cfg = base_cfg();
  hyst_state_t st = {0};
  uint16_t v;
  hyst_update(&st, &cfg, 500, 0, &v);          /* seed at 50, dir +1 */
  CHECK(hyst_update(&st, &cfg, 460, 1, &v));   /* -40 > rev 30: anchor 460+30=490 -> 49 */
  CHECK(v == 49);
  CHECK(st.dir == -1);
  /* Now moving down only needs fwd play: a further small drop advances. */
  CHECK(hyst_update(&st, &cfg, 449, 2, &v));   /* anchor 449+2=451 -> 45 */
  CHECK(v == 45);
}

/* Rate limiting coalesces but never drops: a change inside the period is held
 * and emitted (as the latest value) once the period elapses. */
static void test_rate_limit_coalesces(void)
{
  hyst_config_t cfg = base_cfg();
  cfg.min_period_ms = 10;
  cfg.fwd_thresh = 0;                          /* isolate rate-limiting from play lag */
  hyst_state_t st = {0};
  uint16_t v;
  CHECK(hyst_update(&st, &cfg, 500, 100, &v)); /* first emit, t=100, v=50 */
  CHECK(!hyst_update(&st, &cfg, 520, 105, &v));/* changed but within period: suppressed */
  CHECK(!hyst_update(&st, &cfg, 600, 108, &v));/* still within period: suppressed */
  /* Period elapsed: the latest value (600 -> 60) is emitted, not the stale 520. */
  CHECK(hyst_update(&st, &cfg, 600, 110, &v));
  CHECK(v == 60);
}

/* fwd_thresh=rev_thresh=0 fully disables the backlash: the anchor tracks the
 * (filter-neutralized) sample exactly every call, so a clean forward move
 * always advances immediately, same as test_forward_advances_immediately but
 * explicitly documenting the "disabled" configuration as a supported case. */
static void test_zero_thresholds_disables_backlash(void)
{
  hyst_config_t cfg = base_cfg();
  cfg.fwd_thresh = 0; cfg.rev_thresh = 0;
  hyst_state_t st = {0};
  uint16_t v;
  hyst_update(&st, &cfg, 500, 0, &v);
  /* With backlash active (fwd_thresh=2, test_forward_play_lags), input 511
   * is held at step 50 until 512. Disabled, it advances at 511 already. */
  CHECK(hyst_update(&st, &cfg, 511, 1, &v));
  CHECK(v == 51);
  CHECK(hyst_update(&st, &cfg, 480, 2, &v));   /* reversal advances just as freely */
  CHECK(v == 48);
}

/* The output clamps to 0 and out_max at the range ends. */
static void test_clamps(void)
{
  hyst_config_t cfg = base_cfg();
  hyst_state_t st = {0};
  uint16_t v;
  hyst_update(&st, &cfg, 0, 0, &v);
  CHECK(v == 0);
  /* Big forward jump past in_max clamps to out_max. */
  hyst_update(&st, &cfg, 5000, 1, &v);
  CHECK(v == 100);
}

/* out_max above the old 7-bit ceiling works -- the 14-bit bellows CC range. */
static void test_wide_out_max(void)
{
  hyst_config_t cfg = base_cfg();
  cfg.in_max = 16384;
  cfg.out_max = 16383;
  cfg.fwd_thresh = 0; cfg.rev_thresh = 0;   /* isolate scaling from backlash */
  hyst_state_t st = {0};
  uint16_t v;
  hyst_update(&st, &cfg, 0, 0, &v);
  CHECK(v == 0);
  hyst_update(&st, &cfg, 16384, 1, &v);
  CHECK(v == 16383);
  hyst_update(&st, &cfg, 8192, 2, &v);
  CHECK(v == 8191);   /* 8192/16384 * 16383, truncated */
}

int main(void)
{
  test_first_sample_emits();
  test_forward_advances_immediately();
  test_forward_play_lags();
  test_small_reversal_held();
  test_large_reversal_flips();
  test_rate_limit_coalesces();
  test_zero_thresholds_disables_backlash();
  test_clamps();
  test_wide_out_max();

  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
