/* Host unit tests for bellow_classify. Pure C, no HAL — compile and run
 * natively (see `just test`). Same framework as hysteresis_test.c. */

#include "bellow_classify.h"

#include <stdio.h>
#include <stdint.h>

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

static float fabs_f(float v)
{
  return v < 0.0f ? -v : v;
}

/* intensity is a fraction, not a pre-rounded count, so an inexact division
 * (e.g. 1/330) needs a tolerance rather than exact equality. */
#define CHECK_NEAR(a, b) CHECK(fabs_f((a) - (b)) < 1e-6f)

/* Default calibration values matching property_table.def defaults. */
#define CENTER     3760
#define DEAD       40
#define HYST       20
#define FULL_PUSH  3400
#define FULL_PULL  4200

/* Derived thresholds:
 *   push_edge = CENTER - DEAD/2 - HYST/2 = 3760 - 20 - 10 = 3730
 *   pull_edge = CENTER + DEAD/2 + HYST/2 = 3760 + 20 + 10 = 3790
 *   push span = push_edge - FULL_PUSH = 3730 - 3400 = 330
 *   pull span = FULL_PULL - pull_edge = 4200 - 3790 = 410  */
#define PUSH_EDGE  3730
#define PULL_EDGE  3790
#define PUSH_SPAN  330
#define PULL_SPAN  410

static bellow_classify_result_t classify(bellows_t prev, int32_t value)
{
  return bellow_classify(prev, value, CENTER, DEAD, HYST, FULL_PUSH, FULL_PULL);
}

/* At-rest reading: center maps to NEUTRAL with zero intensity. */
static void test_neutral_at_rest(void)
{
  bellow_classify_result_t r = classify(BELLOWS_NEUTRAL, CENTER);
  CHECK(r.direction == BELLOWS_NEUTRAL);
  CHECK(r.intensity == 0.0f);
}

/* One unit past the push edge enters PUSH with near-zero intensity. */
static void test_enter_push_from_neutral(void)
{
  bellow_classify_result_t r = classify(BELLOWS_NEUTRAL, PUSH_EDGE - 1);
  CHECK(r.direction == BELLOWS_PUSH);
  /* d=1, span=330: intensity = 1/330 = 0.00303... (a fraction, not a
   * pre-scaled count -- see bellow_classify_result_t) */
  CHECK_NEAR(r.intensity, 1.0f / 330.0f);
}

/* One unit past the pull edge enters PULL. */
static void test_enter_pull_from_neutral(void)
{
  bellow_classify_result_t r = classify(BELLOWS_NEUTRAL, PULL_EDGE + 1);
  CHECK(r.direction == BELLOWS_PULL);
  /* d=1, span=410: intensity = 1/410 = 0.00244... */
  CHECK_NEAR(r.intensity, 1.0f / 410.0f);
}

/* From PUSH, retreating to push_edge + hyst - 1 stays in PUSH (hysteresis). */
static void test_hysteresis_holds_push(void)
{
  /* push_edge + hyst - 1 = 3730 + 20 - 1 = 3749 */
  bellow_classify_result_t r = classify(BELLOWS_PUSH, PUSH_EDGE + HYST - 1);
  CHECK(r.direction == BELLOWS_PUSH);
  /* value > push_edge so d=0, intensity=0 (in the hysteresis band) */
  CHECK(r.intensity == 0.0f);
}

/* From PUSH, reaching exactly push_edge + hyst returns to NEUTRAL. */
static void test_return_to_neutral_from_push(void)
{
  /* push_edge + hyst = 3730 + 20 = 3750 */
  bellow_classify_result_t r = classify(BELLOWS_PUSH, PUSH_EDGE + HYST);
  CHECK(r.direction == BELLOWS_NEUTRAL);
  CHECK(r.intensity == 0.0f);
}

/* From PULL, retreating to pull_edge - hyst + 1 stays in PULL. */
static void test_hysteresis_holds_pull(void)
{
  /* pull_edge - hyst + 1 = 3790 - 20 + 1 = 3771 */
  bellow_classify_result_t r = classify(BELLOWS_PULL, PULL_EDGE - HYST + 1);
  CHECK(r.direction == BELLOWS_PULL);
  CHECK(r.intensity == 0.0f);
}

/* From PULL, reaching exactly pull_edge - hyst returns to NEUTRAL. */
static void test_return_to_neutral_from_pull(void)
{
  /* pull_edge - hyst = 3790 - 20 = 3770 */
  bellow_classify_result_t r = classify(BELLOWS_PULL, PULL_EDGE - HYST);
  CHECK(r.direction == BELLOWS_NEUTRAL);
  CHECK(r.intensity == 0.0f);
}

/* Intensity is 1.0 (full scale) at full push. */
static void test_intensity_max_at_full_push(void)
{
  bellow_classify_result_t r = classify(BELLOWS_PUSH, FULL_PUSH);
  CHECK(r.direction == BELLOWS_PUSH);
  CHECK(r.intensity == 1.0f);
}

/* Intensity is 1.0 (full scale) at full pull. */
static void test_intensity_max_at_full_pull(void)
{
  bellow_classify_result_t r = classify(BELLOWS_PULL, FULL_PULL);
  CHECK(r.direction == BELLOWS_PULL);
  CHECK(r.intensity == 1.0f);
}

/* Intensity clamps to 1.0 past full travel (sensor out of range). */
static void test_intensity_clamps_past_full(void)
{
  bellow_classify_result_t r = classify(BELLOWS_PUSH, FULL_PUSH - 100);
  CHECK(r.direction == BELLOWS_PUSH);
  CHECK(r.intensity == 1.0f);
}

/* Intensity at the midpoint of push span is exactly 0.5. */
static void test_intensity_midpoint_push(void)
{
  /* midpoint = PUSH_EDGE - PUSH_SPAN/2 = 3730 - 165 = 3565 */
  bellow_classify_result_t r = classify(BELLOWS_PUSH, PUSH_EDGE - PUSH_SPAN/2);
  CHECK(r.direction == BELLOWS_PUSH);
  /* d = PUSH_SPAN/2 = 165; intensity = 165/330 = 0.5 exactly */
  CHECK(r.intensity == 0.5f);
}

/* From PUSH, jumping directly past the pull edge transitions to PULL. */
static void test_direct_push_to_pull(void)
{
  bellow_classify_result_t r = classify(BELLOWS_PUSH, PULL_EDGE + 1);
  CHECK(r.direction == BELLOWS_PULL);
}

/* Miscalibration: full_push == center (span = 0) -> intensity 0, no crash. */
static void test_miscalibration_zero_span(void)
{
  bellow_classify_result_t r = bellow_classify(BELLOWS_NEUTRAL,
                                               CENTER - 50, CENTER,
                                               DEAD, HYST,
                                               CENTER,   /* full_push == center -> span=0 */
                                               FULL_PULL);
  /* Direction may be PUSH (past push_edge), but intensity must be 0 (span=0). */
  CHECK(r.intensity == 0.0f);
}

/* Direction-only call (full_push=full_pull=0): intensity is always 0, no crash. */
static void test_direction_only_call(void)
{
  /* center=0, dead=64, hyst=32, full=0 */
  bellow_classify_result_t r = bellow_classify(BELLOWS_NEUTRAL,
                                               -50, 0, 64, 32, 0, 0);
  /* push_edge = 0 - 32 - 16 = -48; value=-50 < -48 -> PUSH */
  CHECK(r.direction == BELLOWS_PUSH);
  CHECK(r.intensity == 0.0f);  /* span = push_edge - 0 = -48 < 0 -> 0 */
}

/* Direction-only: NEUTRAL when |P| < push_edge threshold. */
static void test_direction_only_neutral(void)
{
  bellow_classify_result_t r = bellow_classify(BELLOWS_NEUTRAL,
                                               -10, 0, 64, 32, 0, 0);
  /* push_edge = -48; value=-10 > -48 -> NEUTRAL */
  CHECK(r.direction == BELLOWS_NEUTRAL);
  CHECK(r.intensity == 0.0f);
}

int main(void)
{
  test_neutral_at_rest();
  test_enter_push_from_neutral();
  test_enter_pull_from_neutral();
  test_hysteresis_holds_push();
  test_return_to_neutral_from_push();
  test_hysteresis_holds_pull();
  test_return_to_neutral_from_pull();
  test_intensity_max_at_full_push();
  test_intensity_max_at_full_pull();
  test_intensity_clamps_past_full();
  test_intensity_midpoint_push();
  test_direct_push_to_pull();
  test_miscalibration_zero_span();
  test_direction_only_call();
  test_direction_only_neutral();

  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
