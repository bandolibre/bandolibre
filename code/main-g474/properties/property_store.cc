#include "property_store.h"

#include <string.h>

volatile bool property_store_ecc_armed = false;
volatile bool property_store_ecc_fault = false;

namespace {

constexpr uint32_t kMagic = 0x53504C42u;  /* "BLPS" little-endian */
constexpr uint64_t kErased = ~uint64_t{0};
constexpr uint8_t kPad = 0xFF;

/* The header shares the record slot format: one double word. */
struct alignas(8) StoreHeader {
  uint32_t magic;
  uint16_t generation;
  uint16_t crc;
};
static_assert(sizeof(StoreHeader) == sizeof(uint64_t), "the header is one flash double word");

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over the first 6 bytes. */
uint16_t crc16(const void *data)
{
  const uint8_t *b = static_cast<const uint8_t *>(data);
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < 6; i++) {
    crc ^= static_cast<uint16_t>(b[i] << 8);
    for (int bit = 0; bit < 8; bit++)
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

template <typename T>
uint64_t to_dw(const T &v)
{
  uint64_t dw;
  memcpy(&dw, &v, sizeof(dw));
  return dw;
}

/* Serial-number comparison: a is newer than b, across wrap-around. */
bool newer(uint16_t a, uint16_t b)
{
  return static_cast<int16_t>(static_cast<uint16_t>(a - b)) > 0;
}

/* Copy one slot out of flash; false if the read hit a double ECC error. */
bool read_dw(const void *src, void *dst)
{
  property_store_ecc_armed = true;
  memcpy(dst, src, sizeof(uint64_t));
  property_store_ecc_armed = false;
  bool const fault = property_store_ecc_fault;
  property_store_ecc_fault = false;
  return !fault;
}

}  /* namespace */

/* ---- StoreRecord -----------------------------------------------------------*/

StoreRecord StoreRecord::make(uint16_t tag, StoreKind kind, uint16_t value)
{
  StoreRecord r{};
  r.tag = tag;
  r.value = value;
  r.kind = static_cast<uint8_t>(kind);
  r.pad = kPad;
  r.crc = crc16(&r);
  return r;
}

bool StoreRecord::is_valid() const
{
  StoreRecord r;
  if (!read_dw(this, &r)) return false;
  if (to_dw(r) == kErased) return false;
  if (r.kind != static_cast<uint8_t>(StoreKind::Set) && r.kind != static_cast<uint8_t>(StoreKind::Clear))
    return false;
  return r.pad == kPad && r.crc == crc16(&r);
}

/* ---- PropertyStore ---------------------------------------------------------*/

PropertyStore::PropertyStore(Flash &flash, gsl::span<const uint16_t> known_tags)
    : flash_(flash), known_tags_(known_tags)
{
}

bool PropertyStore::read_slot(int p, size_t slot, uint64_t *out) const
{
  return read_dw(&flash_.partition(p)[slot], out);
}

bool PropertyStore::header_valid(int p, uint16_t *generation) const
{
  uint64_t dw;
  if (!read_slot(p, 0, &dw)) return false;
  StoreHeader h;
  memcpy(&h, &dw, sizeof(h));
  if (h.magic != kMagic || h.generation == 0 || h.crc != crc16(&h)) return false;
  *generation = h.generation;
  return true;
}

bool PropertyStore::blank(int p) const
{
  for (size_t i = 0; i < slots(); i++) {
    uint64_t dw;
    if (!read_slot(p, i, &dw) || dw != kErased) return false;
  }
  return true;
}

bool PropertyStore::ensure_blank(int p)
{
  return blank(p) || flash_.erase(p);
}

/* Program and read back. A failed slot is still spent: it is no longer erased. */
bool PropertyStore::write_slot(int p, size_t slot, uint64_t dw)
{
  if (!flash_.program(p, slot, dw)) return false;
  uint64_t check;
  return read_slot(p, slot, &check) && check == dw;
}

bool PropertyStore::write_header(int p, uint16_t generation)
{
  StoreHeader h{};
  h.magic = kMagic;
  h.generation = generation;
  h.crc = crc16(&h);
  return write_slot(p, 0, to_dw(h));
}

void PropertyStore::init()
{
  active_ = -1;
  generation_ = 0;
  next_slot_ = 1;
  repaired_ = 0;

  size_t const bytes = slots() * sizeof(uint64_t);
  enabled_ = flash_.page_size() == bytes && flash_.partition(1).size() == slots() && slots() >= 2;
  if (!enabled_) return;

  uint16_t gen[Flash::kPartitions] = {0, 0};
  bool committed[Flash::kPartitions];
  for (int p = 0; p < Flash::kPartitions; p++) committed[p] = header_valid(p, &gen[p]);

  if (committed[0] && committed[1]) {
    /* Power was lost after a compaction committed, before it erased the old
     * partition: the newer generation is the complete copy. */
    active_ = newer(gen[1], gen[0]) ? 1 : 0;
  } else if (committed[0] || committed[1]) {
    active_ = committed[0] ? 0 : 1;
  }

  for (int p = 0; p < Flash::kPartitions; p++) {
    if (p == active_ || blank(p)) continue;
    /* Stale (older generation) or incomplete (no valid header). */
    repaired_ |= static_cast<uint8_t>(1u << p);
    flash_.erase(p);
  }

  if (active_ < 0) return;
  generation_ = gen[active_];

  /* Records are appended in order, so the write position follows the last
   * slot that is not erased. */
  next_slot_ = slots();
  while (next_slot_ > 1) {
    uint64_t dw;
    if (read_slot(active_, next_slot_ - 1, &dw) && dw == kErased) next_slot_--;
    else break;
  }
}

gsl::span<const StoreRecord> PropertyStore::records() const
{
  if (!enabled_ || active_ < 0) return {};
  gsl::span<const uint64_t> used = flash_.partition(active_).subspan(1, next_slot_ - 1);
  return gsl::span<const StoreRecord>(reinterpret_cast<const StoreRecord *>(used.data()), used.size());
}

PropertyStore::Result PropertyStore::compact()
{
  int const from = active_;
  int const to = 1 - from;
  if (!ensure_blank(to)) return Result::FlashError;

  /* The newest valid record of each known tag; only saved values (SET) carry
   * over, so CLEARs and the tags of removed properties drop out here. */
  gsl::span<const StoreRecord> const old = records();
  size_t slot = 1;
  for (uint16_t tag : known_tags_) {
    for (size_t i = old.size(); i-- > 0;) {
      if (!old[i].is_valid() || old[i].tag != tag) continue;
      if (old[i].record_kind() == StoreKind::Set) {
        if (slot >= slots() || !write_slot(to, slot, to_dw(old[i]))) return Result::FlashError;
        slot++;
      }
      break;
    }
  }

  uint16_t gen = static_cast<uint16_t>(generation_ + 1);
  if (gen == 0) gen = 1;
  if (!write_header(to, gen)) return Result::FlashError;

  /* Committed: from here the new partition wins even if this erase fails
   * (init() erases the older generation). */
  active_ = to;
  generation_ = gen;
  next_slot_ = slot;
  flash_.erase(from);
  return Result::Ok;
}

PropertyStore::Result PropertyStore::append(const StoreRecord &r)
{
  if (!enabled_) return Result::Disabled;

  if (active_ < 0) {
    /* First save ever, or the first since a factory reset. */
    if (!ensure_blank(0) || !write_header(0, 1)) return Result::FlashError;
    active_ = 0;
    generation_ = 1;
    next_slot_ = 1;
  }

  /* A slot that fails to program is spent; try the next one. */
  for (int attempt = 0; attempt < 3; attempt++) {
    if (next_slot_ >= slots()) {
      Result const res = compact();
      if (res != Result::Ok) return res;
      if (next_slot_ >= slots()) return Result::FlashError;
    }
    bool const ok = write_slot(active_, next_slot_, to_dw(r));
    next_slot_++;
    if (ok) return Result::Ok;
  }
  return Result::FlashError;
}

PropertyStore::Result PropertyStore::drop()
{
  if (!enabled_) return Result::Disabled;
  bool ok = true;
  for (int p = 0; p < Flash::kPartitions; p++) ok = ensure_blank(p) && ok;
  active_ = -1;
  generation_ = 0;
  next_slot_ = 1;
  return ok ? Result::Ok : Result::FlashError;
}

PropertyStore::Status PropertyStore::status() const
{
  Status s{};
  s.enabled = enabled_;
  s.partition = active_;
  s.generation = generation_;
  s.used = static_cast<uint16_t>(active_ < 0 ? 0 : next_slot_ - 1);
  s.capacity = static_cast<uint16_t>(slots() - 1);
  s.repaired = repaired_;
  return s;
}
