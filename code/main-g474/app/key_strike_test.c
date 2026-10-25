/* Host unit tests for key_strike. Pure C, no HAL — compile and run
 * natively (see `just test`). Same framework as bellow_curve_test.c. */

#include "key_strike.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                \
  do {                                                             \
    g_checks++;                                                    \
    if (!(cond)) {                                                 \
      g_failures++;                                                \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                               \
  } while (0)

#define START   2400
#define PRESS   1900
#define RELEASE 2100
#define REST    2800

/* One frame; returns the speed if it crossed the press threshold, else -1. */
static int32_t strike(key_strike_t *s, uint16_t v, uint32_t t)
{
  int32_t speed;
  return key_strike_update(s, v, t, START, PRESS, RELEASE, &speed) == KEY_EDGE_PRESS ? speed : -1;
}

/* One frame; returns the speed if it crossed the release threshold, else -1. */
static int32_t release(key_strike_t *s, uint16_t v, uint32_t t)
{
  int32_t speed;
  return key_strike_update(s, v, t, START, PRESS, RELEASE, &speed) == KEY_EDGE_RELEASE ? speed : -1;
}

/* Plays a linear fall from REST down to below PRESS at `counts_per_ms`, one
 * frame every `frame_us` (with `skip` frames out of every `skip+1` lost), and
 * returns the speed reported when the press threshold is crossed, or -1. */
static int32_t play(float counts_per_ms, uint32_t frame_us, int drop_every,
                    uint32_t t_origin, int noise)
{
  key_strike_t s;
  key_strike_reset(&s);
  int32_t speed = -1;
  srand(7);
  for (int i = 0; i < 4000; i++)
  {
    if (drop_every && i % drop_every == 1) continue; /* frame never seen */
    uint32_t t = t_origin + (uint32_t)i * frame_us;
    float pos = REST - counts_per_ms * (float)i * (float)frame_us / 1000.0f;
    if (pos < 1000) pos = 1000;
    int n = noise ? (rand() % (2 * noise + 1)) - noise : 0;
    int32_t r = strike(&s, (uint16_t)(pos + n), t);
    if (r >= 0 && speed < 0) speed = r;
  }
  return speed;
}

/* Holds the key at 1000 for a while, then plays a linear rise back to REST at
 * `counts_per_ms`; returns the speed reported when release is crossed, or -1. */
static int32_t play_release(float counts_per_ms, uint32_t frame_us, int noise)
{
  key_strike_t s;
  key_strike_reset(&s);
  int32_t speed = -1;
  srand(11);
  uint32_t t = 0;
  for (int i = 0; i < 10; i++, t += frame_us) release(&s, 1000, t);
  for (int i = 0; i < 4000; i++, t += frame_us)
  {
    float pos = 1000 + counts_per_ms * (float)i * (float)frame_us / 1000.0f;
    if (pos > REST) pos = REST;
    int n = noise ? (rand() % (2 * noise + 1)) - noise : 0;
    int32_t r = release(&s, (uint16_t)(pos + n), t);
    if (r >= 0 && speed < 0) speed = r;
  }
  return speed;
}

static int near(int32_t got, int32_t want, int pct)
{
  return abs(got - want) * 100 <= want * pct;
}

/* The span START-PRESS is 500 counts, so a fall of 1 count/ms is 10 counts per
 * 10 ms. */
static void test_speed_clean(void)
{
  CHECK(near(play(50.0f, 500, 0, 0, 0), 500, 3));
  CHECK(near(play(10.0f, 500, 0, 0, 0), 100, 3));
  CHECK(near(play(200.0f, 500, 0, 0, 0), 2000, 3));
}

/* A strike faster than one frame interval crosses both thresholds in the same
 * step: the interpolation must still give the slope, not zero time. */
static void test_both_thresholds_in_one_frame(void)
{
  CHECK(near(play(1000.0f, 1000, 0, 0, 0), 10000, 5));
}

static void test_speed_with_noise(void)
{
  CHECK(near(play(50.0f, 500, 0, 0, 5), 500, 8));
}

/* Dropped frames only lengthen the spacing; the timestamps still say by how
 * much, so the speed must not change. */
static void test_dropped_frames(void)
{
  CHECK(near(play(50.0f, 250, 3, 0, 0), 500, 4));
  CHECK(near(play(50.0f, 250, 2, 0, 0), 500, 4));
}

/* The microsecond counter wraps; only differences matter. */
static void test_time_wraparound(void)
{
  CHECK(near(play(50.0f, 500, 0, 0xFFFFF000u, 0), 500, 3));
}

/* Starting below the start threshold (first frame after boot): no transit was
 * seen, so the slowest speed is reported rather than a made-up one. */
static void test_no_transit(void)
{
  key_strike_t s;
  key_strike_reset(&s);
  CHECK(strike(&s, 2100, 0) == -1);
  CHECK(strike(&s, 1800, 1000) == KEY_STRIKE_NO_SPEED);
}

/* A stroke that starts, goes back up above the start threshold, and later
 * presses again must be timed from the second descent only. */
static void test_abandoned_stroke(void)
{
  key_strike_t s;
  key_strike_reset(&s);
  CHECK(strike(&s, 2800, 0) == -1);
  CHECK(strike(&s, 2300, 1000) == -1);     /* below start */
  CHECK(strike(&s, 2700, 2000) == -1);     /* back up */
  CHECK(strike(&s, 2800, 300000) == -1);
  /* Fast second descent: 2800 -> 1500 over 2 frames of 1 ms. */
  CHECK(strike(&s, 2150, 301000) == -1);
  int32_t r = strike(&s, 1500, 302000);
  CHECK(r >= 0);
  CHECK(near(r, 6500, 5));                 /* 650 counts/ms */
}

/* A very slow transit is clamped, not wrapped to a small time. */
static void test_slow_transit_clamped(void)
{
  key_strike_t s;
  key_strike_reset(&s);
  strike(&s, 2800, 0);
  strike(&s, 2300, 1000);
  int32_t r = strike(&s, 1800, 5000000);
  CHECK(r >= 0);
  CHECK(r < 20);
}

/* The span RELEASE-PRESS is 200 counts, so a rise of 1 count/ms is 10 counts
 * per 10 ms, as for the strike. */
static void test_release_speed(void)
{
  CHECK(near(play_release(20.0f, 500, 0), 200, 3));
  CHECK(near(play_release(80.0f, 500, 0), 800, 3));
  CHECK(near(play_release(500.0f, 1000, 0), 5000, 5));  /* both thresholds in one frame */
  CHECK(near(play_release(20.0f, 500, 3), 200, 10));
}

/* A full press and release on one state: each edge is reported once, with
 * its own transit. */
static void test_press_then_release(void)
{
  key_strike_t s;
  key_strike_reset(&s);
  int32_t speed = -1;
  uint32_t t = 0;
  int presses = 0, releases = 0;
  int32_t press_speed = -1, release_speed = -1;
  /* Down at 50 counts/ms, hold, up at 20 counts/ms, one frame per 500 us. */
  for (int i = 0; i <= 40; i++, t += 500)
  {
    key_edge_t e = key_strike_update(&s, (uint16_t)(REST - 25 * i), t, START, PRESS, RELEASE, &speed);
    if (e == KEY_EDGE_PRESS) { presses++; press_speed = speed; }
    if (e == KEY_EDGE_RELEASE) releases++;
  }
  for (int i = 0; i < 20; i++, t += 500)
    CHECK(key_strike_update(&s, 1800, t, START, PRESS, RELEASE, &speed) == KEY_EDGE_NONE);
  for (int i = 0; i <= 100; i++, t += 500)
  {
    key_edge_t e = key_strike_update(&s, (uint16_t)(1800 + 10 * i), t, START, PRESS, RELEASE, &speed);
    if (e == KEY_EDGE_PRESS) presses++;
    if (e == KEY_EDGE_RELEASE) { releases++; release_speed = speed; }
  }
  CHECK(presses == 1);
  CHECK(releases == 1);
  CHECK(near(press_speed, 500, 3));
  CHECK(near(release_speed, 200, 3));
}

/* A release that began above press (first frame after boot, key half held):
 * no transit was seen, so the slowest speed. */
static void test_release_no_transit(void)
{
  key_strike_t s;
  key_strike_reset(&s);
  CHECK(release(&s, 2000, 0) == -1);
  CHECK(release(&s, 2200, 1000) == KEY_STRIKE_NO_SPEED);
}

/* Lifted above press, pushed back below it, then released: timed from the
 * second rise only. */
static void test_abandoned_release(void)
{
  key_strike_t s;
  key_strike_reset(&s);
  CHECK(release(&s, 1700, 0) == -1);
  CHECK(release(&s, 2000, 1000) == -1);    /* above press */
  CHECK(release(&s, 1800, 2000) == -1);    /* back down */
  CHECK(release(&s, 1800, 300000) == -1);
  /* Fast second rise: 1800 -> 2300 over 1 ms. */
  int32_t r = release(&s, 2300, 301000);
  CHECK(r >= 0);
  CHECK(near(r, 5000, 5));                 /* 500 counts/ms */
}

static key_strike_map_t default_map(void)
{
  key_strike_map_t m = {
    .speed_min = 50, .speed_max = 1500, .vel_min = 20, .vel_max = 127,
    .cx1 = 102, .cy1 = 0, .cx2 = 154, .cy2 = 256, .log_scale = false,
  };
  return m;
}

static void test_mapping_limits(void)
{
  key_strike_map_t m = default_map();
  CHECK(key_strike_velocity(&m, 0) == 20);
  CHECK(key_strike_velocity(&m, 50) == 20);
  CHECK(key_strike_velocity(&m, 1500) == 127);
  CHECK(key_strike_velocity(&m, 60000) == 127);
}

/* Monotonic and S-shaped: flatter than the diagonal at both ends, steeper in
 * the middle. */
static void test_mapping_s_shape(void)
{
  key_strike_map_t m = default_map();
  uint8_t prev = 0;
  for (int32_t sp = 0; sp <= 1600; sp += 10)
  {
    uint8_t v = key_strike_velocity(&m, sp);
    CHECK(v >= prev);
    prev = v;
  }
  float mid = key_strike_normalize(&m, 775);
  CHECK(mid > 0.45f && mid < 0.55f);
  CHECK(key_strike_normalize(&m, 50 + 145) < 0.1f);   /* 10% of span */
  CHECK(key_strike_normalize(&m, 50 + 1305) > 0.9f);  /* 90% of span */
}

static void test_mapping_log(void)
{
  key_strike_map_t m = default_map();
  m.log_scale = true;
  m.cx1 = 85; m.cy1 = 85; m.cx2 = 171; m.cy2 = 171;     /* ~linear curve */
  float geo = key_strike_normalize(&m, 274);            /* sqrt(50*1500) ~ 274 */
  CHECK(geo > 0.47f && geo < 0.53f);
  CHECK(key_strike_velocity(&m, 0) == 20);
  CHECK(key_strike_velocity(&m, 1500) == 127);
}

static void test_mapping_degenerate(void)
{
  key_strike_map_t m = default_map();
  m.speed_max = m.speed_min;                            /* no span */
  CHECK(key_strike_velocity(&m, 10) == 20);
  CHECK(key_strike_velocity(&m, 100) == 127);
  m = default_map();
  m.vel_min = 0;                                         /* never a NOTE OFF */
  CHECK(key_strike_velocity(&m, 0) == 1);
}

int main(void)
{
  test_speed_clean();
  test_both_thresholds_in_one_frame();
  test_speed_with_noise();
  test_dropped_frames();
  test_time_wraparound();
  test_no_transit();
  test_abandoned_stroke();
  test_slow_transit_clamped();
  test_release_speed();
  test_press_then_release();
  test_release_no_transit();
  test_abandoned_release();
  test_mapping_limits();
  test_mapping_s_shape();
  test_mapping_log();
  test_mapping_degenerate();
  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures ? 1 : 0;
}
