#include "buttons.h"

#include <stdio.h>

#include "bellow.h"
#include "keyboard_layout.h"
#include "main.h"
#include "properties.h"

/* How long FN1 must be held to calibrate the bellows center rather than
 * advance the bellows program. */
#define FN1_LONG_PRESS_MS 1000

bool buttons_table_mode(void)
{
  return g_properties->table_mode;
}

/* Advances keyboard_tuning to the next tuning, wrapping after the last. The
 * property stays the single source of truth — this keeps no copy of its own —
 * so the button and `set keyboard_tuning` on the console agree and neither can
 * go stale behind the other. */
static void cycle_keyboard_tuning(void)
{
  size_t idx;
  uint16_t tuning;
  if (!property_by_name("keyboard_tuning", &idx)) return;
  if (!property_get_u16(idx, &tuning)) return;
  property_set_u16(idx, (uint16_t)((tuning + 1) % NUM_TUNINGS));
}

/* Flips table_mode. Same reasoning as cycle_keyboard_tuning() above: the
 * property is the single source of truth, so the button and `set table_mode`
 * over the console/sysex agree and neither can go stale behind the other. */
static void toggle_table_mode(void)
{
  size_t idx;
  bool value;
  if (!property_by_name("table_mode", &idx)) return;
  if (!property_get_bool(idx, &value)) return;
  property_set_bool(idx, !value);
}

/* Advances bellow_program to the next program, wrapping after the last. Same
 * reasoning as cycle_keyboard_tuning() above: the property is the single
 * source of truth, so the button and `set bellow_program` over the
 * console/sysex agree and neither can go stale behind the other. */
static void cycle_bellow_program(void)
{
  size_t idx;
  uint16_t program;
  if (!property_by_name("bellow_program", &idx)) return;
  if (!property_get_u16(idx, &program)) return;
  property_set_u16(idx, (uint16_t)((program + 1) % BELLOW_PROGRAM_COUNT));
}

void buttons_poll(void)
{
  /* 0xFF forces a log on the first poll, whatever the buttons read. Its FN0 bit
   * is set, so a button already held at boot is not seen as a fresh press. */
  static uint8_t fn_prev = 0xFF;

  uint8_t fn = 0;
  /* SW_FN0 shares BOOT0, which carries the boot-mode pulldown, so it reads the
   * opposite way from the other two: low while idle, high (SET) when pressed.
   * Match against SET so a press sets the bit like the active-low FN1/FN2. */
  if (HAL_GPIO_ReadPin(SW_FN0_BOOT0_GPIO_Port, SW_FN0_BOOT0_Pin) == GPIO_PIN_SET) fn |= 1;
  if (HAL_GPIO_ReadPin(SW_FN1_GPIO_Port,       SW_FN1_Pin)        == GPIO_PIN_RESET) fn |= 2;
  if (HAL_GPIO_ReadPin(SW_FN2_GPIO_Port,       SW_FN2_Pin)        == GPIO_PIN_RESET) fn |= 4;

  /* FN0 and FN2 act on rising edges (press, not release): FN0 toggles table
   * mode, FN2 advances the keyboard tuning, wrapping after the last. */
  if ((fn & 1) && !(fn_prev & 1)) toggle_table_mode();
  if ((fn & 4) && !(fn_prev & 4)) cycle_keyboard_tuning();

  /* FN1 has a long press too, so a short press can only be told apart on
   * release: released before FN1_LONG_PRESS_MS it advances the bellows
   * program; held that long it calibrates the bellows center instead, and its
   * release then does nothing. Armed only by a press seen here, so neither the
   * 0xFF above nor a button held at boot counts as a press. */
  static bool fn1_armed;
  static uint32_t fn1_down_ms;
  if ((fn & 2) && !(fn_prev & 2))
  {
    fn1_armed = true;
    fn1_down_ms = HAL_GetTick();
  }
  if (fn1_armed && (fn & 2) && HAL_GetTick() - fn1_down_ms >= FN1_LONG_PRESS_MS)
  {
    fn1_armed = false;
    bellow_calibrate_center();
  }
  if (fn1_armed && !(fn & 2))
  {
    fn1_armed = false;
    cycle_bellow_program();
  }
  HAL_GPIO_WritePin(LED_FN1_GPIO_Port, LED_FN1_Pin, bellow_calibrating() ? GPIO_PIN_SET : GPIO_PIN_RESET);

  if (fn != fn_prev)
  {
    fn_prev = fn;
    const char *tname = tuning_name((uint8_t)g_properties->keyboard_tuning);
    printf("FN: %c%c%c  table_mode=%u  bellow_program=%u  keyboard_tuning=%s\r\n",
           (fn & 1) ? '1' : '0',
           (fn & 2) ? '1' : '0',
           (fn & 4) ? '1' : '0',
           g_properties->table_mode, g_properties->bellow_program, tname ? tname : "?");
  }
}
