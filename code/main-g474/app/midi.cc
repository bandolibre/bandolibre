#include "midi.h"
#include "main.h"        /* HAL_GetTick */
#include "properties.h"
#include "usb_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <array>
#include <gsl/span>

namespace {

/* Placeholder until firmware builds carry a real version string. */
constexpr const char *FIRMWARE_VERSION_STRING = "Bandolibre v0.0.1";

enum sysex_message_id : uint8_t {
  SYSEX_MSG_HELLO                    = 0x00,
  SYSEX_MSG_GET_PROPERTY             = 0x01,
  SYSEX_MSG_GET_PROPERTY_DESCRIPTION = 0x02,
  SYSEX_MSG_SET_PROPERTY             = 0x03,
};

/* Reads sysex message fields off the front of a span, advancing an internal
 * cursor. Each read returns false (leaving the cursor at the short read) if
 * not enough bytes remain. */
class DataReader {
 public:
  explicit DataReader(gsl::span<const uint8_t> data) : data_(data) {}

  bool readUInt8(uint8_t *out)
  {
    if (pos_ >= data_.size()) return false;
    *out = data_[pos_++];
    return true;
  }

  bool readUInt16(uint16_t *out)
  {
    uint8_t lo, hi;
    if (!readUInt8(&lo) || !readUInt8(&hi)) return false;
    *out = (uint16_t)(lo | (uint16_t)(hi << 8));
    return true;
  }

  /* Reads a NUL-terminated string, returning it (without the NUL) as a span
   * into the original buffer. */
  bool readString(gsl::span<const char> *out)
  {
    size_t start = pos_;
    while (pos_ < data_.size() && data_[pos_] != '\0') pos_++;
    if (pos_ >= data_.size()) return false;
    *out = gsl::span<const char>(reinterpret_cast<const char *>(data_.data() + start), pos_ - start);
    pos_++;
    return true;
  }

  size_t position() const { return pos_; }

 private:
  gsl::span<const uint8_t> data_;
  size_t pos_ = 0;
};

/* Writes sysex message fields into a fixed buffer, advancing an internal
 * cursor. getSpan() returns the portion written so far, ready to hand to
 * usb_app_midi_send_sysex. */
class DataWriter {
 public:
  explicit DataWriter(gsl::span<uint8_t> buf) : buf_(buf) {}

  bool write(uint8_t v)
  {
    if (pos_ >= buf_.size()) return false;
    buf_[pos_++] = v;
    return true;
  }

  bool write(uint16_t v)
  {
    return write((uint8_t)(v & 0xff)) && write((uint8_t)(v >> 8));
  }

  /* Writes the string's bytes followed by a NUL, the counterpart to
   * DataReader::readString. */
  bool write(gsl::span<const char> s)
  {
    for (char c : s)
      if (!write((uint8_t)c)) return false;
    return write((uint8_t)'\0');
  }

  size_t position() const { return pos_; }

  gsl::span<const uint8_t> getSpan() const { return buf_.first(pos_); }

 private:
  gsl::span<uint8_t> buf_;
  size_t pos_ = 0;
};

void send_hello_response()
{
  std::array<uint8_t, 128> payload;
  DataWriter writer(payload);

  writer.write((uint8_t)SYSEX_MSG_HELLO);
  writer.write(gsl::span<const char>(FIRMWARE_VERSION_STRING, strlen(FIRMWARE_VERSION_STRING)));
  writer.write((uint16_t)property_count());

  gsl::span<const uint8_t> body = writer.getSpan();
  usb_app_midi_send_sysex(body.data(), body.size());
}

/* Reads a property's current value regardless of its underlying type (bool
 * reads back as 0/1). False if index is out of range. */
bool get_property_raw(size_t index, uint16_t *out)
{
  const property_desc_t *d = property_at(index);
  if (!d) return false;
  if (d->type == PROPERTY_TYPE_BOOL) {
    bool b;
    if (!property_get_bool(index, &b)) return false;
    *out = b ? 1 : 0;
    return true;
  }
  return property_get_u16(index, out);
}

/* Writes a property's value regardless of its underlying type (bool takes
 * value != 0); property_set_u16 clamps to [min,max]. False if index is out
 * of range. */
bool set_property_raw(size_t index, uint16_t value)
{
  const property_desc_t *d = property_at(index);
  if (!d) return false;
  if (d->type == PROPERTY_TYPE_BOOL) return property_set_bool(index, value != 0);
  return property_set_u16(index, value);
}

void send_index_value_response(sysex_message_id message_id, uint16_t index, uint16_t value)
{
  std::array<uint8_t, 8> payload;
  DataWriter writer(payload);

  writer.write((uint8_t)message_id);
  writer.write(index);
  writer.write(value);

  gsl::span<const uint8_t> body = writer.getSpan();
  usb_app_midi_send_sysex(body.data(), body.size());
}

void handle_get_property(gsl::span<const uint8_t> body)
{
  DataReader reader(body);
  uint16_t index;
  if (!reader.readUInt16(&index)) {
    printf("sysex: get_property: malformed request\r\n");
    return;
  }

  uint16_t value;
  if (!get_property_raw(index, &value)) {
    printf("sysex: get_property: bad index %u\r\n", index);
    return;
  }

  send_index_value_response(SYSEX_MSG_GET_PROPERTY, index, value);
}

void handle_get_property_description(gsl::span<const uint8_t> body)
{
  DataReader reader(body);
  uint16_t index;
  if (!reader.readUInt16(&index)) {
    printf("sysex: get_property_description: malformed request\r\n");
    return;
  }

  const property_desc_t *d = property_at(index);
  if (!d) {
    printf("sysex: get_property_description: bad index %u\r\n", index);
    return;
  }

  std::array<uint8_t, 160> payload;
  DataWriter writer(payload);

  writer.write((uint8_t)SYSEX_MSG_GET_PROPERTY_DESCRIPTION);
  writer.write(index);
  writer.write((uint8_t)d->type);
  writer.write(gsl::span<const char>(d->description, strlen(d->description)));

  gsl::span<const uint8_t> response_body = writer.getSpan();
  usb_app_midi_send_sysex(response_body.data(), response_body.size());
}

void handle_set_property(gsl::span<const uint8_t> body)
{
  DataReader reader(body);
  uint16_t index, value;
  if (!reader.readUInt16(&index) || !reader.readUInt16(&value)) {
    printf("sysex: set_property: malformed request\r\n");
    return;
  }

  if (!set_property_raw(index, value)) {
    printf("sysex: set_property: bad index %u\r\n", index);
    return;
  }

  /* Echo back the value actually in effect: property_set_u16 clamps to
   * [min,max], so it may differ from what was requested. */
  uint16_t new_value;
  get_property_raw(index, &new_value);
  send_index_value_response(SYSEX_MSG_SET_PROPERTY, index, new_value);
}

}  /* namespace */

void midi_sysex_received(gsl::span<const uint8_t> data)
{
  // `data` is the message identifier and body: usb_app.cc has already
  // stripped and checksum-verified the 0xF0/checksum/0xF7 framing (dumping
  // the raw frame and logging any discard along the way) before calling this.
  if (data.empty()) {
    printf("sysex: empty message\r\n");
    return;
  }

  uint8_t message_id = data[0];
  gsl::span<const uint8_t> body = data.subspan(1);
  switch (message_id) {
    case SYSEX_MSG_HELLO:
      send_hello_response();
      break;
    case SYSEX_MSG_GET_PROPERTY:
      handle_get_property(body);
      break;
    case SYSEX_MSG_GET_PROPERTY_DESCRIPTION:
      handle_get_property_description(body);
      break;
    case SYSEX_MSG_SET_PROPERTY:
      handle_set_property(body);
      break;
    default:
      printf("sysex: unknown message id %u\r\n", message_id);
      break;
  }
}

namespace {

bool parse_arg_impl(gsl::span<const char* const> argv, size_t idx,
                    long lo, long hi, const char *what, long *out)
{
  if (idx >= argv.size()) { printf("missing %s\r\n", what); return false; }
  char *end;
  long v = strtol(argv[idx], &end, 0);
  if (argv[idx][0] == '\0' || *end != '\0') { printf("not a number: %s\r\n", argv[idx]); return false; }
  if (v < lo || v > hi) { printf("%s out of range [%ld,%ld]: %ld\r\n", what, lo, hi, v); return false; }
  *out = v;
  return true;
}

static void cmd_send_sysex(gsl::span<const char* const> argv)
{
  if (argv.size() < 2) {
    printf("usage: send_sysex <byte0> [byte1] ...\r\n");
    printf("  sends sysex message id + body bytes (in hex, decimal, or 0x prefix);\r\n");
    printf("  the 0xF0/checksum header and 0xF7 footer are added automatically\r\n");
    printf("  example: send_sysex 0x00\r\n");
    return;
  }

  std::array<uint8_t, 256> msg;
  size_t len = 0;

  for (size_t i = 1; i < argv.size() && len < msg.size(); i++) {
    char *end;
    long val = strtol(argv[i], &end, 0);
    if (argv[i][0] == '\0' || *end != '\0') {
      printf("not a number: %s\r\n", argv[i]);
      return;
    }
    if (val < 0 || val > 255) {
      printf("byte out of range [0,255]: %ld\r\n", val);
      return;
    }
    msg[len++] = (uint8_t)val;
  }

  usb_app_midi_send_sysex(msg.data(), len);
  printf("sysex sent: ");
  for (size_t i = 0; i < len; i++) printf("%02X ", msg[i]);
  printf("\r\n");
}

}  /* namespace */


void midi_poll(void)
{
  static uint32_t last_tick = 0;
  if (!g_properties->midi_active_sensing_enable) return;
  uint32_t now = HAL_GetTick();
  if ((now - last_tick) < g_properties->midi_active_sensing_period) return;
  last_tick = now;
  usb_app_midi_active_sensing();
}

constexpr uint8_t MIDI_DEFAULT_VELOCITY = 64;
static void cmd_note_on(gsl::span<const char* const> argv_span)
{
  long channel, note, velocity = MIDI_DEFAULT_VELOCITY;
  if (!parse_arg_impl(argv_span, 1, 1, 16, "channel", &channel) ||
      !parse_arg_impl(argv_span, 2, 0, 127, "note", &note))
  {
    printf("usage: send_note_on <channel 1-16> <note 0-127> [velocity 0-127]\r\n");
    return;
  }
  if (argv_span.size() > 3 && !parse_arg_impl(argv_span, 3, 0, 127, "velocity", &velocity)) return;
  usb_app_midi_note_on((uint8_t)(channel - 1), (uint8_t)note, (uint8_t)velocity);
  printf("note on  ch %ld note %ld vel %ld\r\n", channel, note, velocity);
}

static void cmd_note_off(gsl::span<const char* const> argv_span)
{
  long channel, note;
  if (!parse_arg_impl(argv_span, 1, 1, 16, "channel", &channel) ||
      !parse_arg_impl(argv_span, 2, 0, 127, "note", &note))
  {
    printf("usage: send_note_off <channel 1-16> <note 0-127>\r\n");
    return;
  }
  usb_app_midi_note_off((uint8_t)(channel - 1), (uint8_t)note);
  printf("note off ch %ld note %ld\r\n", channel, note);
}

static void cmd_cc(gsl::span<const char* const> argv_span)
{
  long channel, controller, value;
  if (!parse_arg_impl(argv_span, 1, 1, 16, "channel", &channel) ||
      !parse_arg_impl(argv_span, 2, 0, 127, "controller", &controller) ||
      !parse_arg_impl(argv_span, 3, 0, 127, "value", &value))
  {
    printf("usage: send_cc <channel 1-16> <controller 0-127> <value 0-127>\r\n");
    return;
  }
  usb_app_midi_control_change((uint8_t)(channel - 1), (uint8_t)controller, (uint8_t)value);
  printf("cc       ch %ld ctrl %ld val %ld\r\n", channel, controller, value);
}

bool midi_console_execute(gsl::span<const char* const> argv_span)
{
  if (argv_span.empty()) return false;
  if (strcmp(argv_span[0], "send_note_on") == 0)       cmd_note_on(argv_span);
  else if (strcmp(argv_span[0], "send_note_off") == 0) cmd_note_off(argv_span);
  else if (strcmp(argv_span[0], "send_cc") == 0)       cmd_cc(argv_span);
  else if (strcmp(argv_span[0], "send_sysex") == 0)    cmd_send_sysex(argv_span);
  else return false;
  return true;
}

void midi_console_help(void)
{
  printf("\r\nMIDI commands (cable 0, channel 1-16, data 0-127):\r\n");
  printf("  send_note_on  <channel> <note> [velocity]   velocity defaults to %d\r\n", MIDI_DEFAULT_VELOCITY);
  printf("  send_note_off <channel> <note>\r\n");
  printf("  send_cc       <channel> <controller> <value>\r\n");
  printf("  send_sysex    <byte0> [byte1] ...           sends id+body (hex/decimal); framing is automatic\r\n");
}

size_t midi_console_complete(const char *prefix, const char **out, size_t cap)
{
  static const char *const names[] = { "send_note_on", "send_note_off", "send_cc", "send_sysex" };
  size_t n = 0;
  size_t plen = prefix ? strlen(prefix) : 0;
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]) && n < cap; i++)
    if (strncmp(names[i], prefix ? prefix : "", plen) == 0) out[n++] = names[i];
  return n;
}
