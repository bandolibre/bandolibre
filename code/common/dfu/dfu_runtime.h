#ifndef COMMON_DFU_RUNTIME_H_
#define COMMON_DFU_RUNTIME_H_

/* USB DFU runtime interface (DFU 1.1), for the main board's application and
 * bootloader: a standard DFU tool (dfu-util, the update page over WebUSB)
 * sends it DFU_DETACH, and the board restarts into the STM32's own DFU loader
 * in ROM (system_dfu.h). Each configuration descriptor includes
 * DFU_RUNTIME_DESCRIPTOR() as interface DFU_RUNTIME_ITF; this module also
 * provides the Microsoft OS 2.0 descriptors that bind WinUSB to it on
 * Windows, so no driver needs installing there. The device descriptor needs
 * bcdUSB 0x0210 for Windows to ask for them. */

#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The same interface number in every configuration: after the two MIDI
 * interfaces, before the bootloader's mass storage. */
#define DFU_RUNTIME_ITF 2

/* The ROM loader detaches itself, takes downloads and uploads, in 2 KB
 * blocks; the 1 s detach timeout covers the jump. */
#define DFU_RUNTIME_DESCRIPTOR(_stridx)                                                    \
  TUD_DFU_RT_DESCRIPTOR(DFU_RUNTIME_ITF, _stridx,                                          \
                        DFU_ATTR_WILL_DETACH | DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_CAN_UPLOAD, \
                        1000, 2048)

/* Called from the main loop: after a DFU_DETACH, lets USB finish answering it,
 * then enters the ROM loader (does not return). Otherwise returns at once. */
void dfu_runtime_task(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_DFU_RUNTIME_H_ */
