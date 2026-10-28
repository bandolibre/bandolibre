/*
 * USB descriptors for the Bandolibre bootloader.
 *
 * Two configurations, picked once at boot by usb_descriptors_set_drive():
 *
 *   MIDI only     entered from the web update tool (ENTER_UPDATE_MODE). No
 *                 drive appears, so the OS shows nothing but a MIDI port and
 *                 macOS has no unejected disk to complain about at the reset.
 *   MIDI + MSC    entered with FN2 held, from the console `dfu`, or because
 *                 the application is missing or interrupted: the BANDOLIBRE
 *                 drive takes a .uf2 copy, and MIDI still serves the web tool.
 *
 * Both also carry a DFU runtime interface (common/dfu/dfu_runtime.h), which
 * restarts the board into the STM32's own DFU loader on DFU_DETACH.
 */

#include "tusb.h"
#include "stm32g4xx_hal.h"
#include "usb_descriptors.h"
#include "dfu_runtime.h"

/* Same sublicensed ST ids as the application (main-g474/usb/usb_descriptors.c):
 * one product id covers the whole product, so the bootloader is told apart by
 * bcdDevice instead. Hosts cache a driver set per VID/PID/revision and the
 * three interface layouts differ, so the revisions must stay distinct.
 *
 *   application              bcdDevice 0x0101
 *   bootloader, MIDI only    bcdDevice 0x0203
 *   bootloader, MIDI + MSC   bcdDevice 0x0204
 *
 * (Bumped from 0x0100, 0x0201 and 0x0202 when the DFU runtime interface was
 * added, so Windows does not reuse what it cached for the old layouts.)
 */
#define USB_VID  0x0483  /* STMicroelectronics */
#define USB_PID  0xA5B4  /* Bandolibre */
#define BCD_DEVICE_MIDI   0x0203
#define BCD_DEVICE_DRIVE  0x0204

static bool with_drive;

void usb_descriptors_set_drive(bool drive)
{
  with_drive = drive;
}

bool usb_descriptors_has_drive(void)
{
  return with_drive;
}

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
static tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    /* 2.1: Windows then asks for the BOS, and finds the Microsoft OS 2.0
     * descriptors that bind WinUSB to the DFU runtime interface. */
    .bcdUSB             = 0x0210,

    /* Class is declared per interface, not per device. */
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = BCD_DEVICE_MIDI,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void) {
  desc_device.bcdDevice = with_drive ? BCD_DEVICE_DRIVE : BCD_DEVICE_MIDI;
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptors
//--------------------------------------------------------------------+
enum {
  ITF_NUM_MIDI = 0,
  ITF_NUM_MIDI_STREAMING,
  ITF_NUM_DFU_RT,   /* the same number in both configurations */
  ITF_NUM_MSC,
};
TU_VERIFY_STATIC(ITF_NUM_DFU_RT == DFU_RUNTIME_ITF, "the MS OS 2.0 descriptors name this interface");
#define ITF_COUNT_MIDI_ONLY   3
#define ITF_COUNT_MIDI_DRIVE  4

/* Endpoint addresses (STM32 fsdev: each number is a bidirectional pair). */
#define EPNUM_MIDI_OUT  0x01
#define EPNUM_MIDI_IN   0x81
#define EPNUM_MSC_OUT   0x02
#define EPNUM_MSC_IN    0x82

#define STRID_INTERFACE 4

static uint8_t const desc_midi_only[] = {
    /* config number, interface count, string index, total length, attribute, power in mA */
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT_MIDI_ONLY, 0,
                          TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_DFU_RT_DESC_LEN, 0x00, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, STRID_INTERFACE, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 64),
    DFU_RUNTIME_DESCRIPTOR(STRID_INTERFACE),
};

static uint8_t const desc_midi_drive[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT_MIDI_DRIVE, 0,
                          TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_DFU_RT_DESC_LEN + TUD_MSC_DESC_LEN,
                          0x00, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, STRID_INTERFACE, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 64),
    DFU_RUNTIME_DESCRIPTOR(STRID_INTERFACE),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, STRID_INTERFACE, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return with_drive ? desc_midi_drive : desc_midi_only;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

/* Manufacturer, product and interface are the same string on purpose: the MIDI
 * port name comes from the product on Linux and macOS and from the
 * manufacturer on Windows, so on every OS it says "firmware update", which the
 * configuration tool and bandopm exclude from their auto-connect (the
 * application's strings are plain "Bandolibre"). ASCII only, so no UTF-8
 * decoding is needed. */
#define DEVICE_NAME "Bandolibre firmware update"

static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: supported language is English (0x0409)
    DEVICE_NAME,                    // 1: Manufacturer
    DEVICE_NAME,                    // 2: Product
    NULL,                           // 3: Serial, derived from the chip UID
    DEVICE_NAME,                    // 4: MIDI and MSC interfaces
};

/* 96-bit chip UID as 24 hex digits, the same serial the application reports.
 * Windows requires an MSC serial of at least 12 characters. */
static const char *serial_string(void) {
  static char serial[25];
  static const char hex[] = "0123456789ABCDEF";
  uint32_t const uid[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };

  for (size_t w = 0; w < 3; w++) {
    for (size_t n = 0; n < 8; n++) {
      serial[w * 8 + n] = hex[(uid[w] >> (28 - 4 * n)) & 0xF];
    }
  }
  serial[24] = '\0';
  return serial;
}

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;

  size_t chr_count;

  if (index == 0) {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else {
    if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;

    const char *str = (index == 3) ? serial_string() : string_desc_arr[index];

    chr_count = strlen(str);
    size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
    if (chr_count > max_count) chr_count = max_count;

    for (size_t i = 0; i < chr_count; i++) {
      _desc_str[1 + i] = (uint16_t) str[i];
    }
  }

  /* first byte is length (including header), second byte is descriptor type */
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}
