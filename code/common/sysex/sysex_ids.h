#ifndef COMMON_SYSEX_IDS_H
#define COMMON_SYSEX_IDS_H

/* Message ids of the Bandolibre SysEx protocol (framing in sysex_codec.h),
 * for both the application (main-g474/app/midi.cc) and the bootloader's
 * firmware update mode (boot-g474/midi_update.c). The web tools
 * (docs/connect/index.html, docs/update/index.html, bandopm/docs/app.js)
 * repeat these numbers; change them together.
 *
 * Each side answers only the ids it knows and silently ignores the others: the
 * bootloader never answers the configuration tool's requests, and the
 * application never answers the update commands (except ENTER_UPDATE_MODE).
 * A client therefore tells the two apart by HELLO's `mode` byte. */

#ifdef __cplusplus
extern "C" {
#endif

enum sysex_message_id {
  /* Both modes. Reply: [id, string version, u8 mode, u16 store generation]
   * then, in instrument mode only, [u16 property count, u8 wing left,
   * u8 wing right, u8 bellows direction]. */
  SYSEX_MSG_HELLO                    = 0x00,

  /* Instrument mode (application) only. */
  SYSEX_MSG_GET_PROPERTY             = 0x01,
  SYSEX_MSG_GET_PROPERTY_DESCRIPTION = 0x02,
  SYSEX_MSG_SET_PROPERTY             = 0x03,
  /* The tool's live poll: answers with the live peripheral state, and renews
   * the monitor lease that gates the push notifications (midi.cc,
   * monitor_leased). */
  SYSEX_MSG_MONITOR_LEASE            = 0x04,
  /* Device -> host push: the effective bellows direction changed. */
  SYSEX_MSG_BELLOWS_DIRECTION        = 0x05,
  /* Saved values (flash, property_persist.cc): read or write the value a
   * property boots with, and forget them all. */
  SYSEX_MSG_GET_SAVED_VALUE          = 0x06,
  SYSEX_MSG_SET_SAVED_VALUE          = 0x07,
  SYSEX_MSG_FACTORY_RESET            = 0x08,
  /* Device -> host push: a property the device changed and saved on its own. */
  SYSEX_MSG_PROPERTY_CHANGED         = 0x09,
  /* Device -> host push: one per NOTE ON, with the key strike behind it. */
  SYSEX_MSG_KEY_STRIKE               = 0x0A,
  /* Device -> host push: one per NOTE OFF of a key, with its release. */
  SYSEX_MSG_KEY_RELEASE              = 0x0B,
  /* A synth asking for key-speed velocities (bandopm's attack & release
   * mode): renews the velocity lease. No reply. */
  SYSEX_MSG_VELOCITY_LEASE           = 0x0C,

  /* Firmware update. Every request is [id, u8 seq, ...] and every reply
   * [id, u8 seq, u8 status, ...], the seq echoed so a client sharing the port
   * with another never takes that one's reply for its own. The session guards
   * against accidents, not against a hostile client: UPDATE_INFO tells anyone
   * the open one. */

  /* Application: [magic "UPDT"] -> reply, then reboot into the bootloader in
   * MIDI-only update mode. */
  SYSEX_MSG_ENTER_UPDATE_MODE        = 0x20,
  /* Bootloader, the capability query: -> [u16 page size, u32 app base,
   * u32 app size, u16 max chunk, u8 app state, string installed app version,
   * u16 open session or 0, u8 drive: 1 if the BANDOLIBRE drive is also
   * exposed]. */
  SYSEX_MSG_UPDATE_INFO              = 0x21,
  /* [u32 size, u32 crc32] -> [u16 session]. Erases page 0 first: from here
   * until UPDATE_COMMIT the board boots into update mode. */
  SYSEX_MSG_UPDATE_BEGIN             = 0x22,
  /* [u16 session, u32 offset, data...] -> [u32 offset]. Idempotent. */
  SYSEX_MSG_UPDATE_WRITE             = 0x23,
  /* [u16 session, u32 offset, u32 len] -> [u32 crc32] of what is in flash. */
  SYSEX_MSG_UPDATE_CRC               = 0x24,
  /* [u16 session, u32 page offset] -> []. To rewrite a page that failed. */
  SYSEX_MSG_UPDATE_ERASE             = 0x25,
  /* [u16 session] -> [], then reboot into the new application. */
  SYSEX_MSG_UPDATE_COMMIT            = 0x26,
  /* [] -> [], then reboot into the installed application. Refused (BAD_ARGS)
   * when it is not valid, e.g. once BEGIN has erased its first page. */
  SYSEX_MSG_UPDATE_LEAVE             = 0x27,
};

/* HELLO's mode byte. */
enum sysex_mode {
  SYSEX_MODE_INSTRUMENT = 0,
  SYSEX_MODE_UPDATE     = 1,
};

/* Status byte of the update replies. */
enum sysex_update_status {
  UPDATE_OK            = 0,
  UPDATE_BAD_SESSION   = 1,  /* no session, or another client's BEGIN replaced it */
  UPDATE_BAD_ARGS      = 2,
  UPDATE_FLASH_ERROR   = 3,
  UPDATE_CRC_MISMATCH  = 4,
  UPDATE_BUSY          = 5,  /* a .uf2 is being copied onto the drive */
};

/* UPDATE_INFO's app state. */
enum sysex_app_state {
  APP_STATE_VALID       = 0,
  APP_STATE_INVALID     = 1,  /* empty, or not a plausible image */
  APP_STATE_INTERRUPTED = 2,  /* an update began and did not commit */
};

/* ENTER_UPDATE_MODE's body, so a stray frame can't reboot the instrument. */
#define SYSEX_UPDATE_MAGIC "UPDT"

#ifdef __cplusplus
}
#endif

#endif /* COMMON_SYSEX_IDS_H */
