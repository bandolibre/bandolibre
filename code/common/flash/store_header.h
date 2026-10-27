#ifndef COMMON_STORE_HEADER_H
#define COMMON_STORE_HEADER_H

/* The header of a saved-values partition (main-g474/properties/
 * property_store.h has the whole format): one flash double word,
 * u32 magic "BLPS", u16 generation, u16 crc16 over the first 6 bytes.
 *
 * Shared by the property store, which writes it, and the bootloader, which
 * reads the generation for the update tool. Little-endian, as on the chip. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STORE_MAGIC 0x53504C42u   /* "BLPS" little-endian */

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over the first 6 bytes. */
static inline uint16_t store_header_crc16(const uint8_t *b)
{
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < 6; i++) {
    crc ^= (uint16_t) (b[i] << 8);
    for (int bit = 0; bit < 8; bit++)
      crc = (crc & 0x8000) ? (uint16_t) ((crc << 1) ^ 0x1021) : (uint16_t) (crc << 1);
  }
  return crc;
}

#ifdef __cplusplus
}
#endif

#endif /* COMMON_STORE_HEADER_H */
