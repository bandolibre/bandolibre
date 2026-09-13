/*
 * TinyUSB configuration for the Bandolibre main board (STM32G474, USB FS device).
 * Composite device: MIDI + CDC (console mirror).
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// CFG_TUSB_MCU is defined to OPT_MCU_STM32G4 by the build system (CMakeLists.txt)
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#define CFG_TUSB_OS               OPT_OS_NONE

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG            0
#endif

// Enable Device stack
#define CFG_TUD_ENABLED           1

// STM32G474 USB peripheral is Full Speed only
#define CFG_TUD_MAX_SPEED         OPT_MODE_FULL_SPEED

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN        __attribute__ ((aligned(4)))

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUD_ENDPOINT0_SIZE    64

//------------- CLASS -------------//
#define CFG_TUD_CDC               0
#define CFG_TUD_MSC               0
#define CFG_TUD_HID               0
#define CFG_TUD_MIDI              1
#define CFG_TUD_VENDOR            0

// MIDI FIFO size of TX and RX. TX must hold a full sysex response frame in
// one go: usb_app_midi_send_sysex() writes header/body/footer back-to-back
// with no draining in between, so anything that doesn't fit here gets
// silently truncated by tud_midi_stream_write() rather than queued. Sized to
// match MAX_SYSEX (usb_app.cc), the protocol's own max frame size.
#define CFG_TUD_MIDI_RX_BUFSIZE   64
#define CFG_TUD_MIDI_TX_BUFSIZE   256

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */
