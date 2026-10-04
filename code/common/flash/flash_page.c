#include "flash_page.h"

#include <stdio.h>

#include "memmap.h"
#include "stm32g4xx_hal.h"

/* DBANK selects the page size: 2 KB (set, the factory default, and what
 * `st-info --probe` reports as "pagesize: 2048") or 4 KB (clear). */
static bool dual_bank(void)
{
  return (FLASH->OPTR & FLASH_OPTR_DBANK) != 0U;
}

uint32_t flash_page_size(void)
{
  return dual_bank() ? 2048U : 4096U;
}

/* PNB is an absolute page index from the start of flash, and BKER selects a
 * bank only on G4 parts larger than 128 KB. DBANK=1 on this 128 KB part buys
 * 2 KB pages, not a second erase bank, so BKER stays clear and 0x08010000 is
 * page 32 rather than bank 2 page 0. Aiming an erase at the bank this device
 * does not have completes with no error flag and no effect. Same rule as
 * stlink's G4 erase path and dmitrystu/sboot_stm32. */
static void addr_to_page(uint32_t addr, uint32_t *bank, uint32_t *page)
{
  uint32_t const offset = addr - FLASH_ORIGIN;
  uint32_t const size = (uint32_t) (*(volatile uint16_t *) FLASHSIZE_BASE) * 1024U;

  *page = offset / flash_page_size();
  *bank = (size > (128U * 1024U) && offset >= size / 2U) ? FLASH_BANK_2 : FLASH_BANK_1;
}

/* Driven register-by-register rather than through HAL_FLASHEx_Erase(), whose
 * API only takes the bank-relative page encoding this part does not use (see
 * addr_to_page). The read-back matters for the same reason: a misdirected
 * erase still reports success, so the status is not evidence the page is
 * clear. */
bool flash_erase_page(uint32_t addr)
{
  uint32_t bank, page;
  addr_to_page(addr, &bank, &page);

  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_SR_ERRORS);

  /* PG/FSTPG/MER1/MER2 are not cleared by setting PER, and leaving them set
   * alongside it raises PGSERR (RM0440 3.3.8). */
  CLEAR_BIT(FLASH->CR, FLASH_CR_PG | FLASH_CR_FSTPG | FLASH_CR_MER1 | FLASH_CR_MER2);
  MODIFY_REG(FLASH->CR, FLASH_CR_BKER, (bank == FLASH_BANK_2) ? FLASH_CR_BKER : 0U);
  MODIFY_REG(FLASH->CR, FLASH_CR_PNB, page << FLASH_CR_PNB_Pos);
  SET_BIT(FLASH->CR, FLASH_CR_PER);
  SET_BIT(FLASH->CR, FLASH_CR_STRT);

  HAL_StatusTypeDef const status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);
  CLEAR_BIT(FLASH->CR, FLASH_CR_PER | FLASH_CR_PNB | FLASH_CR_BKER);
  FLASH_FlushCaches();

  if (status != HAL_OK) {
    printf("flash: erase failed at 0x%08lx (bank %lu page %lu): status=%d hal_error=0x%08lx\r\n",
           (unsigned long) addr, (unsigned long) bank, (unsigned long) page, (int) status,
           (unsigned long) HAL_FLASH_GetError());
    return false;
  }

  volatile uint32_t const *check = (volatile uint32_t const *) addr;
  uint32_t const words = flash_page_size() / 4U;
  for (uint32_t i = 0; i < words; i++) {
    if (check[i] != 0xFFFFFFFFU) {
      printf("flash: erase at 0x%08lx (bank %lu page %lu) reported OK but word %lu reads "
             "0x%08lx\r\n",
             (unsigned long) addr, (unsigned long) bank, (unsigned long) page, (unsigned long) i,
             (unsigned long) check[i]);
      return false;
    }
  }
  return true;
}

bool flash_program_doubleword(uint32_t dest, uint64_t word)
{
  HAL_StatusTypeDef const status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dest, word);
  if (status == HAL_OK) return true;

  printf("flash: program failed at 0x%08lx: status=%d hal_error=0x%08lx\r\n", (unsigned long) dest,
         (int) status, (unsigned long) HAL_FLASH_GetError());
  return false;
}
