#include "property_flash.h"

#include "flash_page.h"
#include "memmap.h"
#include "stm32g4xx_hal.h"

namespace {

static_assert(PROPS_PARTITION_SIZE % sizeof(uint64_t) == 0, "a partition is whole slots");

uint32_t partition_base(int p)
{
  return PROPS_BASE + static_cast<uint32_t>(p) * PROPS_PARTITION_SIZE;
}

/* Unlocks the flash controller for one operation and clears stale error
 * flags, which would otherwise fail the next HAL_FLASH_Program. */
class Unlocked {
 public:
  Unlocked() : ok_(HAL_FLASH_Unlock() == HAL_OK) { __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_SR_ERRORS); }
  ~Unlocked() { HAL_FLASH_Lock(); }
  bool ok() const { return ok_; }

 private:
  bool ok_;
};

class HalFlash final : public Flash {
 public:
  gsl::span<const uint64_t> partition(int p) const override
  {
    return gsl::span<const uint64_t>(reinterpret_cast<const uint64_t *>(partition_base(p)),
                                     PROPS_PARTITION_SIZE / sizeof(uint64_t));
  }

  bool program(int p, size_t slot, uint64_t dw) override
  {
    Unlocked unlocked;
    return unlocked.ok() &&
           flash_program_doubleword(partition_base(p) + static_cast<uint32_t>(slot * sizeof(uint64_t)), dw);
  }

  /* Stalls instruction fetch, and with it every interrupt, for ~22 ms. */
  bool erase(int p) override
  {
    Unlocked unlocked;
    return unlocked.ok() && flash_erase_page(partition_base(p));
  }

  uint32_t page_size() const override { return flash_page_size(); }
};

}  /* namespace */

Flash &property_flash()
{
  static HalFlash flash;
  return flash;
}
