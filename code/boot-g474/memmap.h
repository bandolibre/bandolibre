#ifndef BOOT_MEMMAP_H
#define BOOT_MEMMAP_H

/* Flash and RAM layout shared by the bootloader and the main board application.
 *
 * This header is the single source of truth for the split. Four other places
 * repeat these numbers and must agree with it:
 *
 *   - boot-g474/STM32G474XX_FLASH.ld        (bootloader region)
 *   - main-g474/STM32G474XX_FLASH_APP.ld    (application region, .app_info)
 *   - main-g474/CMakeLists.txt              (VECT_TAB_OFFSET)
 *   - tool/uf2.py                           (default --base)
 *
 * code/tests/test_dfu_config.py parses all of them and fails if they drift.
 *
 * The STM32G474CBT6 has 128 KB of flash in 2 KB pages:
 *
 *   0x08000000  bootloader          32 KB   this project
 *   0x08008000  application         92 KB   main-g474, Debug and Release
 *   0x0801F000  properties A         2 KB   saved property values, see below
 *   0x0801F800  properties B         2 KB
 *   0x08020000  end
 *
 * Both builds of main-g474 link above the bootloader (both are -Os), so
 * flashing a Debug image over SWD leaves the bootloader in place.
 *
 * The last two pages hold the saved property values, an append-only log over
 * two one-page partitions (main-g474/properties/property_store.h explains the
 * format and why one page each). Neither linker map reaches them, the
 * bootloader refuses images larger than APP_SIZE, and tool/flash.py and
 * tool/uf2.py refuse binaries that would run into PROPS_BASE, so flashing a
 * new firmware never erases the user's settings.
 */

#define FLASH_ORIGIN      0x08000000UL
#define FLASH_TOTAL_SIZE  (128UL * 1024UL)

/* Bootloader: the bottom of flash, where the reset vector always lands. */
#define BOOT_BASE         FLASH_ORIGIN
#define BOOT_SIZE         (32UL * 1024UL)

/* Application. */
#define APP_BASE          (BOOT_BASE + BOOT_SIZE)
#define APP_SIZE          (92UL * 1024UL)

/* Version record of the application (main-g474/app/bootloader.c), placed by its
 * linker script right after the 0x1D8-byte vector table, so the bootloader can
 * tell the update tool which firmware is installed. Read only when the vector
 * table looks valid. */
#define APP_INFO_ADDR         (APP_BASE + 0x200UL)
#define APP_INFO_MAGIC        0x49414C42UL   /* "BLAI" little-endian */
#define APP_INFO_VERSION_LEN  48

#ifndef __ASSEMBLER__
#include <stdint.h>
typedef struct {
  uint32_t magic;
  char version[APP_INFO_VERSION_LEN];   /* NUL-terminated */
} app_info_t;
#endif

/* Saved properties: two partitions of one page each, not writable through DFU.
 * Partition p starts at PROPS_BASE + p * PROPS_PARTITION_SIZE. */
#define PROPS_BASE            (APP_BASE + APP_SIZE)
#define PROPS_PARTITION_SIZE  (2UL * 1024UL)
#define PROPS_SIZE            (2UL * PROPS_PARTITION_SIZE)

/* Erase granularity is not a constant here: it depends on the DBANK option
 * bit, so flash_write.c reads it at run time. The HAL defines its own
 * FLASH_PAGE_SIZE for the dual-bank default; do not shadow it. */

/* Soft entry into the bootloader.
 *
 * The application stores one of two magics here and resets; the bootloader
 * reads the word, clears it, and stays in update mode if it matched. A system
 * reset does not clear RAM, so the value survives the reboot.
 *
 *   BOOT_FLAG_MAGIC       console `dfu`: MIDI and the BANDOLIBRE drive
 *   BOOT_FLAG_MAGIC_MIDI  ENTER_UPDATE_MODE from the web update tool: MIDI
 *                         only, so no drive pops up during a web update
 *
 * This is the top 32 bytes of SRAM, carved out of the RAM region in both
 * linker scripts so neither the stack (_estack sits just below it) nor .bss
 * can land on it. A backup register would need the RTC/TAMP backup domain,
 * which the .ioc does not enable.
 *
 * After a power-on reset the word holds whatever the SRAM powered up with, so
 * the magic is a full 32 bits: the odds of a false DFU entry are 1 in 2^32,
 * and the bootloader clears the word before doing anything else either way.
 */
#define BOOT_FLAG_ADDR    0x2001FFE0UL
#define BOOT_FLAG_MAGIC       0xB00710ADUL
#define BOOT_FLAG_MAGIC_MIDI  0xB0071DD1UL

/* UF2 family id for ST STM32G4xx, from microsoft/uf2 utils/uf2families.json.
 * Blocks carrying any other family id are ignored, so a UF2 built for a
 * different chip cannot be written to this one. */
#define UF2_FAMILY_ID     0x4C71240AUL

#endif /* BOOT_MEMMAP_H */
