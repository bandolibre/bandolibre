/* Host unit tests for the 1-euro filter. Pure C, no HAL -- compile and run
 * natively (see `just test`). Framework-free: a tiny assert macro that counts
 * failures and reports a final summary. */

#include "one_euro_filter.h"

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

static one_euro_config_t default_cfg(void)
{
  one_euro_config_t c = { .mincutoff = 1.0f, .beta = 0.01f, .dcutoff = 1.0f };
  return c;
}

/* The first sample seeds the state and passes through unfiltered. */
static void test_first_sample_passes_through(void)
{
  one_euro_config_t cfg = default_cfg();
  one_euro_state_t st = {0};
  float y = one_euro_update(&st, &cfg, 42.0f, 0);
  CHECK(y == 42.0f);
  CHECK(st.init);
}

/* A slowly drifting, noisy-looking input at a low mincutoff/beta=0 is smoothed
 * close to flat: a step doesn't fully land in one sample. */
static void test_low_speed_heavy_smoothing(void)
{
  one_euro_config_t cfg = { .mincutoff = 0.5f, .beta = 0.0f, .dcutoff = 1.0f };
  one_euro_state_t st = {0};
  float y;
  y = one_euro_update(&st, &cfg, 0.0f, 0);      /* seed at 0 */
  y = one_euro_update(&st, &cfg, 100.0f, 10);    /* 10ms later, a sharp jump */
  CHECK(y > 0.0f && y < 100.0f);   /* lags -- doesn't jump straight to 100 */
}

/* A fast, sustained step input is tracked within a handful of samples once beta
 * lets the cutoff open up, unlike a fixed low mincutoff alone. */
static void test_high_speed_tracks_with_beta(void)
{
  one_euro_config_t cfg = { .mincutoff = 0.5f, .beta = 5.0f, .dcutoff = 1.0f };
  one_euro_state_t st = {0};
  float y = 0.0f;
  uint32_t t = 0;
  y = one_euro_update(&st, &cfg, 0.0f, t);       /* seed at 0 */
  for (int i = 0; i < 20; i++)
  {
    t += 10;
    y = one_euro_update(&st, &cfg, 1000.0f, t);
  }
  CHECK(y > 950.0f);   /* beta opened the cutoff enough to track the step closely */
}

/* Comparison: the same step input, same duration, but beta=0 (no speed-adaptive
 * widening) lags further behind than the beta>0 case above. */
static void test_beta_reduces_lag_versus_fixed_cutoff(void)
{
  one_euro_config_t cfg = { .mincutoff = 0.5f, .beta = 0.0f, .dcutoff = 1.0f };
  one_euro_state_t st = {0};
  float y = 0.0f;
  uint32_t t = 0;
  y = one_euro_update(&st, &cfg, 0.0f, t);
  for (int i = 0; i < 20; i++)
  {
    t += 10;
    y = one_euro_update(&st, &cfg, 1000.0f, t);
  }
  CHECK(y < 950.0f);   /* without beta, still lagging noticeably behind the step */
}

/* A stalled loop (dt far larger than normal) doesn't blow up the output -- the
 * 0.02s clamp caps how much a single call can move. */
static void test_stalled_loop_dt_is_clamped(void)
{
  one_euro_config_t cfg = default_cfg();
  one_euro_state_t st = {0};
  one_euro_update(&st, &cfg, 0.0f, 0);
  float y = one_euro_update(&st, &cfg, 1000.0f, 5000);  /* 5s gap, would be huge dt unclamped */
  CHECK(y == y);            /* not NaN */
  CHECK(y > -1e6f && y < 1e6f);   /* not blown up */
}

/* A very large mincutoff with beta=0 is numerically indistinguishable from
 * pass-through -- the "disable the filter" case hysteresis_test.c's composed
 * tests rely on to keep their exact expected values. */
static void test_large_mincutoff_is_passthrough(void)
{
  one_euro_config_t cfg = { .mincutoff = 1e6f, .beta = 0.0f, .dcutoff = 1.0f };
  one_euro_state_t st = {0};
  one_euro_update(&st, &cfg, 0.0f, 0);
  float y = one_euro_update(&st, &cfg, 1000.0f, 10);
  CHECK(y > 999.9f);   /* alpha -> 1 as cutoff -> infinity, so this lands within ~0.02% */
}

/* Two calls within the same millisecond tick (dt would be 0) don't divide by
 * zero -- the 0.001s floor guards it. */
static void test_same_tick_no_divide_by_zero(void)
{
  one_euro_config_t cfg = default_cfg();
  one_euro_state_t st = {0};
  one_euro_update(&st, &cfg, 0.0f, 100);
  float y = one_euro_update(&st, &cfg, 50.0f, 100);   /* same now_ms */
  CHECK(y == y);   /* not NaN */
}

int main(void)
{
  test_first_sample_passes_through();
  test_low_speed_heavy_smoothing();
  test_high_speed_tracks_with_beta();
  test_beta_reduces_lag_versus_fixed_cutoff();
  test_stalled_loop_dt_is_clamped();
  test_large_mincutoff_is_passthrough();
  test_same_tick_no_divide_by_zero();

  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
