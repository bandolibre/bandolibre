#include "bootloader.h"

#include "main.h"      /* HAL_Delay, NVIC_SystemReset */
#include "memmap.h"
#include "tusb.h"
#include "version.h"   /* FIRMWARE_VERSION_STRING */

_Static_assert(sizeof(FIRMWARE_VERSION_STRING) <= APP_INFO_VERSION_LEN,
               "the version string does not fit app_info_t");

/* Placed at APP_INFO_ADDR by STM32G474XX_FLASH_APP.ld; nothing references it,
 * the linker script KEEPs it. */
__attribute__((section(".app_info"), used))
const app_info_t app_info = {
  .magic = APP_INFO_MAGIC,
  .version = FIRMWARE_VERSION_STRING,
};

void reboot_to_bootloader(bool drive)
{
  /* Keep USB running while the reply goes out: a write only starts a
   * transfer when the endpoint is idle, and the rest of the FIFO leaves from
   * tud_task(). Not usb_app_task(), which would dispatch new sysex into the
   * handler that called us. */
  for (uint32_t const start = HAL_GetTick(); HAL_GetTick() - start < 50U;) tud_task();
  *(volatile uint32_t *)BOOT_FLAG_ADDR = drive ? BOOT_FLAG_MAGIC : BOOT_FLAG_MAGIC_MIDI;
  NVIC_SystemReset();
}
