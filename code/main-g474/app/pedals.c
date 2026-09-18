#include "pedals.h"

#include <stdio.h>

#include "hysteresis.h"
#include "keyboard.h"   /* L_MIDI_CH / R_MIDI_CH */
#include "main.h"
#include "one_euro_filter.h"
#include "properties.h"
#include "usb_app.h"

/* ADC handles for the two pedal wipers, defined by the CubeMX-generated main.c:
 * ADC1 reads pedal 1 (expression), ADC2 pedal 2 (sustain). */
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;

/* Each pedal's wiper is interpolated over its [min,max] property range to 0..127
 * and sent on this controller. Pedal 1 uses Modulation (CC#1) and pedal 2 the
 * Foot Controller (CC#4): both are pre-mapped in most instruments, so the
 * pedals do something out of the box, and either can still be MIDI-learned to a
 * different VST parameter in the DAW. */
#define PEDAL1_CC 1
#define PEDAL2_CC 4

/* Movement of the wiper sample (out of the 12-bit ADC range) that must be
 * exceeded before a new diagnostic line is logged, so wiper noise on a
 * connected pedal doesn't flood the console. */
static const uint32_t PEDAL_WIPER_HYST = 16;

/* Per-pedal state retained across polls: the 1-euro filter state that
 * smooths the wiper, the directional-hysteresis state that then cleans it
 * into a 0..127 CC, plus separate state for the diagnostic log line. */
typedef struct {
  one_euro_state_t oe;       /* pre-hysteresis adaptive low-pass state */
  hyst_state_t hyst;         /* filtered sample -> CC hysteresis/rate-limit state */
  uint8_t  connected_prev;   /* connected flag at the last logged line (0xFF = none yet) */
  uint32_t sample_prev;      /* wiper sample at the last logged line */
} pedal_state_t;

/* Latest raw wiper reading and presence flag, for pedals_get_raw() below.
 * Updated unconditionally every poll, unlike pedal_state_t's *_prev fields
 * above (which only move when a diagnostic line is actually logged). */
typedef struct {
  uint16_t sample;
  bool     connected;
} pedal_raw_t;

static pedal_raw_t g_pedal1_raw;
static pedal_raw_t g_pedal2_raw;

void pedals_get_raw(uint16_t *pedal1_sample, bool *pedal1_connected,
                    uint16_t *pedal2_sample, bool *pedal2_connected)
{
  *pedal1_sample = g_pedal1_raw.sample;
  *pedal1_connected = g_pedal1_raw.connected;
  *pedal2_sample = g_pedal2_raw.sample;
  *pedal2_connected = g_pedal2_raw.connected;
}

/* Polls one pedal: logs detect/movement changes, and while a pedal is connected
 * runs the wiper through a 1-euro adaptive low-pass (one_euro_filter.h) and then
 * directional hysteresis (see hysteresis.h), emitting its Effect Controller CC
 * whenever hyst_update() says the cleaned value is worth sending. The detect
 * line is pulled high in the no-pedal state (a normally-closed jack switch
 * biases it to VCC), so a connected pedal reads GPIO_PIN_RESET. */
static void pedal_poll_one(const char *name, GPIO_TypeDef *det_port, uint16_t det_pin,
                           ADC_HandleTypeDef *adc, uint8_t controller,
                           const one_euro_config_t *oe_cfg, const hyst_config_t *cfg,
                           pedal_state_t *st, pedal_raw_t *raw)
{
  uint8_t connected = HAL_GPIO_ReadPin(det_port, det_pin) == GPIO_PIN_RESET;
  uint32_t sample = HAL_ADC_GetValue(adc);

  raw->sample = (uint16_t)sample;
  raw->connected = connected;

  /* Always log a presence change; otherwise only when log_pedals is set and the
   * wiper has moved enough to be worth a line. */
  uint32_t delta = sample > st->sample_prev ? sample - st->sample_prev : st->sample_prev - sample;
  if (connected != st->connected_prev ||
      (g_properties->log_pedals && connected && delta > PEDAL_WIPER_HYST))
  {
    st->connected_prev = connected;
    st->sample_prev = sample;
    printf("%s connected=%u val=%u\r\n", name, connected, (unsigned)sample);
  }

  /* Only stream the controller while a pedal is plugged in; the detect line
   * floats otherwise. Resending starts fresh on reconnect (reseed the filter
   * and anchor from the first sample after plug-in). */
  if (!connected)
  {
    st->hyst.init = false;
    return;
  }

  uint32_t now_ms = HAL_GetTick();
  float xf = one_euro_update(&st->oe, oe_cfg, (float)sample, now_ms);
  uint32_t filtered = (uint32_t)(xf < 0.0f ? 0.0f : xf + 0.5f);

  uint16_t value;
  if (!hyst_update(&st->hyst, cfg, filtered, now_ms, &value)) return;
  /* Mirror the bellows expression CC: send on both keyboard channels so the
   * mapping works regardless of which channel the DAW listens on. */
  usb_app_midi_control_change(L_MIDI_CH, controller, value);
  usb_app_midi_control_change(R_MIDI_CH, controller, value);
}

void pedals_poll(void)
{
  static pedal_state_t pedal1 = { .connected_prev = 0xFF, .sample_prev = 0xFFFF };
  static pedal_state_t pedal2 = { .connected_prev = 0xFF, .sample_prev = 0xFFFF };

  /* Built per poll from g_properties so edits take effect live; hyst_update() and
   * one_euro_update() are inlined, so these structs scalarize away rather than
   * hitting the stack. */
  one_euro_config_t oe_cfg1 = {
    .mincutoff = g_properties->pedal1_1e_mincutoff / 256.0f,
    .beta = g_properties->pedal1_1e_beta / 65536.0f,
    .dcutoff = 1.0f,
  };
  hyst_config_t cfg1 = {
    .in_min = g_properties->pedal1_min, .in_max = g_properties->pedal1_max, .out_max = 127,
    .fwd_thresh = g_properties->pedal1_hyst_fwd, .rev_thresh = g_properties->pedal1_hyst_rev,
    .min_period_ms = g_properties->pedal1_cc_period_ms,
  };
  one_euro_config_t oe_cfg2 = {
    .mincutoff = g_properties->pedal2_1e_mincutoff / 256.0f,
    .beta = g_properties->pedal2_1e_beta / 65536.0f,
    .dcutoff = 1.0f,
  };
  hyst_config_t cfg2 = {
    .in_min = g_properties->pedal2_min, .in_max = g_properties->pedal2_max, .out_max = 127,
    .fwd_thresh = g_properties->pedal2_hyst_fwd, .rev_thresh = g_properties->pedal2_hyst_rev,
    .min_period_ms = g_properties->pedal2_cc_period_ms,
  };

  /* Start both conversions before reading either, so they run concurrently on
   * ADC1/ADC2 rather than back to back. pedal_poll_one() reads the results. */
  HAL_ADC_Start(&hadc1);
  HAL_ADC_Start(&hadc2);
  HAL_ADC_PollForConversion(&hadc1, 1);
  HAL_ADC_PollForConversion(&hadc2, 1);

  pedal_poll_one("PEDAL1", EXP_PEDAL_INT_GPIO_Port, EXP_PEDAL_INT_Pin, &hadc1,
                 PEDAL1_CC, &oe_cfg1, &cfg1, &pedal1, &g_pedal1_raw);
  pedal_poll_one("PEDAL2", SUS_PEDAL_INT_GPIO_Port, SUS_PEDAL_INT_Pin, &hadc2,
                 PEDAL2_CC, &oe_cfg2, &cfg2, &pedal2, &g_pedal2_raw);
}
