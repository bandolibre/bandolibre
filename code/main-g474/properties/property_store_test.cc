/* Host unit tests for the flash property store (property_store.h), run on the
 * RAM-backed FakeFlash. Framework-free like the other *_test files: a CHECK
 * macro that counts failures and a final summary. */

#include "fake_flash.h"
#include "property_store.h"

#include <stdio.h>
#include <string.h>

#include <map>
#include <vector>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                     \
  do {                                                                  \
    g_checks++;                                                         \
    if (!(cond)) {                                                      \
      g_failures++;                                                     \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
    }                                                                   \
  } while (0)

using Saved = std::map<uint16_t, uint16_t>;  /* tag -> saved value */
using Result = PropertyStore::Result;

/* Tags 1..71, like the 71 persistent properties of the firmware. */
static std::vector<uint16_t> known_tags()
{
  std::vector<uint16_t> t;
  for (uint16_t i = 1; i <= 71; i++) t.push_back(i);
  return t;
}
static const std::vector<uint16_t> g_tags = known_tags();

/* What a boot would apply: the last valid record per tag. */
static Saved effective(const PropertyStore &s)
{
  Saved out;
  for (const StoreRecord &r : s.records()) {
    if (!r.is_valid()) continue;
    if (r.record_kind() == StoreKind::Set) out[r.tag] = r.value;
    else out.erase(r.tag);
  }
  return out;
}

/* A fresh boot over the same flash. */
static Saved reboot(FakeFlash &f)
{
  f.no_cut();
  PropertyStore s(f, g_tags);
  s.init();
  return effective(s);
}

static Result set(PropertyStore &s, uint16_t tag, uint16_t value)
{
  return s.append(StoreRecord::make(tag, StoreKind::Set, value));
}

static Result clear(PropertyStore &s, uint16_t tag)
{
  return s.append(StoreRecord::make(tag, StoreKind::Clear, 0));
}

/* Same CRC as the store, to craft headers for the generation tests. */
static uint16_t crc16(const uint8_t *b)
{
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < 6; i++) {
    crc ^= (uint16_t)(b[i] << 8);
    for (int bit = 0; bit < 8; bit++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

static uint64_t header(uint16_t generation)
{
  uint8_t b[8] = {'B', 'L', 'P', 'S', (uint8_t)generation, (uint8_t)(generation >> 8), 0, 0};
  uint16_t crc = crc16(b);
  b[6] = (uint8_t)crc;
  b[7] = (uint8_t)(crc >> 8);
  uint64_t dw;
  memcpy(&dw, b, 8);
  return dw;
}

static uint64_t record(uint16_t tag, uint16_t value)
{
  StoreRecord r = StoreRecord::make(tag, StoreKind::Set, value);
  uint64_t dw;
  memcpy(&dw, &r, 8);
  return dw;
}

static bool blank(const FakeFlash &f, int p)
{
  for (size_t i = 0; i < FakeFlash::kSlots; i++)
    if (f.peek(p, i) != ~uint64_t{0}) return false;
  return true;
}

/* ---- tests -----------------------------------------------------------------*/

static void test_empty_boot()
{
  FakeFlash f;
  PropertyStore s(f, g_tags);
  s.init();
  PropertyStore::Status st = s.status();
  CHECK(st.enabled);
  CHECK(st.partition == -1);
  CHECK(st.generation == 0);
  CHECK(st.used == 0);
  CHECK(st.capacity == 255);
  CHECK(s.records().empty());
  CHECK(f.programs == 0 && f.erases == 0);  /* booting never writes a blank store */
}

static void test_append_and_reload()
{
  FakeFlash f;
  PropertyStore s(f, g_tags);
  s.init();
  CHECK(set(s, 1, 100) == Result::Ok);
  CHECK(set(s, 2, 200) == Result::Ok);
  CHECK(set(s, 1, 101) == Result::Ok);

  PropertyStore::Status st = s.status();
  CHECK(st.partition == 0 && st.generation == 1 && st.used == 3);
  CHECK(blank(f, 1));

  Saved want = {{1, 101}, {2, 200}};
  CHECK(effective(s) == want);
  CHECK(reboot(f) == want);

  PropertyStore again(f, g_tags);
  again.init();
  CHECK(again.status().used == 3);  /* write position recovered */
  CHECK(set(again, 3, 300) == Result::Ok);
  CHECK(again.status().used == 4);
}

static void test_clear()
{
  FakeFlash f;
  PropertyStore s(f, g_tags);
  s.init();
  set(s, 1, 5);
  set(s, 2, 6);
  CHECK(clear(s, 1) == Result::Ok);
  CHECK(reboot(f) == (Saved{{2, 6}}));
}

static void test_corrupt_slot_skipped()
{
  FakeFlash f;
  PropertyStore s(f, g_tags);
  s.init();
  set(s, 1, 10);
  set(s, 2, 20);
  /* Damage record 2 (slot 2): a bad CRC, as a torn write leaves. */
  f.poke(0, 2, f.peek(0, 2) & ~(uint64_t{0xFFFF} << 16));  /* value 20 -> 0, CRC no longer matches */
  CHECK(reboot(f) == (Saved{{1, 10}}));

  PropertyStore again(f, g_tags);
  again.init();
  CHECK(again.status().used == 2);  /* the corrupt slot stays used */
  set(again, 3, 30);
  CHECK(reboot(f) == (Saved{{1, 10}, {3, 30}}));

  /* An ECC fault on a read makes that record invalid. */
  const StoreRecord &first = again.records()[0];
  CHECK(first.is_valid());
  property_store_ecc_fault = true;
  CHECK(!first.is_valid());
  CHECK(!property_store_ecc_fault);  /* consumed */
}

static void test_compaction()
{
  FakeFlash f;
  PropertyStore s(f, g_tags);
  s.init();

  set(s, 999, 1);  /* a tag this firmware no longer knows: must drop out */
  set(s, 41, 70);
  clear(s, 41);    /* cleared: must not be carried */
  /* Fill the page exactly, cycling through tags 1..40. */
  for (uint16_t v = 0; s.status().used < s.status().capacity; v++) set(s, (uint16_t)(v % 40 + 1), v);
  Saved model = effective(s);
  CHECK(model.size() == 41);  /* 1..40 and 999 */
  model.erase(999);

  CHECK(s.status().partition == 0);
  CHECK(f.erases == 0);
  /* The next append compacts into partition B. */
  CHECK(set(s, 50, 5000) == Result::Ok);
  model[50] = 5000;

  PropertyStore::Status st = s.status();
  CHECK(st.partition == 1);
  CHECK(st.generation == 2);
  CHECK(st.used == 41);  /* 40 carried + the pending record */
  CHECK(blank(f, 0));

  Saved got = reboot(f);
  CHECK(got == model);
  CHECK(got.find(999) == got.end());
}

static void test_both_committed_newer_wins()
{
  /* Power lost after a compaction committed B, before it erased A. */
  FakeFlash f;
  f.poke(0, 0, header(3));
  f.poke(0, 1, record(1, 10));
  f.poke(1, 0, header(4));
  f.poke(1, 1, record(1, 20));
  PropertyStore s(f, g_tags);
  s.init();
  CHECK(s.status().partition == 1);
  CHECK(s.status().repaired == 1);  /* A erased */
  CHECK(effective(s) == (Saved{{1, 20}}));
  CHECK(blank(f, 0));

  /* Across wrap-around: generation 1 is newer than 0xFFFF. */
  FakeFlash w;
  w.poke(0, 0, header(1));
  w.poke(0, 1, record(1, 20));
  w.poke(1, 0, header(0xFFFF));
  w.poke(1, 1, record(1, 10));
  PropertyStore sw(w, g_tags);
  sw.init();
  CHECK(sw.status().partition == 0);
  CHECK(effective(sw) == (Saved{{1, 20}}));
  CHECK(blank(w, 1));
}

static void test_generation_wraps_past_zero()
{
  FakeFlash f;
  f.poke(0, 0, header(0xFFFF));
  for (size_t i = 1; i < FakeFlash::kSlots; i++) f.poke(0, i, record(1, (uint16_t)i));
  PropertyStore s(f, g_tags);
  s.init();
  CHECK(s.status().used == 255);
  CHECK(set(s, 2, 2) == Result::Ok);
  CHECK(s.status().partition == 1);
  CHECK(s.status().generation == 1);  /* 0 is reserved for "none" */
  CHECK(reboot(f) == (Saved{{1, 255}, {2, 2}}));
}

static void test_incomplete_partition_erased()
{
  /* Records but no header: a compaction that never committed. */
  FakeFlash f;
  f.poke(0, 0, header(5));
  f.poke(0, 1, record(1, 10));
  f.poke(1, 1, record(1, 99));
  PropertyStore s(f, g_tags);
  s.init();
  CHECK(s.status().partition == 0);
  CHECK(s.status().repaired == 2);  /* B erased */
  CHECK(blank(f, 1));
  CHECK(effective(s) == (Saved{{1, 10}}));
}

static void test_drop()
{
  FakeFlash f;
  PropertyStore s(f, g_tags);
  s.init();
  set(s, 1, 10);
  CHECK(s.drop() == Result::Ok);
  CHECK(blank(f, 0) && blank(f, 1));
  CHECK(s.status().partition == -1 && s.status().generation == 0);
  CHECK(reboot(f).empty());
  CHECK(set(s, 2, 20) == Result::Ok);  /* usable again right away */
  CHECK(reboot(f) == (Saved{{2, 20}}));
}

static void test_disabled_on_4k_pages()
{
  FakeFlash f;
  f.set_page_size(4096);
  f.poke(0, 0, header(1));
  f.poke(1, 5, 0x1234);  /* garbage that a working store would erase */
  PropertyStore s(f, g_tags);
  s.init();
  CHECK(!s.status().enabled);
  CHECK(s.records().empty());
  CHECK(set(s, 1, 1) == Result::Disabled);
  CHECK(s.drop() == Result::Disabled);
  CHECK(f.programs == 0 && f.erases == 0);
  CHECK(f.peek(1, 5) == 0x1234);
}

/* Cut the power after every single flash operation of `op`, with and without
 * tearing the operation it lands on, and check that a reboot always finds
 * either the state before or the state after, never a mix or a loss. Then
 * check the store is still writable. */
template <typename Setup, typename Op>
static void power_loss_sweep(const char *what, Setup setup, Op op)
{
  FakeFlash f;
  {
    PropertyStore s(f, g_tags);
    s.init();
    setup(s);
  }
  uint64_t snapshot[2][FakeFlash::kSlots];
  f.save(snapshot);
  Saved before = reboot(f);

  /* Reference run, no cut. */
  Saved after;
  {
    PropertyStore s(f, g_tags);
    s.init();
    CHECK(op(s) == Result::Ok);
    after = effective(s);
  }

  int cuts = 0;
  for (int tears = 0; tears <= 1; tears++) {
    for (long n = 0;; n++) {
      f.restore(snapshot);
      PropertyStore s(f, g_tags);
      s.init();
      f.cut_after(n, tears != 0);
      op(s);
      bool cut = f.cut_happened();
      Saved got = reboot(f);
      if (got != before && got != after) {
        g_failures++;
        printf("FAIL %s: cut after %ld ops (%s) gave a state that is neither before nor after\n", what, n,
               tears ? "torn" : "clean");
      }
      g_checks++;
      /* Still writable after the reboot. */
      PropertyStore again(f, g_tags);
      again.init();
      CHECK(set(again, 60, 6060) == Result::Ok);
      Saved later = reboot(f);
      got[60] = 6060;
      CHECK(later == got);
      if (!cut) break;
      cuts++;
    }
  }
  CHECK(cuts > 0);
}

static void test_power_loss()
{
  power_loss_sweep(
      "first append", [](PropertyStore &) {}, [](PropertyStore &s) { return set(s, 1, 11); });

  power_loss_sweep(
      "plain append", [](PropertyStore &s) { set(s, 1, 10); },
      [](PropertyStore &s) { return set(s, 1, 11); });

  power_loss_sweep(
      "compaction",
      [](PropertyStore &s) {
        uint16_t v = 0;
        while (s.status().used < s.status().capacity) {
          set(s, (uint16_t)(v % 71 + 1), v);
          v++;
        }
      },
      [](PropertyStore &s) { return set(s, 5, 777); });

  power_loss_sweep(
      "drop", [](PropertyStore &s) { set(s, 1, 10); set(s, 2, 20); },
      [](PropertyStore &s) { return s.drop(); });
}

int main()
{
  test_empty_boot();
  test_append_and_reload();
  test_clear();
  test_corrupt_slot_skipped();
  test_compaction();
  test_both_committed_newer_wins();
  test_generation_wraps_past_zero();
  test_incomplete_partition_erased();
  test_drop();
  test_disabled_on_4k_pages();
  test_power_loss();

  printf("property_store_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures ? 1 : 0;
}
