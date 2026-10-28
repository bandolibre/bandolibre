#include "system_dfu.h"

#include <stdbool.h>
#include <stdint.h>

#include "stm32g4xx.h"

/* The ROM loader: its vector table (initial SP, then reset handler) is at the
 * start of system memory. */
#define SYSTEM_MEMORY_BASE 0x1FFF0000UL

/* The keyboards' lines on this board (main-g474.ioc): NRST open-drain through
 * a BAT54C, BOOT0 push-pull. */
#define WING_NRST_PINS   ((1UL << 2) | (1UL << 11))   /* PB2 left, PB11 right */
#define L_BOOT0_PIN      (1UL << 3)                   /* PA3 */
#define R_BOOT0_PIN      (1UL << 13)                  /* PB13 */

/* Busy-waits without a timer (interrupts are off): `n` loops are about
 * n / 16 µs at 170 MHz, n / 2 µs at 16 MHz. */
static void spin(uint32_t n)
{
  for (volatile uint32_t i = 0; i < n; i++) { }
}

static void gpio_output(GPIO_TypeDef *port, uint32_t pins, bool open_drain)
{
  for (uint32_t pin = 0; pin < 16U; pin++) {
    if (!(pins & (1UL << pin))) continue;
    if (open_drain) port->OTYPER |= 1UL << pin;
    else port->OTYPER &= ~(1UL << pin);
    port->MODER = (port->MODER & ~(3UL << (2 * pin))) | (1UL << (2 * pin));
  }
}

void system_dfu_enter(void)
{
  __disable_irq();
  SysTick->CTRL = 0;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
  for (uint32_t i = 0; i < 8U; i++) {
    NVIC->ICER[i] = 0xFFFFFFFFUL;
    NVIC->ICPR[i] = 0xFFFFFFFFUL;
  }

  /* Both keyboards restarted into their own ROM loaders: BOOT0 high during an
   * NRST pulse. There they are SPI slaves and never drive the clock. In their
   * firmware the left one is SPI master on PA4-PA7, which is also this ROM
   * loader's SPI1 port: it would take a frame for its sync byte, pick SPI and
   * switch USB off. Holding them in reset would not last: the ROM loader
   * reconfigures port B, which releases NRST. They stay in their loaders,
   * whatever happens to these lines, until the application restarts them
   * (keyboard_init()). */
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
  (void) RCC->AHB2ENR;
  GPIOA->BSRR = L_BOOT0_PIN;
  GPIOB->BSRR = R_BOOT0_PIN;
  gpio_output(GPIOA, L_BOOT0_PIN, false);
  gpio_output(GPIOB, R_BOOT0_PIN, false);
  GPIOB->BRR = WING_NRST_PINS;
  gpio_output(GPIOB, WING_NRST_PINS, true);
  spin(16000);                 /* about 1 ms in reset */
  GPIOB->BSRR = WING_NRST_PINS;   /* released: they sample BOOT0, still high */

  /* Off USB, so the host sees this device leave before the loader arrives. */
  USB->BCDR &= (uint16_t) ~USB_BCDR_DPPU;

  /* Back to the 16 MHz HSI with the PLL off, as after a reset. */
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) { }
  RCC->CFGR = RCC_CFGR_SW_HSI;
  RCC->CR &= ~RCC_CR_PLLON;
  while (RCC->CR & RCC_CR_PLLRDY) { }

  spin(100000);   /* about 50 ms: the host notices the disconnect, the keyboards start */

  /* Every peripheral back to its reset state, except the flash interface,
   * which is running this code. */
  RCC->AHB1RSTR = RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST | RCC_AHB1RSTR_DMAMUX1RST |
                  RCC_AHB1RSTR_CORDICRST | RCC_AHB1RSTR_FMACRST | RCC_AHB1RSTR_CRCRST;
  RCC->AHB2RSTR = RCC_AHB2RSTR_GPIOARST | RCC_AHB2RSTR_GPIOBRST | RCC_AHB2RSTR_GPIOCRST | RCC_AHB2RSTR_GPIODRST |
                  RCC_AHB2RSTR_GPIOERST | RCC_AHB2RSTR_GPIOFRST | RCC_AHB2RSTR_GPIOGRST |
                  RCC_AHB2RSTR_ADC12RST | RCC_AHB2RSTR_ADC345RST | RCC_AHB2RSTR_DAC1RST |
                  RCC_AHB2RSTR_DAC2RST | RCC_AHB2RSTR_DAC3RST | RCC_AHB2RSTR_DAC4RST |
                  RCC_AHB2RSTR_RNGRST;
  RCC->AHB3RSTR = RCC_AHB3RSTR_FMCRST | RCC_AHB3RSTR_QSPIRST;
  RCC->APB1RSTR1 = RCC_APB1RSTR1_TIM2RST | RCC_APB1RSTR1_TIM3RST | RCC_APB1RSTR1_TIM4RST |
                   RCC_APB1RSTR1_TIM5RST | RCC_APB1RSTR1_TIM6RST | RCC_APB1RSTR1_TIM7RST |
                   RCC_APB1RSTR1_CRSRST | RCC_APB1RSTR1_SPI2RST | RCC_APB1RSTR1_SPI3RST |
                   RCC_APB1RSTR1_USART2RST | RCC_APB1RSTR1_USART3RST | RCC_APB1RSTR1_UART4RST |
                   RCC_APB1RSTR1_UART5RST | RCC_APB1RSTR1_I2C1RST | RCC_APB1RSTR1_I2C2RST |
                   RCC_APB1RSTR1_USBRST | RCC_APB1RSTR1_FDCANRST | RCC_APB1RSTR1_PWRRST |
                   RCC_APB1RSTR1_I2C3RST | RCC_APB1RSTR1_LPTIM1RST;
  RCC->APB1RSTR2 = RCC_APB1RSTR2_LPUART1RST | RCC_APB1RSTR2_I2C4RST | RCC_APB1RSTR2_UCPD1RST;
  RCC->APB2RSTR = RCC_APB2RSTR_SYSCFGRST | RCC_APB2RSTR_TIM1RST | RCC_APB2RSTR_SPI1RST |
                  RCC_APB2RSTR_TIM8RST | RCC_APB2RSTR_USART1RST | RCC_APB2RSTR_SPI4RST |
                  RCC_APB2RSTR_TIM15RST | RCC_APB2RSTR_TIM16RST | RCC_APB2RSTR_TIM17RST |
                  RCC_APB2RSTR_TIM20RST | RCC_APB2RSTR_SAI1RST | RCC_APB2RSTR_HRTIM1RST;
  RCC->AHB1RSTR = 0;
  RCC->AHB2RSTR = 0;
  RCC->AHB3RSTR = 0;
  RCC->APB1RSTR1 = 0;
  RCC->APB1RSTR2 = 0;
  RCC->APB2RSTR = 0;

  /* Peripheral clocks as after a reset (RM0440), plus SYSCFG for the remap. */
  RCC->AHB1ENR = RCC_AHB1ENR_FLASHEN;
  RCC->AHB2ENR = 0;
  RCC->AHB3ENR = 0;
  RCC->APB1ENR1 = RCC_APB1ENR1_RTCAPBEN;
  RCC->APB1ENR2 = 0;
  RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
  (void) RCC->APB2ENR;

  /* System memory at address 0, as BOOT0 selects it, then the loader's own
   * stack and reset handler. */
  SYSCFG->MEMRMP = SYSCFG_MEMRMP_MEM_MODE_0;
  uint32_t const sp = ((uint32_t const *) SYSTEM_MEMORY_BASE)[0];
  uint32_t const pc = ((uint32_t const *) SYSTEM_MEMORY_BASE)[1];
  SCB->VTOR = SYSTEM_MEMORY_BASE;
  __set_CONTROL(0);
  __DSB();
  __ISB();
  __enable_irq();
  /* One statement: no stack use between the new stack and the branch. */
  __asm volatile ("msr msp, %0\n\tbx %1" : : "r" (sp), "r" (pc) : "memory");
  __builtin_unreachable();
}
