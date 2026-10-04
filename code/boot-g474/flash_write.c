#include "flash_write.h"

#include <stdio.h>
#include <string.h>

#include "flash_page.h"
#include "memmap.h"
#include "stm32g4xx_hal.h"

/* Program one page's worth of the staged image. `offset` is relative to
 * APP_BASE and page-aligned; `len` is the number of bytes of `image` that fall
 * in this page (the last page is usually short). */
static bool program_page(const uint8_t *image, uint32_t offset, uint32_t len)
{
  for (uint32_t i = 0; i < len; i += 8U) {
    /* HAL_FLASH_Program takes the doubleword by value, and the staging buffer
     * is only 4-byte aligned, so copy through a local rather than casting. */
    uint64_t word = 0xFFFFFFFFFFFFFFFFULL;
    uint32_t const chunk = (len - i < 8U) ? (len - i) : 8U;
    memcpy(&word, image + offset + i, chunk);

    if (!flash_program_doubleword(APP_BASE + offset + i, word)) return false;
  }
  return true;
}

bool flash_write_app(const uint8_t *image, size_t len)
{
  if (len == 0U || len > APP_SIZE) {
    printf("flash: refusing to write %lu bytes (APP_SIZE=%lu)\r\n",
           (unsigned long) len, (unsigned long) APP_SIZE);
    return false;
  }

  uint32_t const page = flash_page_size();
  uint32_t const total = ((uint32_t) len + page - 1U) & ~(page - 1U);
  uint32_t const pages = total / page;

  printf("flash: writing %lu bytes, page=%lu bytes, %lu pages, DBANK=%d, FLASHSIZE=%u KB\r\n",
         (unsigned long) len, (unsigned long) page, (unsigned long) pages,
         (FLASH->OPTR & FLASH_OPTR_DBANK) != 0U, *(uint16_t *) FLASHSIZE_BASE);

  if (HAL_FLASH_Unlock() != HAL_OK) {
    printf("flash: HAL_FLASH_Unlock failed, hal_error=0x%08lx\r\n",
           (unsigned long) HAL_FLASH_GetError());
    return false;
  }

  bool ok = true;

  /* Erase everything the image covers first. This clears the vector table
   * immediately, so from here until the final program step the application is
   * invalid and an interrupted update falls back into DFU mode. */
  for (uint32_t p = 0; p < pages && ok; p++) {
    ok = flash_erase_page(APP_BASE + p * page);
  }
  printf("flash: erase %s\r\n", ok ? "done" : "FAILED");

  /* Program from the second page upwards, leaving the vector table for last. */
  for (uint32_t p = 1; p < pages && ok; p++) {
    uint32_t const offset = p * page;
    uint32_t const chunk = ((uint32_t) len - offset < page) ? ((uint32_t) len - offset) : page;
    ok = program_page(image, offset, chunk);
  }
  printf("flash: program pages 1..%lu %s\r\n", (unsigned long) (pages - 1U),
         ok ? "done" : "FAILED");

  if (ok) {
    uint32_t const chunk = ((uint32_t) len < page) ? (uint32_t) len : page;
    ok = program_page(image, 0U, chunk);
    printf("flash: program vector table page (0) %s\r\n", ok ? "done" : "FAILED");
  }

  HAL_FLASH_Lock();
  return ok;
}
