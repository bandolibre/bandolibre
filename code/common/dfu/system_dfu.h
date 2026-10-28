#ifndef COMMON_SYSTEM_DFU_H_
#define COMMON_SYSTEM_DFU_H_

/* Restarting into the STM32G474's own USB DFU loader, in ROM (system memory,
 * AN2606), from software: what holding FN0 (BOOT0) at power-on does, without
 * the button and without unplugging the left keyboard. Shared by the
 * application and the bootloader of the main board. */

#ifdef __cplusplus
extern "C" {
#endif

/* Restarts both keyboards into their own ROM loaders (they send nothing
 * there), drops off USB, puts the clocks and peripherals back as a reset
 * leaves them, and jumps into the ROM loader, which then shows up as USB DFU
 * device 0483:df11. Does not return; the board comes back from it with the
 * update tool's Exit DFU (or a replug), and the application restarts the
 * keyboards into their firmware. */
void system_dfu_enter(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* COMMON_SYSTEM_DFU_H_ */
