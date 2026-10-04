#ifndef PROPERTY_STORE_H_
#define PROPERTY_STORE_H_

/* Saved property values in flash: an append-only log over two partitions.
 *
 * Every persistent property has a factory value (the firmware's
 * .default_value in property_table.def) and optionally a saved value, which
 * overrides it at boot. Saving a value appends one record to the active
 * partition; the last record of a tag wins. When the active partition is full,
 * the newest saved value of every known tag is copied to the other partition,
 * which then becomes active, and the full one is erased. The other partition
 * is otherwise always erased.
 *
 * Format. The G4 programs flash one 64-bit double word at a time, with ECC
 * over all 64 bits, and a double word can be programmed only once per erase.
 * So everything is one double word ("slot"):
 *
 *   slot 0       header  u32 magic "BLPS", u16 generation, u16 crc16
 *   slots 1..    records u16 tag, u16 value, u8 kind, u8 0xFF, u16 crc16
 *
 * crc16 is CRC-16/CCITT-FALSE over the first 6 bytes. An all-0xFF slot is
 * free; erased flash reads 0xFF on this part, never zeros. A record whose CRC
 * fails (a write torn by power loss) is skipped but keeps its slot. `kind` is
 * SET (a saved value) or CLEAR (no saved value: use the factory value, so a
 * firmware that changes the factory value applies it).
 *
 * The header is written only when a partition is initialised, and it is the
 * last write of that initialisation: on first use (an empty header, then
 * records are appended) and at the end of a compaction, as its commit. It is
 * never rewritten afterwards; the partition's next change is its erase.
 *
 * `generation` grows by one at each compaction (skipping 0, which status()
 * uses for "none"). Its only job is to tell the fresh partition from the stale
 * one when power was lost between committing the new header and erasing the
 * old partition, leaving both with a valid header. Wrap-around is handled by
 * serial-number comparison.
 *
 * Power loss at any point leaves at most one committed, consistent partition
 * after init(): a partition without a valid header is erased, and of two valid
 * ones the older generation is erased.
 *
 * Sizing: one 2 KB page per partition. A page is 256 slots, so 255 records.
 * The firmware has 61 persistent properties, so a compaction carries at most
 * 61 records and leaves at least 194 slots free. At the datasheet's minimum of
 * 10 k erase cycles per page that is 2 x 10 k x 194 ~= 3.9 M records over the
 * device's life, or 60 k Saves even if every Save changed all 61 values. A
 * second page per partition would cost 4 KB of application flash and double
 * the ~22 ms erase stall (code runs from the same flash) for headroom nobody
 * needs. If the flash is set to single-bank (DBANK=0), pages are 4 KB and both
 * partitions would share one erase unit, so the store disables itself rather
 * than erase anything.
 *
 * This class is HAL-free: it reaches flash only through the Flash interface,
 * so the host tests run it on a RAM-backed fake that enforces the same rules.
 */

#include <stddef.h>
#include <stdint.h>

#include <gsl/span>

/* A double word torn by power loss can fail its ECC check, and reading it
 * raises an NMI. The store sets property_store_ecc_armed only around its own
 * slot reads; NMI_Handler, seeing a double ECC error while armed, clears it,
 * sets property_store_ecc_fault and returns, and the store treats that slot as
 * corrupt. (Arming rather than decoding FLASH->ECCR's bank-relative address
 * keeps the handler independent of how this 128 KB part maps its banks.) */
extern "C" volatile bool property_store_ecc_armed;
extern "C" volatile bool property_store_ecc_fault;

enum class StoreKind : uint8_t {
  Set   = 0x01,  /* value is the saved value */
  Clear = 0x02,  /* no saved value: use the factory value */
};

/* One record slot. Memory layout == flash layout (little-endian), so the
 * active partition is read in place through a span of these. */
struct alignas(8) StoreRecord {
  uint16_t tag;
  uint16_t value;
  uint8_t  kind;
  uint8_t  pad;  /* 0xFF */
  uint16_t crc;

  static StoreRecord make(uint16_t tag, StoreKind kind, uint16_t value);

  /* CRC good, kind known, not an erased or ECC-faulted slot. */
  bool is_valid() const;
  StoreKind record_kind() const { return static_cast<StoreKind>(kind); }
};
static_assert(sizeof(StoreRecord) == sizeof(uint64_t), "a record is one flash double word");

/* The two partitions, addressed in double-word slots: the G4 programming
 * granule, so a partial or misaligned write cannot be expressed. */
class Flash {
 public:
  static constexpr int kPartitions = 2;

  /* Read-only, memory-mapped view of partition p. */
  virtual gsl::span<const uint64_t> partition(int p) const = 0;
  /* Program one erased slot. */
  virtual bool program(int p, size_t slot, uint64_t dw) = 0;
  /* Erase partition p; true only if it reads back all 0xFF. */
  virtual bool erase(int p) = 0;
  /* Erase granularity of the device, in bytes. */
  virtual uint32_t page_size() const = 0;

 protected:
  ~Flash() = default;
};

class PropertyStore {
 public:
  enum class Result : uint8_t { Ok, FlashError, Disabled };

  struct Status {
    bool     enabled;     /* false: page size does not match one partition */
    int      partition;   /* active partition, -1 if none is committed */
    uint16_t generation;  /* of the active partition, 0 if none */
    uint16_t used;        /* record slots used in the active partition */
    uint16_t capacity;    /* record slots per partition */
    uint8_t  repaired;    /* bit p: init() erased partition p (stale or incomplete) */
  };

  /* known_tags: the persistent tags of this firmware. Compaction keeps only
   * these, so records of properties that no longer exist drop out. */
  PropertyStore(Flash &flash, gsl::span<const uint16_t> known_tags);

  /* Scan both partitions, repair what power loss left behind (reported in
   * status().repaired), find the active partition and the write position.
   * Call once before anything else. Never prints: the caller reports. */
  void init();

  /* Used record slots of the active partition, in write order, read in place.
   * Skip entries that are not is_valid(); for a tag, the last valid one wins. */
  gsl::span<const StoreRecord> records() const;

  /* Append a record, compacting into the other partition first when the
   * active one is full. */
  Result append(const StoreRecord &r);

  /* Erase both partitions: every saved value is gone. */
  Result drop();

  Status status() const;

 private:
  size_t slots() const { return flash_.partition(0).size(); }
  bool read_slot(int p, size_t slot, uint64_t *out) const;
  bool header_valid(int p, uint16_t *generation) const;
  bool blank(int p) const;
  bool ensure_blank(int p);
  bool write_slot(int p, size_t slot, uint64_t dw);
  bool write_header(int p, uint16_t generation);
  Result compact();

  Flash &flash_;
  gsl::span<const uint16_t> known_tags_;
  bool enabled_ = false;
  int active_ = -1;
  uint16_t generation_ = 0;
  size_t next_slot_ = 1;
  uint8_t repaired_ = 0;
};

#endif /* PROPERTY_STORE_H_ */
