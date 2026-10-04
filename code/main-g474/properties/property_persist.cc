/* Saved property values: the RAM cache of what the flash store holds, and the
 * C API of properties.h over it.
 *
 * The cache is filled once at boot from the store's records and then kept in
 * step with every record appended, so reads (property_default, the web tool's
 * GET_SAVED_VALUE) never touch flash. properties.c stays C; this file is C++
 * only because PropertyStore is. */

#include "properties.h"
#include "property_flash.h"
#include "property_store.h"

#include <stdio.h>

#include <array>

namespace {

/* Every tag in table order, then the persistent ones: the store's known tags,
 * which decide what survives a compaction. */
constexpr uint16_t kAllTags[] = {
#define PROPERTY(ptag, ctype, pname, ...) (ptag),
#include "property_table.def"
#undef PROPERTY
};
constexpr size_t kCount = sizeof(kAllTags) / sizeof(kAllTags[0]);

constexpr size_t count_persistent()
{
  size_t n = 0;
  for (uint16_t t : kAllTags)
    if (t != PROPERTY_TAG_NONE) n++;
  return n;
}
constexpr size_t kPersistentCount = count_persistent();

constexpr std::array<uint16_t, kPersistentCount> make_known_tags()
{
  std::array<uint16_t, kPersistentCount> tags{};
  size_t n = 0;
  for (uint16_t t : kAllTags)
    if (t != PROPERTY_TAG_NONE) tags[n++] = t;
  return tags;
}
constexpr std::array<uint16_t, kPersistentCount> kKnownTags = make_known_tags();

/* A compaction carries one record per persistent property; keeping that well
 * under the 255 slots of a page is what makes the log cheap (property_store.h). */
static_assert(kPersistentCount <= 127, "too many persistent properties for one-page partitions");

std::array<uint16_t, kCount> g_saved;
std::array<bool, kCount> g_has_saved;

PropertyStore &store()
{
  static PropertyStore s(property_flash(), kKnownTags);
  return s;
}

bool index_of_tag(uint16_t tag, size_t *out)
{
  for (size_t i = 0; i < kCount; i++) {
    if (kAllTags[i] == tag) {
      *out = i;
      return true;
    }
  }
  return false;
}

property_save_result_t from_store(PropertyStore::Result r)
{
  switch (r) {
    case PropertyStore::Result::Ok: return PROPERTY_SAVE_OK;
    case PropertyStore::Result::Disabled: return PROPERTY_SAVE_DISABLED;
    case PropertyStore::Result::FlashError: break;
  }
  return PROPERTY_SAVE_FLASH_ERROR;
}

char partition_name(int p)
{
  return static_cast<char>('A' + p);
}

}  /* namespace */

void property_load_from_flash(void)
{
  PropertyStore &s = store();
  s.init();

  g_has_saved.fill(false);
  for (const StoreRecord &r : s.records()) {
    size_t i;
    if (!r.is_valid() || !index_of_tag(r.tag, &i)) continue;
    if (r.record_kind() == StoreKind::Clear) {
      g_has_saved[i] = false;
      continue;
    }
    const property_desc_t *d = property_at(i);
    if (r.value < d->min || r.value > d->max) continue;  /* stale bounds: keep the previous one */
    g_saved[i] = r.value;
    g_has_saved[i] = true;
  }
  property_reset_all();

  PropertyStore::Status const st = s.status();
  for (int p = 0; p < Flash::kPartitions; p++)
    if (st.repaired & (1u << p)) printf("props: erased partition %c, left incomplete by a power loss\r\n", partition_name(p));

  property_store_status_t const ps = property_store_status();
  if (!ps.enabled)
    printf("props: saving disabled, flash pages are %lu bytes (needs 2048, DBANK=1)\r\n",
           (unsigned long)property_flash().page_size());
  else if (ps.partition < 0)
    printf("props: nothing saved, factory values\r\n");
  else
    printf("props: partition %c, generation %u, %u/%u slots used, %u saved value%s\r\n",
           partition_name(ps.partition), ps.generation, ps.used, ps.capacity, ps.saved_count,
           ps.saved_count == 1 ? "" : "s");
}

bool property_get_saved(size_t index, uint16_t *out)
{
  if (index >= kCount || !g_has_saved[index]) return false;
  if (out) *out = g_saved[index];
  return true;
}

uint16_t property_default(size_t index)
{
  const property_desc_t *d = property_at(index);
  if (!d) return 0;
  return g_has_saved[index] ? g_saved[index] : d->factory_value;
}

property_save_result_t property_set_saved(size_t index, uint16_t value)
{
  const property_desc_t *d = property_at(index);
  if (!d) return PROPERTY_SAVE_BAD_INDEX;
  if (d->tag == PROPERTY_TAG_NONE) return PROPERTY_SAVE_TRANSIENT;

  if (d->type == PROPERTY_TYPE_BOOL) value = value ? 1 : 0;
  if (value < d->min) value = d->min;
  if (value > d->max) value = d->max;

  if (value == d->factory_value) return property_clear_saved(index);
  if (g_has_saved[index] && g_saved[index] == value) return PROPERTY_SAVE_OK;

  property_save_result_t const r = from_store(store().append(StoreRecord::make(d->tag, StoreKind::Set, value)));
  if (r == PROPERTY_SAVE_OK) {
    g_saved[index] = value;
    g_has_saved[index] = true;
  }
  return r;
}

property_save_result_t property_clear_saved(size_t index)
{
  const property_desc_t *d = property_at(index);
  if (!d) return PROPERTY_SAVE_BAD_INDEX;
  if (d->tag == PROPERTY_TAG_NONE) return PROPERTY_SAVE_TRANSIENT;
  if (!g_has_saved[index]) return PROPERTY_SAVE_OK;

  property_save_result_t const r = from_store(store().append(StoreRecord::make(d->tag, StoreKind::Clear, 0)));
  if (r == PROPERTY_SAVE_OK) g_has_saved[index] = false;
  return r;
}

property_save_result_t property_save_all(size_t *changed)
{
  size_t n = 0;
  property_save_result_t r = PROPERTY_SAVE_OK;
  for (size_t i = 0; i < kCount && r == PROPERTY_SAVE_OK; i++) {
    if (kAllTags[i] == PROPERTY_TAG_NONE) continue;
    uint16_t const live = property_value(i);
    if (live == property_default(i)) continue;
    r = property_set_saved(i, live);
    if (r == PROPERTY_SAVE_OK) n++;
  }
  if (changed) *changed = n;
  return r;
}

property_save_result_t property_factory_reset(void)
{
  property_save_result_t const r = from_store(store().drop());
  g_has_saved.fill(false);
  property_reset_all();
  return r;
}

property_store_status_t property_store_status(void)
{
  PropertyStore::Status const st = store().status();
  property_store_status_t out{};
  out.enabled = st.enabled;
  out.partition = static_cast<int8_t>(st.partition);
  out.generation = st.generation;
  out.used = st.used;
  out.capacity = st.capacity;
  for (bool has : g_has_saved)
    if (has) out.saved_count++;
  return out;
}
