#!/usr/bin/env python3
"""Exercises the Bandolibre sysex protocol (code/main-g474/app/midi.cc,
usb/usb_app.cc) over raw USB and dumps every frame sent/received, decoded, to
stdout and to a log file.

This talks to the device's MIDIStreaming interface directly via libusb
(pyusb), implementing the USB-MIDI 4-byte event packing/unpacking ourselves
(the same Code-Index-Number scheme TinyUSB implements on the firmware side -
see midi_input_process() in usb_app.cc) instead of going through ALSA. That's
deliberate: this project has a reproducible kernel-level hang
(snd_use_lock_sync_helper / snd_seq_open, all in D-state, unkillable) in the
Linux ALSA sequencer core when any process opens or closes a MIDI sequencer
client for this device - a host kernel bug, not a firmware issue, but one
that's 100% reproducible and requires a reboot to clear. Going in over raw
USB never touches that subsystem at all.

One-time setup this needs (raw USB access to this device requires a udev
rule; without it, opening the device raises "Access denied"):
  echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="a5b4", MODE="0666"' \\
    | sudo tee /etc/udev/rules.d/99-bandolibre.rules
  sudo udevadm control --reload-rules
  sudo udevadm trigger

Every send and receive is logged immediately (flushed to disk) before moving
on, so if something still goes wrong partway through, the log already on
disk shows exactly which request was in flight and what, if anything, came
back before things went quiet.

Examples:
  sysex_dump.py                  # hello + dump every property
  sysex_dump.py --hello-only     # just the handshake, for quick checks
  sysex_dump.py --timeout 5      # more patient per-request timeout
"""

import argparse
import datetime
import pathlib
import re
import sys
import time

import usb.core
import usb.util

# get_property_description deliberately doesn't return the property's name
# over the wire (see midi.cc) - only its type and documentation. For display
# purposes we can still read it from the same source the firmware is built
# from, since this tool lives in the same checkout.
PROPERTY_TABLE_DEF = (pathlib.Path(__file__).resolve().parent
                      / "../../main-g474/properties/property_table.def")
PROPERTY_ENTRY_RE = re.compile(r"^PROPERTY\(\s*[^,]+,\s*[^,]+,\s*(\w+)\s*,", re.MULTILINE)


def load_property_names():
    """Returns property names in declaration order (index i is what the
    firmware's property_at(i) returns), or [] if the .def file can't be
    read - name display is a convenience, not a protocol requirement."""
    try:
        text = PROPERTY_TABLE_DEF.read_text()
    except OSError:
        return []
    return PROPERTY_ENTRY_RE.findall(text)

VENDOR_ID = 0x0483
PRODUCT_ID = 0xA5B4

USB_CLASS_AUDIO = 0x01
USB_SUBCLASS_MIDISTREAMING = 0x03

# USB-MIDI Code Index Number -> how many of the packet's next 3 bytes are
# real payload. Mirrors the switch in midi_input_process() in usb_app.cc.
CIN_VALID_BYTES = {0x4: 3, 0x5: 1, 0x6: 2, 0x7: 3}

SYSEX_START = 0xF0
SYSEX_END = 0xF7

# 7-bit-safe encoding for the checksum+payload region between SYSEX_START
# and SYSEX_END. Mirrors sysex_encode7()/sysex_decode7() in usb_app.cc - see
# the comment there: an earlier version escaped only the 3 literal byte
# values that collide with our own framing, which broke the moment a
# checksum/value byte with bit 7 set passed through any transport that
# reconstructs a standard MIDI byte stream (confirmed: the Linux kernel's
# USB-MIDI driver feeding ALSA/Web MIDI does this and misreads such a byte
# as a new status byte, even though this raw-USB path - which decodes
# USB-MIDI CIN-tagged packets directly and never reinterprets them as
# generic MIDI - was never affected).

MSG_HELLO = 0x00
MSG_GET_PROPERTY = 0x01
MSG_GET_PROPERTY_DESCRIPTION = 0x02
MSG_SET_PROPERTY = 0x03
MSG_GET_PERIPHERALS = 0x04

MESSAGE_NAMES = {
    MSG_HELLO: "HELLO",
    MSG_GET_PROPERTY: "GET_PROPERTY",
    MSG_GET_PROPERTY_DESCRIPTION: "GET_PROPERTY_DESCRIPTION",
    MSG_SET_PROPERTY: "SET_PROPERTY",
    MSG_GET_PERIPHERALS: "GET_PERIPHERALS",
}

PROPERTY_TYPE_NAMES = {0: "bool", 1: "u16"}


def pack_usb_midi(data: bytes, cable: int = 0) -> list:
    """Packs a complete byte stream (starting with 0xF0, ending with 0xF7)
    into 4-byte USB-MIDI event packets, using the same CIN scheme TinyUSB
    uses on the firmware side (tud_midi_n_stream_write in midi_device.c)."""
    packets = []
    i = 0
    n = len(data)
    while n - i > 3:
        packets.append(bytes([(cable << 4) | 0x4, *data[i:i + 3]]))
        i += 3
    remainder = data[i:]
    cin = {1: 0x5, 2: 0x6, 3: 0x7}[len(remainder)]
    padded = remainder + bytes(3 - len(remainder))
    packets.append(bytes([(cable << 4) | cin, *padded]))
    return packets


def checksum(data: bytes) -> int:
    """XOR-fold over 16-bit little-endian words, folding a lone trailing byte
    if len is odd. Must match sysex_checksum() in usb_app.cc."""
    cs = 0
    i = 0
    n = len(data)
    while i + 1 < n:
        cs ^= data[i] | (data[i + 1] << 8)
        i += 2
    if i < n:
        cs ^= data[i]
    return cs & 0xFFFF


def encode7(data: bytes) -> bytes:
    """Encodes data (the checksum + payload region) into 7-bit-safe groups:
    each run of up to 7 input bytes becomes 8 output bytes - a leading byte
    holding the high bit of each input byte (bit j = input byte j's bit 7),
    followed by those bytes with bit 7 cleared. Mirrors sysex_encode7() in
    usb_app.cc."""
    out = bytearray()
    for i in range(0, len(data), 7):
        group = data[i:i + 7]
        msb = 0
        for j, b in enumerate(group):
            if b & 0x80:
                msb |= 1 << j
        out.append(msb)
        out.extend(b & 0x7F for b in group)
    return bytes(out)


def decode7(data: bytes) -> bytes:
    """Reverses encode7(). Mirrors sysex_decode7() in usb_app.cc. Raises
    FrameError if data ends mid-group (truncated frame)."""
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        msb = data[i]
        i += 1
        group = data[i:i + 7]
        if not group and i < n:
            raise FrameError("truncated 7-bit group")
        for j, b in enumerate(group):
            if msb & (1 << j):
                b |= 0x80
            out.append(b)
        i += len(group)
    return bytes(out)


def build_frame(payload: bytes) -> list:
    cs = checksum(payload)
    body = encode7(bytes([cs & 0xFF, (cs >> 8) & 0xFF]) + payload)
    return [SYSEX_START, *body, SYSEX_END]


def hexdump(data) -> str:
    return " ".join(f"{b:02X}" for b in data)


class FrameError(Exception):
    pass


def unpack_frame(frame: bytes) -> bytes:
    """Mirrors unpack_sysex() in usb_app.cc. Raises FrameError with the exact
    reason instead of silently returning None, since this script's whole
    point is to surface what went wrong."""
    if len(frame) < 5:
        raise FrameError("frame too short")
    if frame[0] != SYSEX_START or frame[-1] != SYSEX_END:
        raise FrameError("missing 0xF0/0xF7 markers")
    body = decode7(frame[1:-1])
    if len(body) < 3:
        raise FrameError("frame too short")
    received_checksum = body[0] | (body[1] << 8)
    covered = body[2:]
    computed = checksum(covered)
    if computed != received_checksum:
        raise FrameError(f"checksum mismatch (got {received_checksum:04X}, want {computed:04X})")
    return covered


class DataReader:
    """Mirrors DataReader in midi.cc."""

    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def read_u8(self):
        if self.pos >= len(self.data):
            raise FrameError("truncated: expected a byte, ran out of data")
        b = self.data[self.pos]
        self.pos += 1
        return b

    def read_u16(self):
        return self.read_u8() | (self.read_u8() << 8)

    def read_string(self):
        start = self.pos
        while self.pos < len(self.data) and self.data[self.pos] != 0:
            self.pos += 1
        if self.pos >= len(self.data):
            raise FrameError("truncated: unterminated string")
        text = self.data[start:self.pos].decode("utf-8", errors="replace")
        self.pos += 1  # skip NUL
        return text


def decode_payload(payload: bytes) -> str:
    """Best-effort human description of a message-id + body payload, for
    logging. Never raises - falls back to a raw dump on anything unexpected,
    since a malformed payload is exactly the kind of thing we're here to see."""
    try:
        reader = DataReader(payload)
        message_id = reader.read_u8()
        name = MESSAGE_NAMES.get(message_id, f"UNKNOWN(0x{message_id:02X})")

        if message_id == MSG_HELLO:
            if reader.pos >= len(payload):
                return f"{name} (request, no body)"
            version = reader.read_string()
            count = reader.read_u16()
            left_wing_id = reader.read_u8()
            right_wing_id = reader.read_u8()
            return (f"{name} version={version!r} property_count={count} "
                    f"left_wing_id={left_wing_id} right_wing_id={right_wing_id}")

        if message_id in (MSG_GET_PROPERTY, MSG_SET_PROPERTY):
            index = reader.read_u16()
            if reader.pos >= len(payload):
                return f"{name} (request) index={index}"
            value = reader.read_u16()
            return f"{name} (response) index={index} value={value}"

        if message_id == MSG_GET_PROPERTY_DESCRIPTION:
            index = reader.read_u16()
            if reader.pos >= len(payload):
                return f"{name} (request) index={index}"
            ptype = reader.read_u8()
            description = reader.read_string()
            type_name = PROPERTY_TYPE_NAMES.get(ptype, f"0x{ptype:02X}")
            return f"{name} (response) index={index} type={type_name} description={description!r}"

        if message_id == MSG_GET_PERIPHERALS:
            if reader.pos >= len(payload):
                return f"{name} (request, no body)"
            hall0 = reader.read_u16()
            hall1 = reader.read_u16()
            pedal1_connected = reader.read_u8()
            pedal1_sample = reader.read_u16()
            pedal2_connected = reader.read_u8()
            pedal2_sample = reader.read_u16()
            return (f"{name} hall0={hall0} hall1={hall1} "
                    f"pedal1={'connected' if pedal1_connected else 'disconnected'}:{pedal1_sample} "
                    f"pedal2={'connected' if pedal2_connected else 'disconnected'}:{pedal2_sample}")

        return f"{name} body={hexdump(payload[1:])}"
    except FrameError as e:
        return f"<undecodable: {e}> raw={hexdump(payload)}"


class Logger:
    """Writes every line to stdout and to a log file, both flushed
    immediately - if the process later wedges in a blocking kernel read,
    nothing written so far is lost."""

    def __init__(self, path):
        self.path = path
        self.fh = open(path, "a", buffering=1)

    def log(self, message: str):
        line = f"[{time.monotonic():9.3f}] {message}"
        print(line, flush=True)
        self.fh.write(line + "\n")
        self.fh.flush()

    def close(self):
        self.fh.close()


def find_midistreaming_interface(dev):
    for cfg in dev:
        for intf in cfg:
            if intf.bInterfaceClass == USB_CLASS_AUDIO and \
               intf.bInterfaceSubClass == USB_SUBCLASS_MIDISTREAMING:
                return cfg, intf
    return None, None


class Bandolibre:
    def __init__(self, log: Logger):
        self.log = log

        dev = usb.core.find(idVendor=VENDOR_ID, idProduct=PRODUCT_ID)
        if dev is None:
            print(f"error: no USB device {VENDOR_ID:04x}:{PRODUCT_ID:04x} found.", file=sys.stderr)
            sys.exit(1)

        cfg, intf = find_midistreaming_interface(dev)
        if intf is None:
            print("error: device has no MIDIStreaming interface.", file=sys.stderr)
            sys.exit(1)
        self.iface_num = intf.bInterfaceNumber

        if dev.is_kernel_driver_active(self.iface_num):
            dev.detach_kernel_driver(self.iface_num)
            self.log.log(f"detached kernel driver from interface {self.iface_num}")

        usb.util.claim_interface(dev, self.iface_num)

        self.ep_out = usb.util.find_descriptor(
            intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress)
            == usb.util.ENDPOINT_OUT)
        self.ep_in = usb.util.find_descriptor(
            intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress)
            == usb.util.ENDPOINT_IN)
        if self.ep_out is None or self.ep_in is None:
            print("error: MIDIStreaming interface is missing a bulk endpoint.", file=sys.stderr)
            sys.exit(1)

        self.dev = dev
        self.log.log(f"connected: {dev.manufacturer!r} {dev.product!r} "
                      f"(interface {self.iface_num}, OUT=0x{self.ep_out.bEndpointAddress:02x}, "
                      f"IN=0x{self.ep_in.bEndpointAddress:02x})")

        self._flush_input()

    def _flush_input(self):
        """Drains and discards whatever is already sitting in the device's IN
        endpoint buffer (e.g. Active Sensing backlogged while nothing was
        polling this endpoint since boot). TinyUSB's MIDI TX ring buffer is
        small and, once full, appears to silently drop further writes rather
        than block - so a real response can be lost if it's queued while the
        buffer is still full of stale traffic. Draining before the first
        request gives the device room to actually send its reply."""
        discarded = 0
        while True:
            try:
                data = self.ep_in.read(64, timeout=50)
                discarded += len(data)
            except usb.core.USBError:
                break
        if discarded:
            self.log.log(f"   flushed {discarded} stale bytes from IN endpoint before first request")

    def close(self):
        """Releases the interface and hands it back to the kernel driver, so
        the device goes back to normal (ALSA-visible) afterward. This is a
        USB driver-binding operation, not an ALSA sequencer client
        open/close, so it does not touch the code path that hangs."""
        usb.util.release_interface(self.dev, self.iface_num)
        try:
            self.dev.attach_kernel_driver(self.iface_num)
        except usb.core.USBError as e:
            self.log.log(f"warning: could not reattach kernel driver: {e}")

    def transact(self, payload: bytes, timeout: float) -> bytes:
        """Sends one sysex request and waits for the matching response frame,
        logging both directions in full regardless of outcome."""
        frame = bytes(build_frame(payload))
        self.log.log(f"-> {hexdump(frame)}  [{decode_payload(bytes(payload))}]")
        for packet in pack_usb_midi(frame):
            n = self.ep_out.write(packet, timeout=int(timeout * 1000))
            self.log.log(f"   wrote {n} bytes: {hexdump(packet)}")

        deadline = time.monotonic() + timeout
        buf = []
        in_sysex = False
        while time.monotonic() < deadline:
            remaining_ms = max(1, int((deadline - time.monotonic()) * 1000))
            try:
                data = self.ep_in.read(64, timeout=min(remaining_ms, 200))
                self.log.log(f"   read {len(data)} bytes: {hexdump(data)}")
            except usb.core.USBError as e:
                is_timeout = e.errno == 110 or "timeout" in str(e).lower() or "timed out" in str(e).lower()
                self.log.log(f"   read error (errno={e.errno}): {e}"
                             f"{' [treating as timeout, retrying]' if is_timeout else ' [FATAL]'}")
                if is_timeout:
                    continue
                raise

            for i in range(0, len(data) - 3, 4):
                packet = data[i:i + 4]
                valid = CIN_VALID_BYTES.get(packet[0] & 0x0F)
                if valid is None:
                    continue
                for b in packet[1:1 + valid]:
                    if b == SYSEX_START:
                        in_sysex = True
                        buf = [b]
                    elif in_sysex:
                        buf.append(b)
                        if b == SYSEX_END:
                            raw = bytes(buf)
                            in_sysex = False
                            buf = []
                            try:
                                response = unpack_frame(raw)
                            except FrameError as e:
                                self.log.log(f"<- {hexdump(raw)}  [DISCARDED: {e}]")
                                continue  # keep waiting; maybe the real reply is still coming
                            self.log.log(f"<- {hexdump(raw)}  [{decode_payload(response)}]")
                            return response

        self.log.log(f"<- TIMEOUT after {timeout}s waiting for a response to the request above")
        raise TimeoutError(f"no response within {timeout}s")

    def hello(self, timeout: float):
        """Returns (version, property_count, left_wing_id, right_wing_id).
        A wing id of 0 means that side hasn't sent a good frame yet (e.g. not
        connected)."""
        response = self.transact(bytes([MSG_HELLO]), timeout)
        reader = DataReader(response)
        reader.read_u8()  # message id
        version = reader.read_string()
        count = reader.read_u16()
        left_wing_id = reader.read_u8()
        right_wing_id = reader.read_u8()
        return version, count, left_wing_id, right_wing_id

    def get_property_description(self, index: int, timeout: float):
        payload = bytes([MSG_GET_PROPERTY_DESCRIPTION]) + index.to_bytes(2, "little")
        response = self.transact(payload, timeout)
        reader = DataReader(response)
        reader.read_u8()   # message id
        reader.read_u16()  # echoed index
        ptype = reader.read_u8()
        description = reader.read_string()
        return ptype, description

    def get_property_value(self, index: int, timeout: float) -> int:
        payload = bytes([MSG_GET_PROPERTY]) + index.to_bytes(2, "little")
        response = self.transact(payload, timeout)
        reader = DataReader(response)
        reader.read_u8()   # message id
        reader.read_u16()  # echoed index
        return reader.read_u16()

    def get_peripherals(self, timeout: float):
        """Returns (hall0, hall1, pedal1_connected, pedal1_sample,
        pedal2_connected, pedal2_sample): the bellows' two raw hall ADC
        readings and both pedals' wiper ADC readings + presence flags."""
        response = self.transact(bytes([MSG_GET_PERIPHERALS]), timeout)
        reader = DataReader(response)
        reader.read_u8()  # message id
        hall0 = reader.read_u16()
        hall1 = reader.read_u16()
        pedal1_connected = bool(reader.read_u8())
        pedal1_sample = reader.read_u16()
        pedal2_connected = bool(reader.read_u8())
        pedal2_sample = reader.read_u16()
        return hall0, hall1, pedal1_connected, pedal1_sample, pedal2_connected, pedal2_sample


def format_value(ptype: int, value: int) -> str:
    if ptype == 0:  # PROPERTY_TYPE_BOOL
        return "on" if value else "off"
    return str(value)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--timeout", type=float, default=2.0,
                        help="seconds to wait for each response (default 2.0)")
    parser.add_argument("--hello-only", action="store_true",
                        help="only do the hello handshake, skip property enumeration")
    parser.add_argument("--log-file", default=None,
                        help="path to write the log to (default: sysex_dump_<timestamp>.log)")
    args = parser.parse_args()

    log_path = args.log_file or datetime.datetime.now().strftime("sysex_dump_%Y%m%d_%H%M%S.log")
    log = Logger(log_path)
    log.log(f"logging to {log_path}")

    names = load_property_names()

    board = Bandolibre(log)
    try:
        version, count, left_wing_id, right_wing_id = board.hello(args.timeout)
        log.log(f"hello OK: {version}, {count} properties, "
                f"left_wing_id={left_wing_id} right_wing_id={right_wing_id}")

        if args.hello_only:
            return

        rows = []
        for index in range(count):
            name = names[index] if index < len(names) else "?"
            try:
                ptype, description = board.get_property_description(index, args.timeout)
                value = board.get_property_value(index, args.timeout)
                rows.append((index, name, format_value(ptype, value), description))
            except (TimeoutError, FrameError) as e:
                log.log(f"property {index} ({name}): FAILED ({e}) - continuing with the next one")

        log.log("")
        log.log("=== summary ===")
        name_width = max((len(n) for _, n, _, _ in rows), default=4)
        value_width = max((len(v) for _, _, v, _ in rows), default=5)
        for index, name, value, description in rows:
            log.log(f"{index:3d}  {name:<{name_width}}  {value:<{value_width}}  {description}")
        if len(rows) != count:
            log.log(f"({count - len(rows)} of {count} properties failed - see above)")

        log.log("")
        try:
            hall0, hall1, p1_conn, p1_val, p2_conn, p2_val = board.get_peripherals(args.timeout)
            log.log(f"peripherals: hall0={hall0} hall1={hall1}  "
                    f"pedal1={'connected' if p1_conn else 'disconnected'}:{p1_val}  "
                    f"pedal2={'connected' if p2_conn else 'disconnected'}:{p2_val}")
        except (TimeoutError, FrameError) as e:
            log.log(f"peripherals: FAILED ({e})")
    finally:
        log.log("closing (releasing USB interface, reattaching kernel driver)")
        board.close()
        log.close()


if __name__ == "__main__":
    main()
