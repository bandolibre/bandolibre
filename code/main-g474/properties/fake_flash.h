#ifndef FAKE_FLASH_H_
#define FAKE_FLASH_H_

/* Host-test stand-in for the two flash partitions of the property store.
 *
 * Enforces what the G4 enforces: a slot can be programmed only while erased,
 * and erasing sets every bit. A power cut can be scheduled after a number of
 * operations: the operation it lands on is torn (a program clears only some
 * of its bits, an erase reaches only the first half of the page) or, with
 * `cut_tears` false, does nothing; every later operation does nothing. */

#include <stdint.h>
#include <string.h>

#include "property_store.h"

class FakeFlash : public Flash {
 public:
  static constexpr size_t kSlots = 256;  /* 2 KB / 8 */

  FakeFlash() { memset(mem_, 0xFF, sizeof(mem_)); }

  gsl::span<const uint64_t> partition(int p) const override
  {
    return gsl::span<const uint64_t>(mem_[p], kSlots);
  }

  bool program(int p, size_t slot, uint64_t dw) override
  {
    if (!tick()) {
      if (torn_now_) mem_[p][slot] &= dw | 0x00FF00FF00FF00FFull;
      return false;
    }
    if (mem_[p][slot] != ~uint64_t{0}) return false;  /* PROGERR */
    mem_[p][slot] = dw;
    programs++;
    return true;
  }

  bool erase(int p) override
  {
    if (!tick()) {
      if (torn_now_) memset(mem_[p], 0xFF, sizeof(mem_[p]) / 2);
      return false;
    }
    memset(mem_[p], 0xFF, sizeof(mem_[p]));
    erases++;
    return true;
  }

  uint32_t page_size() const override { return page_size_; }

  /* Test controls. */
  void set_page_size(uint32_t s) { page_size_ = s; }
  void cut_after(long ops, bool tears) { ops_left_ = ops; cut_tears_ = tears; cut_happened_ = false; }
  void no_cut() { ops_left_ = -1; }
  bool cut_happened() const { return cut_happened_; }
  void poke(int p, size_t slot, uint64_t dw) { mem_[p][slot] = dw; }
  uint64_t peek(int p, size_t slot) const { return mem_[p][slot]; }
  void save(uint64_t (&out)[2][kSlots]) const { memcpy(out, mem_, sizeof(mem_)); }
  void restore(const uint64_t (&in)[2][kSlots]) { memcpy(mem_, in, sizeof(mem_)); }

  long programs = 0;
  long erases = 0;

 private:
  /* False once the power is gone; records whether this op is the torn one. */
  bool tick()
  {
    torn_now_ = false;
    if (ops_left_ < 0) return true;
    if (ops_left_ == 0) {
      torn_now_ = cut_tears_ && !cut_happened_;
      cut_happened_ = true;
      return false;
    }
    ops_left_--;
    return true;
  }

  alignas(8) uint64_t mem_[2][kSlots];
  uint32_t page_size_ = kSlots * sizeof(uint64_t);
  long ops_left_ = -1;
  bool cut_tears_ = false;
  bool cut_happened_ = false;
  bool torn_now_ = false;
};

#endif /* FAKE_FLASH_H_ */
