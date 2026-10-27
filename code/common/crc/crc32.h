#ifndef COMMON_CRC32_H
#define COMMON_CRC32_H

/* CRC-32 as in zlib, PNG and JavaScript's usual implementations (reflected
 * polynomial 0xEDB88320, init and final XOR 0xFFFFFFFF), so the update tool
 * can check what the bootloader wrote against the image it sent. In software,
 * a nibble at a time: about 10 ms over the whole application region, and no
 * hardware CRC unit to configure in the bootloader. Host-tested. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start with crc = 0, then feed any number of pieces in order. */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_CRC32_H */
