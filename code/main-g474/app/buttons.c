#include "buttons.h"

#include <stdio.h>

#include "keyboard_layout.h"
#include "main.h"
#include "properties.h"

/* Number of bellows sensitivity levels FN1 cycles through. */
#define BELLOW_SENS_LEVELS 3

bool buttons_table_mode(void)
{
  return g_properties->table_mode;
}

uint8_t buttons_bellow_sens_level(void)
{
  return (uint8_t)g_properties->bellow_sens_level;
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

/* Advances bellow_sens_level to the next level, wrapping after the last. Same
 * reasoning as cycle_keyboard_tuning() above: the property is the single
 * source of truth, so the button and `set bellow_sens_level` over the
 * console/sysex agree and neither can go stale behind the other. */
static void cycle_bellow_sens_level(void)
{
  size_t idx;
  uint16_t level;
  if (!property_by_name("bellow_sens_level", &idx)) return;
  if (!property_get_u16(idx, &level)) return;
  property_set_u16(idx, (uint16_t)((level + 1) % BELLOW_SENS_LEVELS));
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

  /* Act on rising edges (press, not release). FN0 toggles table mode; FN1
   * advances the bellows sensitivity level, wrapping after the last; FN2
   * advances the keyboard tuning, wrapping after the last. */
  if ((fn & 1) && !(fn_prev & 1)) toggle_table_mode();
  if ((fn & 2) && !(fn_prev & 2)) cycle_bellow_sens_level();
  if ((fn & 4) && !(fn_prev & 4)) cycle_keyboard_tuning();

  if (fn != fn_prev)
  {
    fn_prev = fn;
    const char *tname = tuning_name((uint8_t)g_properties->keyboard_tuning);
    printf("FN: %c%c%c  table_mode=%u  bellow_sens_level=%u  keyboard_tuning=%s\r\n",
           (fn & 1) ? '1' : '0',
           (fn & 2) ? '1' : '0',
           (fn & 4) ? '1' : '0',
           g_properties->table_mode, g_properties->bellow_sens_level, tname ? tname : "?");
  }
}
