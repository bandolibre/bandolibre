#include "bellow.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "bellow_classify.h"
#include "bellow_curve.h"
#include "buttons.h"
#include "console.h"
#include "swo.h"
#include "hysteresis.h"
#include "keyboard.h"   /* L_MIDI_CH / R_MIDI_CH */
#include "main.h"
#include "properties.h"
#include "report.h"
#include "usb_app.h"


/* ADC handles for the two hall sensors, defined by the CubeMX-generated main.c. */
extern ADC_HandleTypeDef hadc3;
extern ADC_HandleTypeDef hadc4;

/* Naive bellows model state. */
typedef struct {
  bellows_t direction;
  uint16_t intensity;
} bellow_naive_state_t;

typedef struct {
  bellows_t direction;
  uint16_t  intensity;
} bellow_output_t;

static bellow_output_t g_bellow_out = {.direction = BELLOWS_NEUTRAL, .intensity = 0};

/* Latest raw hall readings, for bellow_get_raw() below. */
static uint16_t g_bellow_last_hall0;
static uint16_t g_bellow_last_hall1;

/* Latest values from the bellow_poll() pipeline, captured there for
 * bellow_report() to display; not otherwise consumed. */
static float    g_bellow_1e_out;   /* bellow_filter_total() output, raw hall-total units */
static uint16_t g_bellow_cc_out;   /* CC value after backlash+scale, 0..16383 */

/* Passthrough 1-euro config: filtering happens upstream of classification, in
 * bellow_filter_total() below, so bellow_send_cc()'s own hyst_update() must not
 * filter a second time. A very large mincutoff with beta=0 is the same
 * filter-neutral idiom hysteresis_test.c uses to isolate backlash/scale/
 * rate-limit behavior from the 1-euro stage. */
static const one_euro_config_t k_oe_passthrough = { .mincutoff = 1e8f, .beta = 0.0f, .dcutoff = 1.0f };

/* Combined-hall calibration: center is the at-rest reading, hard push/pull
 * the readings at full travel. The deadzone sets how far from center the
 * bellows must move to leave BELLOWS_NEUTRAL (no air moves there, so
 * note_table maps every key to NOTE_NONE). The hysteresis margin then has to
 * be given back before returning to NEUTRAL, so a bellows resting right at
 * the deadzone edge doesn't chatter between NEUTRAL and PUSH/PULL. These are
 * the bellow_* properties in g_properties (properties.h). */

bellows_t bellow_direction(void)
{
  return g_bellow_out.direction;
}

uint16_t bellow_intensity(void)
{
  return g_bellow_out.intensity;
}

void bellow_get_raw(uint16_t *hall0, uint16_t *hall1)
{
  *hall0 = g_bellow_last_hall0;
  *hall1 = g_bellow_last_hall1;
}

/* Bellows sensitivity multiplier (Q8, 256 = x1.0) for the level FN1 currently
 * selects: level 0 is unity, levels 1 and 2 use the bellow_scale_mid/high
 * properties. Applied to the intensity, so it scales both note velocity and
 * CC#11. Table mode takes neither from the bellows and so is unaffected by it. */
uint16_t bellow_sens_scale_q8(void)
{
  switch (buttons_bellow_sens_level())
  {
    case 0:  return g_properties->bellow_scale_low;
    case 1:  return g_properties->bellow_scale_mid;
    case 2:  return g_properties->bellow_scale_high;
    default: return 256;
  }
}

/* Naive bellows model: classifies hall reading into direction and intensity.
 * hall_total is the filtered combined reading from bellow_filter_total(), not
 * the raw sample -- classification runs on a smoothed signal so sensor jitter
 * near the deadzone boundary doesn't flicker the direction. */
static void bellow_naive(uint32_t hall_total, bellow_naive_state_t *state)
{
  uint32_t center = g_properties->bellow_center;

  bellow_classify_result_t r = bellow_classify(state->direction, (int32_t)hall_total, (int32_t)center,
                                               g_properties->bellow_dead, g_properties->bellow_hyst,
                                               g_properties->bellow_full_push, g_properties->bellow_full_pull);
  state->direction = r.direction;
  if (r.direction == BELLOWS_PUSH)
    state->intensity = bellow_curve_apply(r.intensity,
        g_properties->bellow_push_curve_x1, g_properties->bellow_push_curve_y1,
        g_properties->bellow_push_curve_x2, g_properties->bellow_push_curve_y2);
  else
    state->intensity = bellow_curve_apply(r.intensity,
        g_properties->bellow_pull_curve_x1, g_properties->bellow_pull_curve_y1,
        g_properties->bellow_pull_curve_x2, g_properties->bellow_pull_curve_y2);
}

static void bellow_swo_trace(const bellow_naive_state_t *naive,
                             int32_t hall_total_centred, uint16_t keys)
{
  static uint32_t last_tick;
  static uint32_t last_header_sent;
  uint32_t now = HAL_GetTick();
  if (now - last_tick < 10) return;   /* 100 Hz */
  last_tick = now;
  if (last_header_sent == 0)
  {
    swo_print("timestamp,"
              "naive.direction,naive.intensity,"
              "hall_total_centred,keys\n");
    last_header_sent = 100;
  }
  last_header_sent--;
  swo_printf("%lu,"
             "%d,%u,"
             "%ld,%u\n",
             (unsigned long)now,
             (int)naive->direction, (unsigned)naive->intensity,
             (long)hall_total_centred, (unsigned)keys);
}

/* Emits CC#11 (Expression, paired with CC#43 as the 14-bit LSB) from the
 * effective intensity through the shared directional-hysteresis + rate-limit
 * pipeline (hysteresis.h, as the pedals use) -- noise suppression itself
 * already happened upstream, in bellow_filter_total() filtering the raw hall
 * reading before classification, so this stage's own 1-euro sub-stage is
 * neutralized (k_oe_passthrough) to avoid filtering the same signal twice.
 * bellow_cchyst is a small residual backlash on top (0 disables it) and
 * bellow_cc_period_ms caps the send rate. The rate limit coalesces rather than
 * drops, so the latest value is always eventually sent. Fed by
 * bellow_intensity().
 *
 * When intensity drops to 0 (bellow at rest), CC=0 is forced and the hysteresis
 * state is reset so backlash restarts cleanly on the next gesture rather than
 * smoothing across the rest gap.
 *
 * Table mode is the exception: the bellows rests there, so it must not drive
 * expression at all (see the branch below). */
static void bellow_send_cc(void)
{
  static hyst_state_t st;
  static bool table_prev;

  /* Table mode plays with the bellows at rest, so the bellows drives nothing
   * here: its intensity is 0 and the CC=0 that follows would silence every note.
   * Expression is constant instead, at the same tablemode_velocity the notes play
   * at. Send it once on entry, then stay quiet while table mode is engaged. */
  if (buttons_table_mode())
  {
    if (!table_prev)
    {
      table_prev = true;
      uint16_t cc14 = (uint16_t)g_properties->tablemode_velocity << 7;
      usb_app_midi_control_change_14bit(L_MIDI_CH, 11, cc14);
      usb_app_midi_control_change_14bit(R_MIDI_CH, 11, cc14);
    }
    g_bellow_cc_out = (uint16_t)g_properties->tablemode_velocity << 7;
    return;
  }
  table_prev = false;

  uint16_t intensity = bellow_intensity();
  if (intensity == 0)
  {
    if (st.have_out && st.last_out != 0)
    {
      usb_app_midi_control_change_14bit(L_MIDI_CH, 11, 0);
      usb_app_midi_control_change_14bit(R_MIDI_CH, 11, 0);
    }
    st = (hyst_state_t){0};
    g_bellow_cc_out = 0;
    return;
  }

  hyst_config_t cfg = {
    .in_min = 0, .in_max = BELLOW_INTENSITY_MAX, .out_max = 16383,
    .fwd_thresh = g_properties->bellow_cchyst, .rev_thresh = g_properties->bellow_cchyst,
    .oe = k_oe_passthrough,
    .min_period_ms = g_properties->bellow_cc_period_ms,
  };

  uint16_t value;
  bool emit = hyst_update(&st, &cfg, intensity, HAL_GetTick(), &value);
  /* hyst_update writes *out on every call regardless of whether it also says
   * to emit -- so the report always has the latest scaled value even on a
   * rate-limited or unchanged tick. */
  g_bellow_cc_out = value;
  if (!emit) return;

  /* The single bellows drives both keyboards, which play on separate MIDI
   * channels (L_MIDI_CH / R_MIDI_CH), so send the expression CC on both. */
  usb_app_midi_control_change_14bit(L_MIDI_CH, 11, value);
  usb_app_midi_control_change_14bit(R_MIDI_CH, 11, value);
}

static void delay_us(uint32_t us)
{
  uint32_t cycles = us * (SystemCoreClock / 1000000U);
  uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < cycles);
}

/* Caps the hall-sensor sampling rate at bellow_sample_period_us (default 4kHz):
 * true once that many microseconds have elapsed since the last due sample, so
 * bellow_poll() can skip the ADC work (and everything downstream of it) the
 * rest of the time. HAL_GetTick() is only millisecond-resolution, too coarse
 * for a sub-millisecond gate, so this uses the same DWT cycle counter
 * delay_us() above already relies on. */
static bool bellow_sample_due(void)
{
  static uint32_t last_cycles;
  static bool have_last;

  uint32_t now = DWT->CYCCNT;
  uint32_t period_cycles = g_properties->bellow_sample_period_us * (SystemCoreClock / 1000000U);
  if (have_last && (now - last_cycles) < period_cycles) return false;

  last_cycles = now;
  have_last = true;
  return true;
}

/* Filters the raw combined hall reading before it reaches classification, so
 * direction/intensity -- and everything downstream of them, including CC#11 --
 * are derived from a smoothed signal instead of raw sensor jitter (previously
 * the filter sat after classification, in bellow_send_cc(), which smoothed the
 * already-processed intensity instead of the noise at its source). Reuses the
 * bellow_cc_1e_mincutoff/beta tuning knobs from that earlier stage. Also
 * records the filtered value in g_bellow_1e_out for bellow_report(). */
static uint32_t bellow_filter_total(uint32_t hall_total)
{
  static one_euro_state_t st;
  one_euro_config_t cfg = {
    .mincutoff = g_properties->bellow_cc_1e_mincutoff / 256.0f,
    .beta = g_properties->bellow_cc_1e_beta / 65536.0f,
    .dcutoff = 1.0f,
  };
  float xf = one_euro_update(&st, &cfg, (float)hall_total, HAL_GetTick());
  g_bellow_1e_out = xf;
  return (uint32_t)(xf < 0.0f ? 0.0f : xf + 0.5f);
}

/* One acquisition of both hall sensors and their combined reading. */
typedef struct {
  uint16_t hall0;
  uint16_t hall1;
  uint32_t conv_cycles;  /* CPU cycles spent in the two ADC conversions */
} bellow_sample_t;

/* Reads both hall sensors and returns their values and combined total. */
static bellow_sample_t bellow_sample(void)
{
  /* The two hall sensors share a gate-switched supply (HALL_NEN). Enable it,
   * let the gate and sensor settle, sample both, then disable to save power. */
  HAL_GPIO_WritePin(HALL_NEN_GPIO_Port, HALL_NEN_Pin, GPIO_PIN_RESET);
  /* Settling budget: gate RC (R5||R6 * Ciss_Q1 = 909R * 130pF, 5t~600ns) +
   * VDDH caps (RDS_on_Q1 * (CP1+CP2) = ~120mO * 200nF, 5t~120ns) +
   * SC4015SO power-on start <1us (datasheet) => worst case <3us; the default
   * bellow_settle_us of 5us is ~1.7x margin. */
  delay_us(g_properties->bellow_settle_us);

  uint32_t conv_start = DWT->CYCCNT;
  HAL_ADC_Start(&hadc3);
  HAL_ADC_Start(&hadc4);
  HAL_ADC_PollForConversion(&hadc3, 10);
  uint32_t hall0 = HAL_ADC_GetValue(&hadc3);
  HAL_ADC_PollForConversion(&hadc4, 10);
  uint32_t hall1 = HAL_ADC_GetValue(&hadc4);
  uint32_t conv_cycles = DWT->CYCCNT - conv_start;

  HAL_GPIO_WritePin(HALL_NEN_GPIO_Port, HALL_NEN_Pin, GPIO_PIN_SET);

  bellow_sample_t s;
  s.hall0 = (uint16_t)hall0;
  s.hall1 = (uint16_t)hall1;
  s.conv_cycles = conv_cycles;
  return s;
}

/* Human-readable name for a bellows direction. */
static const char *bellows_dir_str(bellows_t dir)
{
  switch (dir)
  {
    case BELLOWS_PULL:    return "PULL";
    case BELLOWS_PUSH:    return "PUSH";
    case BELLOWS_NEUTRAL: return "NEUTRAL";
    default:              return "?";
  }
}

/* Emits the optional console log and live-report dashboard. Called every
 * bellow_poll() tick regardless of whether this tick actually sampled the hall
 * sensors, so the report dashboard's own frame (report_begin()/report_end(),
 * driven by its independent report_hz timer) always gets its rows even on a
 * tick where the two rates don't line up; a skipped sampling tick just adds
 * nothing new to accumulate. */
static void bellow_report(bool sampled, const bellow_sample_t *s, const bellow_naive_state_t *naive_state)
{

  /* Per-frame stats, accumulated every sample and reset after each emitted report
   * frame (and on (re-)enable so the first frame never shows stale data), so each
   * line reflects only the samples since the previous one. Extremes are of the
   * combined reading; the std (E[x^2]-E[x]^2) is per sensor, accumulated in
   * integers to keep the sums exact, with the count shown alongside. */
  static uint16_t hall_min, hall_max;
  static uint32_t n, sum0, sum1, sumT;
  static uint64_t sq0, sq1, sqT;
  static double sum1e, sq1e;   /* g_bellow_1e_out is a float, so accumulated in double */
  static uint32_t push_entries, pull_entries;
  static bool show_was_on;
  if (g_properties->show_bellow && !show_was_on)
  {
    hall_min = 0xFFFF; hall_max = 0;
    n = 0; sum0 = sum1 = sumT = 0; sq0 = sq1 = sqT = 0;
    sum1e = sq1e = 0.0;
    push_entries = pull_entries = 0;
  }
  show_was_on = g_properties->show_bellow;
  if (sampled)
  {
    uint16_t total = s->hall0 + s->hall1;
    if (total < hall_min) hall_min = total;
    if (total > hall_max) hall_max = total;
    n++;
    sum0 += s->hall0; sq0 += (uint32_t)s->hall0 * s->hall0;
    sum1 += s->hall1; sq1 += (uint32_t)s->hall1 * s->hall1;
    sumT += total;     sqT += (uint32_t)total * total;
    sum1e += (double)g_bellow_1e_out; sq1e += (double)g_bellow_1e_out * g_bellow_1e_out;

    /* Zone-entry counters: how many times the naive direction transitioned
     * into PUSH/PULL since the last report frame (tracked independently of
     * show_bellow so a mid-window enable doesn't miscount the first sample
     * as an entry). */
    static bellows_t last_dir = BELLOWS_NEUTRAL;
    if (naive_state->direction != last_dir)
    {
      if (naive_state->direction == BELLOWS_PUSH) push_entries++;
      else if (naive_state->direction == BELLOWS_PULL) pull_entries++;
      last_dir = naive_state->direction;
    }
  }

  if (g_report_due && g_properties->show_bellow && n > 0)
  {
    /* Double precision for the E[x^2]-E[x]^2 subtraction: hall readings run in
     * the thousands, so their squares are large relative to a small variance,
     * and float32's ~7 significant digits aren't enough to keep the difference
     * from occasionally going slightly negative (sqrtf of that is NaN). The
     * result is clamped to 0 as a floor against any residual rounding. */
    double mean0 = (double)sum0 / n;
    double mean1 = (double)sum1 / n;
    double meanT = (double)sumT / n;
    double var0 = (double)sq0 / n - mean0 * mean0;
    double var1 = (double)sq1 / n - mean1 * mean1;
    double varT = (double)sqT / n - meanT * meanT;
    double mean1e = sum1e / n;
    double var1e = sq1e / n - mean1e * mean1e;
    float std0 = sqrtf((float)(var0 > 0.0 ? var0 : 0.0));
    float std1 = sqrtf((float)(var1 > 0.0 ? var1 : 0.0));
    float stdT = sqrtf((float)(varT > 0.0 ? varT : 0.0));
    float std1e = sqrtf((float)(var1e > 0.0 ? var1e : 0.0));
    float conv_us = (float)s->conv_cycles / (SystemCoreClock / 1000000.0f);
    float force = (naive_state->direction == BELLOWS_PUSH) ? -(float)naive_state->intensity
                : (naive_state->direction == BELLOWS_PULL) ?  (float)naive_state->intensity : 0.0f;
    console_dash_println("BELLOW  dir=%-7s int=%4u  force=%+5d keys=%u",
                         bellows_dir_str(naive_state->direction), naive_state->intensity,
                         (int)force, keyboard_keys_pressed());
    /* std0/std1/stdT line's "std0 =" / "std1 =" / "stdT =" labels are each
     * padded to the same width as "hall0=" / "hall1=" / "total=" above (and
     * the value fields use matching widths), so the two lines' columns
     * line up in the monospace console. */
    console_dash_println("        hall0=%5u hall1=%5u total=%5u  (min %u max %u)  sample_count=%lu  conv=%.1fus",
                         (unsigned)s->hall0, (unsigned)s->hall1, (unsigned)s->hall0 + s->hall1,
                         hall_min, hall_max, (unsigned long)n, (double)conv_us);
    console_dash_println("        std0 =%5.1f std1 =%5.1f stdT =%5.1f",
                         (double)std0, (double)std1, (double)stdT);
    console_dash_println("        1euro=%8.1f std=%5.1f  (mincutoff=%.2fHz beta=%.4f)",
                         (double)g_bellow_1e_out, (double)std1e,
                         (double)g_properties->bellow_cc_1e_mincutoff / 256.0,
                         (double)g_properties->bellow_cc_1e_beta / 65536.0);
    /* cc11's second form is the 14-bit value as MSB.LSB (CC#11.CC#43, each
     * 7-bit) rendered as one decimal -- value/128 puts the MSB in the integer
     * part and the LSB as a fraction of it, the same conversion midi.html's
     * live CC#11 readout uses (updateLiveDot() there). */
    console_dash_println("        zone entries: push=%6lu pull=%6lu   cc11=%5u (%6.2f)",
                         (unsigned long)push_entries, (unsigned long)pull_entries,
                         (unsigned)g_bellow_cc_out, (double)g_bellow_cc_out / 128.0);

    hall_min = 0xFFFF; hall_max = 0;
    n = 0; sum0 = sum1 = sumT = 0; sq0 = sq1 = sqT = 0;
    sum1e = sq1e = 0.0;
    push_entries = pull_entries = 0;
  }
}

/* Samples both hall sensors, filters the combined reading (bellow_filter_total),
 * updates the bellows direction/intensity from the filtered value, emits the
 * expression CC, and reports. Call once per main loop iteration; sampling is
 * internally rate-limited to bellow_sample_period_us, so most calls skip the
 * ADC work and the direction/intensity/CC update (the previous values carry
 * over). bellow_report() still runs every call regardless: it has its own
 * report_hz cadence (g_report_due, set by report_begin() in report.c) that
 * is independent of the sample rate, and it must run on every tick so it
 * doesn't miss a report_due tick that happens to fall between samples -
 * which would leave that dashboard frame without its bellow rows. */
void bellow_poll(void)
{
  static bellow_naive_state_t naive = {.direction = BELLOWS_NEUTRAL, .intensity = 0};
  static bellow_sample_t s;

  bool sampled = bellow_sample_due();
  if (sampled)
  {
    s = bellow_sample();
    g_bellow_last_hall0 = s.hall0;
    g_bellow_last_hall1 = s.hall1;
    uint32_t hall_total = bellow_filter_total(s.hall0 + s.hall1);
    bellow_naive(hall_total, &naive);

    g_bellow_out.direction = naive.direction;
    g_bellow_out.intensity = naive.intensity;
    uint32_t scaled = ((uint32_t)g_bellow_out.intensity * bellow_sens_scale_q8()) >> 8;
    g_bellow_out.intensity = (uint16_t)(scaled > BELLOW_INTENSITY_MAX ? BELLOW_INTENSITY_MAX : scaled);

    int32_t hall_total_centred = (int32_t)hall_total - (int32_t)g_properties->bellow_center;
    bellow_swo_trace(&naive, hall_total_centred, keyboard_keys_pressed());
    bellow_send_cc();
  }
  bellow_report(sampled, &s, &naive);
}

/* Diagnostic sweep: for each bellow_settle_us value in a fixed range, take
 * BELLOW_TUNE_SAMPLES samples (each spaced BELLOW_TUNE_PAUSE_MS apart) and print
 * the per-sensor mean and standard deviation. Helps pick the smallest settling
 * delay that still yields stable hall readings. Blocks the main loop for the
 * duration; temporarily overrides bellow_settle_us and restores it on return. */
void bellow_tune(void)
{
  enum {
    BELLOW_TUNE_LO        = 0,
    BELLOW_TUNE_HI        = 20,
    BELLOW_TUNE_STEP      = 2,
    BELLOW_TUNE_SAMPLES   = 100,
    BELLOW_TUNE_PAUSE_MS  = 20,
  };

  size_t idx;
  if (!property_by_name("bellow_settle_us", &idx))
  {
    printf("bellow_settle_us property not found\r\n");
    return;
  }
  uint16_t saved;
  property_get_u16(idx, &saved);

  /* stdout is block-buffered and the main loop (which would flush it) is stalled
   * for the whole sweep, so flush each row explicitly or nothing appears until
   * the command returns. */
  printf("settle_us   h1_avg  h1_std   h2_avg  h2_std\r\n");
  fflush(stdout);
  for (uint16_t us = BELLOW_TUNE_LO; us <= BELLOW_TUNE_HI; us += BELLOW_TUNE_STEP)
  {
    property_set_u16(idx, us);

    uint32_t sum0 = 0, sum1 = 0;
    uint64_t sq0 = 0, sq1 = 0;
    for (int i = 0; i < BELLOW_TUNE_SAMPLES; i++)
    {
      bellow_sample_t s = bellow_sample();
      sum0 += s.hall0; sq0 += (uint32_t)s.hall0 * s.hall0;
      sum1 += s.hall1; sq1 += (uint32_t)s.hall1 * s.hall1;
      /* This runs in the USART RX ISR (microrl execute callback), where SysTick
       * cannot preempt, so HAL_Delay would hang on a frozen tick. Busy-wait on
       * the DWT cycle counter instead. */
      delay_us(BELLOW_TUNE_PAUSE_MS * 1000U);
    }

    /* Mean and variance (E[x^2] - E[x]^2) in float; the FPU does the divides and
     * sqrt. Samples are accumulated in integers to keep the sums exact. */
    float mean0 = (float)sum0 / BELLOW_TUNE_SAMPLES;
    float mean1 = (float)sum1 / BELLOW_TUNE_SAMPLES;
    float std0 = sqrtf((float)sq0 / BELLOW_TUNE_SAMPLES - mean0 * mean0);
    float std1 = sqrtf((float)sq1 / BELLOW_TUNE_SAMPLES - mean1 * mean1);

    printf("%9u   %6.1f  %6.2f   %6.1f  %6.2f\r\n",
           us, (double)mean0, (double)std0, (double)mean1, (double)std1);
    fflush(stdout);
  }

  property_set_u16(idx, saved);
}
