#include "dfu_runtime.h"

#include <stdbool.h>

#include "stm32g4xx_hal.h"
#include "system_dfu.h"

static volatile bool detach_requested;

/* TinyUSB calls this on DFU_DETACH, before the request's status stage has gone
 * out: only note it here, and leave from the main loop. */
void tud_dfu_runtime_reboot_to_dfu_cb(void)
{
  detach_requested = true;
}

void dfu_runtime_task(void)
{
  if (!detach_requested) return;
  for (uint32_t const start = HAL_GetTick(); HAL_GetTick() - start < 20U;) tud_task();
  system_dfu_enter();
}

//--------------------------------------------------------------------+
// Microsoft OS 2.0 descriptors: WinUSB for the DFU runtime interface
//--------------------------------------------------------------------+

#define MS_OS_20_VENDOR_CODE  0x01
#define MS_OS_20_REQUEST      7   /* wIndex of MS_OS_20_DESCRIPTOR_INDEX */

/* Set header (10) + configuration subset (8) + function subset (8) +
 * compatible id (20) + registry property (132). */
#define MS_OS_20_DESC_LEN     178

static uint8_t const desc_ms_os_20[] = {
  /* Set header: length, type, Windows 8.1 or later, total length */
  U16_TO_U8S_LE(10), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR),
  U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

  /* Configuration subset: length, type, configuration index, reserved, length */
  U16_TO_U8S_LE(8), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION),
  0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 10),

  /* Function subset: length, type, first interface, reserved, length */
  U16_TO_U8S_LE(8), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION),
  DFU_RUNTIME_ITF, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 10 - 8),

  /* Compatible id: WINUSB */
  U16_TO_U8S_LE(20), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID),
  'W', 'I', 'N', 'U', 'S', 'B', 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,

  /* Registry property: DeviceInterfaceGUIDs (REG_MULTI_SZ), so libusb and
   * browsers find the interface */
  U16_TO_U8S_LE(132), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
  U16_TO_U8S_LE(7), U16_TO_U8S_LE(42),
  'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0, 'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0,
  'f', 0, 'a', 0, 'c', 0, 'e', 0, 'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0, 0,
  U16_TO_U8S_LE(80),
  '{', 0, '1', 0, 'C', 0, '3', 0, 'B', 0, '0', 0, '2', 0, '8', 0, '5', 0, '-', 0,
  '8', 0, 'B', 0, '9', 0, '6', 0, '-', 0, '4', 0, '8', 0, '2', 0, '1', 0, '-', 0,
  '8', 0, 'A', 0, 'C', 0, 'C', 0, '-', 0, '2', 0, 'B', 0, '9', 0, 'A', 0, '3', 0,
  'E', 0, 'C', 0, 'C', 0, 'C', 0, 'F', 0, 'E', 0, 'D', 0, '}', 0, 0, 0, 0, 0,
};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "MS OS 2.0 descriptor length");

#define BOS_TOTAL_LEN (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

static uint8_t const desc_bos[] = {
  TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),
  TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, MS_OS_20_VENDOR_CODE),
};

uint8_t const *tud_descriptor_bos_cb(void)
{
  return desc_bos;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
  if (stage != CONTROL_STAGE_SETUP) return true;
  if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR &&
      request->bRequest == MS_OS_20_VENDOR_CODE && request->wIndex == MS_OS_20_REQUEST) {
    return tud_control_xfer(rhport, request, (void *) (uintptr_t) desc_ms_os_20, sizeof(desc_ms_os_20));
  }
  return false;
}
