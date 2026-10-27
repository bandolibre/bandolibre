#include "midi_update.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crc32.h"
#include "flash_page.h"
#include "memmap.h"
#include "midi_tx.h"
#include "msc_disk.h"
#include "stm32g4xx_hal.h"
#include "store_header.h"
#include "sysex_codec.h"
#include "sysex_ids.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "version.h"   /* FIRMWARE_VERSION_STRING, "Bandolibre v1.2..." */

/* Largest data chunk of one UPDATE_WRITE, announced by UPDATE_INFO. A 2 KB
 * page takes 8 of them, and a frame stays near 300 bytes on the wire. */
#define MAX_CHUNK    256u
#define MAX_PAYLOAD  (8u + MAX_CHUNK)   /* id, seq, session, offset, data */
#define MAX_REPLY    96u
#define MAX_PAGES    64u                /* APP_SIZE / the smallest page, rounded up */

_Static_assert(APP_SIZE / 2048u <= MAX_PAGES, "the erased-page bitmap is too small");
/* midi_tx_send() waits when the TX FIFO is full; sizing it for a whole reply
 * (4-byte USB-MIDI packets of 3 sysex bytes) just means it never has to. */
_Static_assert((SYSEX_FRAME_SIZE(MAX_REPLY) + 2u) / 3u * 4u <= CFG_TUD_MIDI_TX_BUFSIZE,
               "a whole reply should fit the MIDI TX FIFO");

static uint8_t rx_buf[SYSEX_FRAME_SIZE(MAX_PAYLOAD)];
static sysex_rx_t rx = { rx_buf, sizeof(rx_buf), 0 };
static uint8_t decoded[MAX_PAYLOAD + 2];

static struct {
  uint16_t id;          /* 0 = no session */
  uint32_t size;        /* image size, a multiple of 8 */
  uint32_t crc;         /* CRC-32 of the whole image */
  uint8_t head[8];      /* the image's first double word, programmed at commit */
  uint64_t erased;      /* bit p: page p was erased in this session */
} session;

static bool reboot_pending;
static uint32_t reboot_from;

bool midi_update_active(void)
{
  return session.id != 0;
}

void midi_update_cancel(void)
{
  session.id = 0;
}

//--------------------------------------------------------------------+
// Little-endian field reader and reply writer
//--------------------------------------------------------------------+

typedef struct {
  const uint8_t *p;
  size_t n;
  bool ok;
} reader_t;

static uint32_t rd(reader_t *r, size_t bytes)
{
  if (r->n < bytes) { r->ok = false; return 0; }
  uint32_t v = 0;
  for (size_t i = 0; i < bytes; i++) v |= (uint32_t) r->p[i] << (8 * i);
  r->p += bytes;
  r->n -= bytes;
  return v;
}

typedef struct {
  uint8_t buf[MAX_REPLY];
  size_t n;
} writer_t;

static void wr(writer_t *w, uint32_t v, size_t bytes)
{
  for (size_t i = 0; i < bytes && w->n < sizeof(w->buf); i++) w->buf[w->n++] = (uint8_t) (v >> (8 * i));
}

static void wr_str(writer_t *w, const char *s, bool terminate)
{
  while (*s && w->n < sizeof(w->buf)) w->buf[w->n++] = (uint8_t) *s++;
  if (terminate) wr(w, 0, 1);
}

static void send(const writer_t *w)
{
  uint8_t frame[SYSEX_FRAME_SIZE(MAX_REPLY)];
  size_t const n = sysex_pack(w->buf, w->n, frame, sizeof(frame));
  if (n) midi_tx_send(frame, n);
}

/* Starts a reply to an update request: [id, seq, status]. */
static void reply_head(writer_t *w, uint8_t id, uint8_t seq, uint8_t status)
{
  w->n = 0;
  wr(w, id, 1);
  wr(w, seq, 1);
  wr(w, status, 1);
}

static void reply_status(uint8_t id, uint8_t seq, uint8_t status)
{
  writer_t w;
  reply_head(&w, id, seq, status);
  send(&w);
}

//--------------------------------------------------------------------+
// Flash
//--------------------------------------------------------------------+

static bool erase_page(uint32_t page)
{
  session.erased &= ~(1ULL << page);
  HAL_FLASH_Unlock();
  bool const ok = flash_erase_page(APP_BASE + page * flash_page_size());
  HAL_FLASH_Lock();
  if (ok) session.erased |= 1ULL << page;
  return ok;
}

static uint64_t read_dw(uint32_t offset)
{
  uint64_t v = 0;
  if (!flash_read(APP_BASE + offset, &v, sizeof(v))) v = 0;   /* torn: neither erased nor equal */
  return v;
}

static bool program_dw(uint32_t offset, uint64_t v)
{
  HAL_FLASH_Unlock();
  bool const ok = flash_program_doubleword(APP_BASE + offset, v);
  HAL_FLASH_Lock();
  return ok && read_dw(offset) == v;
}

/* CRC-32 of [offset, offset+len) of the application as it will be once
 * committed: the first double word comes from RAM. */
static uint32_t image_crc(uint32_t offset, uint32_t len)
{
  uint32_t crc = 0;
  if (offset < sizeof(session.head) && len > 0) {
    uint32_t const n = (len < sizeof(session.head) - offset) ? len : sizeof(session.head) - offset;
    crc = crc32_update(crc, session.head + offset, n);
    offset += n;
    len -= n;
  }
  flash_ecc_fault = false;
  crc = crc32_update(crc, (const uint8_t *) (APP_BASE + offset), len);
  if (flash_ecc_fault) crc = ~crc;   /* a torn double word never matches */
  return crc;
}

//--------------------------------------------------------------------+
// What is installed
//--------------------------------------------------------------------+

static uint8_t app_state(void)
{
  if (app_valid()) return APP_STATE_VALID;
  uint64_t const erased = ~0ULL;
  uint64_t head;
  /* Torn by a power cut during COMMIT's last write. */
  if (!flash_read(APP_BASE, &head, sizeof(head))) return APP_STATE_INTERRUPTED;
  if (head != erased) return APP_STATE_INVALID;
  /* No vector table but something above it: an update that BEGIN started and
   * COMMIT never finished. An empty region is just empty. */
  for (uint32_t offset = 8; offset < APP_SIZE; offset += 8)
    if (read_dw(offset) != erased) return APP_STATE_INTERRUPTED;
  return APP_STATE_INVALID;
}

/* The version record of the installed application (memmap.h), or "". */
static const char *app_version(void)
{
  static app_info_t info;
  if (!app_valid() || !flash_read(APP_INFO_ADDR, &info, sizeof(info)) || info.magic != APP_INFO_MAGIC)
    return "";
  info.version[APP_INFO_VERSION_LEN - 1] = '\0';
  return info.version;
}

/* Generation of the saved-values store, as the application reports it: the
 * newer of the two partitions with a valid header (common/flash/
 * store_header.h), 0 if none. */
static uint16_t store_generation(void)
{
  uint16_t best = 0;
  for (uint32_t p = 0; p < 2; p++) {
    uint8_t h[8];
    if (!flash_read(PROPS_BASE + p * PROPS_PARTITION_SIZE, h, sizeof(h))) continue;
    uint32_t const magic = (uint32_t) h[0] | (uint32_t) h[1] << 8 | (uint32_t) h[2] << 16 | (uint32_t) h[3] << 24;
    uint16_t const gen = (uint16_t) (h[4] | h[5] << 8);
    uint16_t const crc = (uint16_t) (h[6] | h[7] << 8);
    if (magic != STORE_MAGIC || gen == 0 || crc != store_header_crc16(h)) continue;
    if (best == 0 || (int16_t) (uint16_t) (gen - best) > 0) best = gen;
  }
  return best;
}

//--------------------------------------------------------------------+
// Requests
//--------------------------------------------------------------------+

/* [id, string version, u8 mode, u16 generation], the part of HELLO common to
 * both modes. */
static void handle_hello(void)
{
  static char const prefix[] = "Bandolibre ";
  char const *tag = FIRMWARE_VERSION_STRING;
  if (strncmp(tag, prefix, sizeof(prefix) - 1) == 0) tag += sizeof(prefix) - 1;

  writer_t w = { .n = 0 };
  wr(&w, SYSEX_MSG_HELLO, 1);
  wr_str(&w, "Bandolibre bootloader ", false);
  wr_str(&w, tag, true);
  wr(&w, SYSEX_MODE_UPDATE, 1);
  wr(&w, store_generation(), 2);
  send(&w);
}

static void handle_info(uint8_t seq)
{
  writer_t w;
  reply_head(&w, SYSEX_MSG_UPDATE_INFO, seq, UPDATE_OK);
  wr(&w, flash_page_size(), 2);
  wr(&w, APP_BASE, 4);
  wr(&w, APP_SIZE, 4);
  wr(&w, MAX_CHUNK, 2);
  wr(&w, app_state(), 1);
  wr_str(&w, app_version(), true);
  wr(&w, session.id, 2);
  wr(&w, usb_descriptors_has_drive(), 1);
  send(&w);
}

static void handle_begin(uint8_t seq, reader_t *r)
{
  uint32_t const size = rd(r, 4);
  uint32_t const crc = rd(r, 4);
  if (!r->ok || size == 0 || size % 8U != 0 || size > APP_SIZE) {
    reply_status(SYSEX_MSG_UPDATE_BEGIN, seq, UPDATE_BAD_ARGS);
    return;
  }
  if (uf2_receiving() || uf2_commit_ready()) {
    reply_status(SYSEX_MSG_UPDATE_BEGIN, seq, UPDATE_BUSY);
    return;
  }

  /* A new number every time, so a client still holding the previous one
   * (another tab, another browser) gets BAD_SESSION from now on. */
  uint16_t id;
  do {
    id = (uint16_t) (SysTick->VAL ^ HAL_GetTick() ^ HAL_GetUIDw0() ^ (HAL_GetUIDw0() >> 16));
  } while (id == 0 || id == session.id);

  session.id = id;
  session.size = size;
  session.crc = crc;
  session.erased = 0;
  memset(session.head, 0xFF, sizeof(session.head));

  /* The point of no return: without page 0 the board boots into update mode
   * until a COMMIT succeeds. */
  bool const ok = erase_page(0);
  printf("update: begin %lu bytes, crc %08lx, session %04x%s\r\n", (unsigned long) size,
         (unsigned long) crc, id, ok ? "" : ", erase FAILED");
  if (!ok) session.id = 0;

  writer_t w;
  reply_head(&w, SYSEX_MSG_UPDATE_BEGIN, seq, ok ? UPDATE_OK : UPDATE_FLASH_ERROR);
  wr(&w, session.id, 2);
  send(&w);
}

static uint8_t do_write(reader_t *r, uint32_t offset)
{
  size_t const len = r->n;
  uint32_t const page_size = flash_page_size();
  /* Written so nothing wraps: offset + len could overflow, and a page number
   * past the region would erase outside it. */
  if (len == 0 || len > MAX_CHUNK || len % 8U != 0 || offset % 8U != 0 ||
      offset >= session.size || len > session.size - offset)
    return UPDATE_BAD_ARGS;
  uint32_t const page = offset / page_size;
  if ((offset + len - 1U) / page_size != page) return UPDATE_BAD_ARGS;

  if ((session.erased & (1ULL << page)) == 0U && !erase_page(page)) return UPDATE_FLASH_ERROR;

  for (size_t i = 0; i < len; i += 8U) {
    uint64_t v;
    memcpy(&v, r->p + i, sizeof(v));
    uint32_t const at = offset + (uint32_t) i;
    if (at == 0U) {
      memcpy(session.head, &v, sizeof(v));   /* programmed by COMMIT */
      continue;
    }
    uint64_t const current = read_dw(at);
    if (current == v) continue;                    /* a retry, or 0xFF padding */
    if (current != ~0ULL) return UPDATE_FLASH_ERROR;   /* needs UPDATE_ERASE first */
    if (!program_dw(at, v)) return UPDATE_FLASH_ERROR;
  }
  return UPDATE_OK;
}

static void handle_write(uint8_t seq, reader_t *r)
{
  uint32_t const offset = rd(r, 4);
  uint8_t const status = r->ok ? do_write(r, offset) : UPDATE_BAD_ARGS;

  writer_t w;
  reply_head(&w, SYSEX_MSG_UPDATE_WRITE, seq, status);
  wr(&w, offset, 4);
  send(&w);
}

static void handle_crc(uint8_t seq, reader_t *r)
{
  uint32_t const offset = rd(r, 4);
  uint32_t const len = rd(r, 4);
  bool const ok = r->ok && offset <= APP_SIZE && len <= APP_SIZE - offset;

  writer_t w;
  reply_head(&w, SYSEX_MSG_UPDATE_CRC, seq, ok ? UPDATE_OK : UPDATE_BAD_ARGS);
  wr(&w, ok ? image_crc(offset, len) : 0U, 4);
  send(&w);
}

static void handle_erase(uint8_t seq, reader_t *r)
{
  uint32_t const offset = rd(r, 4);
  uint32_t const page_size = flash_page_size();
  uint8_t status = UPDATE_BAD_ARGS;
  if (r->ok && offset % page_size == 0U && offset < APP_SIZE)
    status = erase_page(offset / page_size) ? UPDATE_OK : UPDATE_FLASH_ERROR;
  reply_status(SYSEX_MSG_UPDATE_ERASE, seq, status);
}

static uint8_t do_commit(void)
{
  /* Every page of the image must have been erased and written in this
   * session; a page left from an older image would pass no CRC anyway, but
   * this names the problem. */
  uint32_t const page_size = flash_page_size();
  for (uint32_t p = 0; p * page_size < session.size; p++)
    if ((session.erased & (1ULL << p)) == 0U) return UPDATE_BAD_ARGS;

  uint64_t head;
  memcpy(&head, session.head, sizeof(head));
  if (head == ~0ULL) return UPDATE_BAD_ARGS;   /* offset 0 was never sent */

  uint32_t const crc = image_crc(0, session.size);
  if (crc != session.crc) {
    printf("update: commit refused, crc %08lx expected %08lx\r\n", (unsigned long) crc,
           (unsigned long) session.crc);
    return UPDATE_CRC_MISMATCH;
  }

  /* The last write: from here the vector table is valid. */
  if (!program_dw(0, head)) return UPDATE_FLASH_ERROR;
  return UPDATE_OK;
}

static void handle_commit(uint8_t seq)
{
  uint8_t const status = do_commit();
  printf("update: commit %s\r\n", status == UPDATE_OK ? "OK, rebooting" : "FAILED");
  reply_status(SYSEX_MSG_UPDATE_COMMIT, seq, status);
  if (status != UPDATE_OK) return;

  session.id = 0;
  reboot_pending = true;
  reboot_from = HAL_GetTick();
}

/* Back to the instrument without updating: the tool gave up before BEGIN, or
 * the player chose not to update. Only with a valid application, so a board
 * that needs a firmware stays where it can get one. */
static void handle_leave(uint8_t seq)
{
  bool const ok = app_valid();
  reply_status(SYSEX_MSG_UPDATE_LEAVE, seq, ok ? UPDATE_OK : UPDATE_BAD_ARGS);
  if (!ok) return;
  printf("update: leaving update mode\r\n");
  session.id = 0;
  reboot_pending = true;
  reboot_from = HAL_GetTick();
}

static void dispatch(const uint8_t *payload, size_t len)
{
  uint8_t const id = payload[0];
  if (id == SYSEX_MSG_HELLO) {
    handle_hello();
    return;
  }
  if (id < SYSEX_MSG_UPDATE_INFO || id > SYSEX_MSG_UPDATE_LEAVE || len < 2) return;   /* not ours */
  if (reboot_pending) return;

  uint8_t const seq = payload[1];
  reader_t r = { payload + 2, len - 2, true };

  if (id == SYSEX_MSG_UPDATE_INFO) {
    handle_info(seq);
    return;
  }
  if (id == SYSEX_MSG_UPDATE_BEGIN) {
    handle_begin(seq, &r);
    return;
  }
  if (id == SYSEX_MSG_UPDATE_LEAVE) {
    handle_leave(seq);
    return;
  }

  uint16_t const sid = (uint16_t) rd(&r, 2);
  if (!r.ok || session.id == 0 || sid != session.id) {
    reply_status(id, seq, UPDATE_BAD_SESSION);
    return;
  }

  switch (id) {
    case SYSEX_MSG_UPDATE_WRITE:  handle_write(seq, &r); break;
    case SYSEX_MSG_UPDATE_CRC:    handle_crc(seq, &r); break;
    case SYSEX_MSG_UPDATE_ERASE:  handle_erase(seq, &r); break;
    case SYSEX_MSG_UPDATE_COMMIT: handle_commit(seq); break;
    default: break;
  }
}

void midi_update_task(void)
{
  uint8_t packet[4];
  while (tud_midi_available() && tud_midi_packet_read(packet)) {
    uint8_t const n = sysex_usb_packet_len(packet[0]);
    for (uint8_t i = 1; i <= n; i++) {
      size_t frame_len;
      if (sysex_rx_byte(&rx, packet[i], &frame_len) != SYSEX_RX_FRAME) continue;

      const uint8_t *payload;
      size_t payload_len;
      if (sysex_unpack(rx_buf, frame_len, decoded, sizeof(decoded), &payload, &payload_len) == NULL)
        dispatch(payload, payload_len);
    }
  }

  /* Leave the COMMIT reply time to reach the host before the reset. */
  if (reboot_pending && HAL_GetTick() - reboot_from >= 100U) NVIC_SystemReset();
}
