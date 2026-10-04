#ifndef COMMON_FLASH_PAGE_H
#define COMMON_FLASH_PAGE_H

/* Page erase and double-word programming of the STM32G474's internal flash.
 *
 * Shared by the bootloader (writing the application) and the main firmware
 * (writing the saved properties), so the one subtle part, the page encoding
 * of an erase on this 128 KB part, lives in a single place.
 *
 * The caller unlocks the flash (HAL_FLASH_Unlock) around these calls. Both
 * stall instruction fetch while they run: code executes from the same flash.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Size of one erasable page, read from the DBANK option bit at run time:
 * 2 KB when the flash is in its (factory default) dual-bank configuration,
 * 4 KB in single-bank. */
uint32_t flash_page_size(void);

/* Erase the page starting at `addr` (page aligned) and read it back: true only
 * if every word is 0xFFFFFFFF afterwards. Flushes the flash caches, so reads
 * through a pointer see the erased contents. */
bool flash_erase_page(uint32_t addr);

/* Program one 64-bit double word at `addr` (8-byte aligned, erased). */
bool flash_program_doubleword(uint32_t addr, uint64_t word);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_FLASH_PAGE_H */
