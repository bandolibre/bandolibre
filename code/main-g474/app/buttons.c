#include "buttons.h"

#include <stdio.h>

#include "bellow.h"
#include "keyboard_layout.h"
#include "main.h"
#include "properties.h"

/* How long FN1 must be held to calibrate the bellows center rather than
 * advance the bellows program. */
#define FN1_LONG_PRESS_MS 1000

/* The three LEDs are dimmed with hardware PWM, never switched: at full duty
 * they are far too bright. The led_max_duty property (counts out of
 * LED_PWM_TOP) is the brightness of a "100%" LED; every level below is a share
 * of it.
 *   LED_FN0  PB9  TIM4_CH4 (AF2)
 *   LED_FN1  PB7  TIM4_CH2 (AF2)
 *   LED_FN2  PB5  TIM3_CH2 (AF2)
 * HAL's timer module is not built and main.c is CubeMX-generated, so the timers
 * are set up here with CMSIS registers, after MX_GPIO_Init() made the pins
 * plain outputs. LED_PWM_TOP+1 counts per period is far above any flicker rate
 * at the bus clock. */
#define LED_PWM_TOP   1023U

static void led_pwm_init(void)
{
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_TIM4_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {0};
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_LOW;
  gpio.Pin       = LED_FN0_Pin | LED_FN1_Pin;
  gpio.Alternate = GPIO_AF2_TIM4;
  HAL_GPIO_Init(GPIOB, &gpio);
  gpio.Pin       = LED_FN2_Pin;
  gpio.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOB, &gpio);

  TIM4->PSC   = 0;
  TIM4->ARR   = LED_PWM_TOP;
  TIM4->CCR2  = 0;
  TIM4->CCR4  = 0;
  TIM4->CCMR1 = TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2PE;  /* PWM mode 1 */
  TIM4->CCMR2 = TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4PE;
  TIM4->CCER  = TIM_CCER_CC2E | TIM_CCER_CC4E;
  TIM4->CR1   = TIM_CR1_ARPE;
  TIM4->EGR   = TIM_EGR_UG;
  TIM4->CR1  |= TIM_CR1_CEN;

  TIM3->PSC   = 0;
  TIM3->ARR   = LED_PWM_TOP;
  TIM3->CCR2  = 0;
  TIM3->CCMR1 = TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2PE;
  TIM3->CCER  = TIM_CCER_CC2E;
  TIM3->CR1   = TIM_CR1_ARPE;
  TIM3->EGR   = TIM_EGR_UG;
  TIM3->CR1  |= TIM_CR1_CEN;
}

/* An intermediate state is not a steady glow (a half-duty LED looks nearly as
 * bright as a full one) but a slow pulse between these shares of led_max_duty,
 * eased at both ends so it breathes rather than ramps. */
#define LED_PULSE_MIN_PCT  3U
#define LED_PULSE_MAX_PCT  34U
#define LED_PULSE_MS       2000U

/* Smoothstep 3x^2 - 2x^3 on 0..1024. */
static uint32_t smoothstep(uint32_t x)
{
  return (x * x * (3U * 1024U - 2U * x)) >> 20;
}

/* Duty for `state` out of `states` ordered states: the first is off, the last
 * is led_max_duty and every state between pulses. With three states that is
 * off, pulsing, full. */
static uint32_t led_level(uint32_t state, uint32_t states, uint32_t now_ms)
{
  if (state >= states) state = states - 1;
  if (state == 0) return 0;
  if (state == states - 1) return g_properties->led_max_duty;

  uint32_t phase = now_ms % LED_PULSE_MS;
  uint32_t half  = LED_PULSE_MS / 2U;
  uint32_t tri   = (phase < half ? phase : LED_PULSE_MS - phase) * 1024U / half;
  uint32_t pct   = LED_PULSE_MIN_PCT +
                   (LED_PULSE_MAX_PCT - LED_PULSE_MIN_PCT) * smoothstep(tri) / 1024U;
  return g_properties->led_max_duty * pct / 100U;
}

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
  /* Each LED shows its property: FN0 table_mode off/on (0%, 100%), FN1 the
   * bellows program and FN2 the keyboard tuning (off, pulsing, 100% for their
   * three values). FN1 is at 100% while calibrating, whatever the program. */
  static bool led_pwm_ready;
  if (!led_pwm_ready)
  {
    led_pwm_init();
    led_pwm_ready = true;
  }
  uint32_t now = HAL_GetTick();
  TIM4->CCR4 = led_level(g_properties->table_mode ? 1 : 0, 2, now);
  TIM4->CCR2 = bellow_calibrating() ? g_properties->led_max_duty
                                    : led_level(g_properties->bellow_program, BELLOW_PROGRAM_COUNT, now);
  TIM3->CCR2 = led_level(g_properties->keyboard_tuning, NUM_TUNINGS, now);

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
