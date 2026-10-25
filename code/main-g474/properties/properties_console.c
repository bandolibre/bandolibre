#include "properties_console.h"
#include "properties.h"
#include "ansi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *type_name(property_type_t t)
{
  switch (t)
  {
    case PROPERTY_TYPE_BOOL:  return "bool";
    case PROPERTY_TYPE_POINT: return "point";
    default:                  return "u16";
  }
}

/* A value as the console shows it: a number, or "x,y" for a point. A point's
 * 16-bit value packs x in its low byte and y in its high byte. */
typedef struct { char s[16]; } value_text_t;

static value_text_t value_text(const property_desc_t *d, uint16_t v)
{
  value_text_t t;
  if (d->type == PROPERTY_TYPE_POINT) snprintf(t.s, sizeof t.s, "%u,%u", v & 0xFFu, v >> 8);
  else snprintf(t.s, sizeof t.s, "%u", v);
  return t;
}

/* The range column: a point's coordinates each span a byte. */
static value_text_t range_text(const property_desc_t *d)
{
  value_text_t t;
  if (d->type == PROPERTY_TYPE_POINT) snprintf(t.s, sizeof t.s, "x,y 0..255");
  else snprintf(t.s, sizeof t.s, "[%u, %u]", d->min, d->max);
  return t;
}

/* "name = value  (min M, max X, default D)", the default being the saved
 * value if there is one: "(..., saved S, factory F)". A point has no min/max. */
static void print_one(size_t index)
{
  const property_desc_t *d = property_at(index);
  uint16_t saved;
  char bounds[24] = "";
  if (d->type != PROPERTY_TYPE_POINT) snprintf(bounds, sizeof bounds, "min %u, max %u, ", d->min, d->max);
  if (property_get_saved(index, &saved))
    printf("%s = %s  (%ssaved %s, factory %s)\r\n", d->name, value_text(d, property_value(index)).s,
           bounds, value_text(d, saved).s, value_text(d, d->factory_value).s);
  else
    printf("%s = %s  (%sdefault %s)\r\n", d->name, value_text(d, property_value(index)).s,
           bounds, value_text(d, d->factory_value).s);
}

/* Shell-style glob match supporting '*' (any run) and '?' (one char). Used so
 * `get log_*` / `set log_* 0` can address a whole family of properties at once. */
static bool name_match(const char *pat, const char *name)
{
  while (*pat)
  {
    if (*pat == '*')
    {
      while (*pat == '*') pat++;
      if (!*pat) return true;
      for (; *name; name++)
        if (name_match(pat, name)) return true;
      return false;
    }
    if (!*name || (*pat != '?' && *pat != *name)) return false;
    pat++; name++;
  }
  return !*name;
}

static bool has_wildcard(const char *s)
{
  return strchr(s, '*') || strchr(s, '?');
}

/* One row of the `get` table; `row` alternates the background. The default
 * column is the saved value, starred, if there is one, else the firmware's
 * factory value. */
static void print_row(size_t index, size_t row)
{
  const property_desc_t *d = property_at(index);
  uint16_t saved;
  bool has = property_get_saved(index, &saved);
  printf("%s%-28s %5s %-12s %7s %7s%c %8s  %s" ANSI_RESET "\r\n", (row & 1) ? ANSI_BG_GREY236 : "",
         d->name, type_name(d->type), range_text(d).s, value_text(d, property_value(index)).s,
         value_text(d, has ? saved : d->factory_value).s, has ? '*' : ' ',
         value_text(d, d->factory_value).s, d->description);
}

static bool property_cmd_get(int argc, const char *const *argv)
{
  if (argc < 2) { printf("usage: get <name>\r\n"); return false; }
  size_t matched = 0;
  for (size_t i = 0; i < property_count(); i++)
  {
    if (!name_match(argv[1], property_at(i)->name)) continue;
    if (!matched)
      printf(ANSI_BOLD ANSI_FG_GREEN "%-28s %5s %-12s %7s %8s %8s  %s" ANSI_RESET "\r\n", "name", "type", "range", "value", "default",
             "firmware", "description");
    print_row(i, matched++);
  }
  if (!matched) { printf("no property matches: %s\r\n", argv[1]); return false; }
  return true;
}

/* Parse a value string for property i: a number, or "x,y" (each 0..255) for a
 * point. Returns false (with a message) if it doesn't fit the property. */
static bool parse_value(size_t i, const char *s, uint16_t *out)
{
  const property_desc_t *d = property_at(i);
  char *end;
  unsigned long v = strtoul(s, &end, 0);
  if (d->type == PROPERTY_TYPE_POINT)
  {
    if (s[0] == '\0' || *end != ',') { printf("%s: not an x,y point: %s\r\n", d->name, s); return false; }
    const char *ys = end + 1;
    unsigned long y = strtoul(ys, &end, 0);
    if (ys[0] == '\0' || *end != '\0' || v > 255 || y > 255)
    { printf("%s: not an x,y point (each 0..255): %s\r\n", d->name, s); return false; }
    *out = PROPERTY_POINT(v, y);
    return true;
  }
  if (s[0] == '\0' || *end != '\0') { printf("not a number: %s\r\n", s); return false; }
  *out = v > 0xFFFFu ? 0xFFFFu : (uint16_t)v;
  return true;
}

static void set_one(size_t i, uint16_t v)
{
  property_set_value(i, v);
  print_one(i);
}

static bool property_cmd_set(int argc, const char *const *argv)
{
  /* `set <bool name>` with no value flips the boolean. */
  if (argc == 2 && !has_wildcard(argv[1]))
  {
    size_t i;
    bool cur;
    if (!property_by_name(argv[1], &i)) { printf("unknown property: %s\r\n", argv[1]); return false; }
    if (!property_get_bool(i, &cur)) { printf("usage: set <name> <value>  (%s is not a boolean)\r\n", argv[1]); return false; }
    set_one(i, !cur);
    return true;
  }
  if (argc < 3) { printf("usage: set <name> [<value>]  (the value may be left out for a boolean)\r\n"); return false; }
  uint16_t v;
  if (has_wildcard(argv[1]))
  {
    size_t matched = 0;
    for (size_t i = 0; i < property_count(); i++)
    {
      if (!name_match(argv[1], property_at(i)->name)) continue;
      matched++;
      if (parse_value(i, argv[2], &v)) set_one(i, v);
    }
    if (!matched) { printf("no property matches: %s\r\n", argv[1]); return false; }
    return true;
  }
  size_t i;
  if (!property_by_name(argv[1], &i)) { printf("unknown property: %s\r\n", argv[1]); return false; }
  if (!parse_value(i, argv[2], &v)) return false;
  set_one(i, v);
  return true;
}

static bool property_cmd_reset(int argc, const char *const *argv)
{
  if (argc < 2) { printf("usage: reset <name>\r\n"); return false; }
  if (has_wildcard(argv[1]))
  {
    size_t matched = 0;
    for (size_t i = 0; i < property_count(); i++)
      if (name_match(argv[1], property_at(i)->name)) { property_reset(i); print_one(i); matched++; }
    if (!matched) { printf("no property matches: %s\r\n", argv[1]); return false; }
    return true;
  }
  size_t i;
  if (!property_by_name(argv[1], &i)) { printf("unknown property: %s\r\n", argv[1]); return false; }
  property_reset(i);
  print_one(i);
  return true;
}

/* The default column is the saved value, starred, if there is one. */
static void property_cmd_show(void)
{
  printf("%-18s %7s %6s %6s %8s %8s\r\n", "name", "value", "min", "max", "default", "factory");
  for (size_t i = 0; i < property_count(); i++)
  {
    const property_desc_t *d = property_at(i);
    uint16_t saved;
    bool has = property_get_saved(i, &saved);
    printf("%-18s %7s %6u %6u %7s%c %8s\r\n", d->name, value_text(d, property_value(i)).s, d->min, d->max,
           value_text(d, has ? saved : d->factory_value).s, has ? '*' : ' ',
           value_text(d, d->factory_value).s);
  }
}

static const char *save_error(property_save_result_t r)
{
  switch (r)
  {
    case PROPERTY_SAVE_OK:          return "ok";
    case PROPERTY_SAVE_BAD_INDEX:   return "bad index";
    case PROPERTY_SAVE_TRANSIENT:   return "not a saved property";
    case PROPERTY_SAVE_FLASH_ERROR: return "flash write failed";
    case PROPERTY_SAVE_DISABLED:    return "saving disabled (flash not in 2 KB pages)";
  }
  return "?";
}

static void property_cmd_save(void)
{
  size_t n = 0;
  property_save_result_t r = property_save_all(&n);
  if (r != PROPERTY_SAVE_OK) printf("save: %s after %u changes\r\n", save_error(r), (unsigned)n);
  else printf("saved %u change%s\r\n", (unsigned)n, n == 1 ? "" : "s");
}

static void property_cmd_factory_reset(void)
{
  property_save_result_t r = property_factory_reset();
  if (r != PROPERTY_SAVE_OK) printf("factory_reset: %s\r\n", save_error(r));
  else printf("saved values erased, every property back to its factory value\r\n");
}

static void property_cmd_store(void)
{
  property_store_status_t st = property_store_status();
  if (!st.enabled) { printf("store: %s\r\n", save_error(PROPERTY_SAVE_DISABLED)); return; }
  if (st.partition < 0) { printf("store: nothing saved\r\n"); return; }
  printf("store: partition %c, generation %u, %u/%u slots used, %u saved value%s\r\n",
         'A' + st.partition, st.generation, st.used, st.capacity, st.saved_count,
         st.saved_count == 1 ? "" : "s");
}

bool properties_execute(int argc, const char *const *argv)
{
  if (argc < 1) return false;
  if (strcmp(argv[0], "show") == 0)  { property_cmd_show();        return true; }
  if (strcmp(argv[0], "get") == 0)   { property_cmd_get(argc, argv);   return true; }
  if (strcmp(argv[0], "set") == 0)   { property_cmd_set(argc, argv);   return true; }
  if (strcmp(argv[0], "reset") == 0) { property_cmd_reset(argc, argv); return true; }
  if (strcmp(argv[0], "save") == 0)  { property_cmd_save();         return true; }
  if (strcmp(argv[0], "factory_reset") == 0) { property_cmd_factory_reset(); return true; }
  if (strcmp(argv[0], "store") == 0) { property_cmd_store();        return true; }
  return false;
}

void properties_help(void)
{
  printf("Property commands:\r\n");
  printf("  show                 list all properties with current value (* = saved default)\r\n");
  printf("  get <name>           table of value, range, default, firmware value and description\r\n");
  printf("                       (name may glob, e.g. log_* or *)\r\n");
  printf("  set <name> [<value>] set a property, clamped to its range (name may glob, e.g. log_*);\r\n");
  printf("                       without a value, a boolean is toggled; a point takes x,y\r\n");
  printf("  reset <name>         restore default(s): saved value, else factory (name may glob)\r\n");
  printf("  save                 save current values as defaults, in flash\r\n");
  printf("  factory_reset        erase saved values, restore every factory value\r\n");
  printf("  store                state of the saved-values flash store\r\n");
  printf("  help                 this help\r\n");
}

size_t properties_complete(const char *prefix, const char **out, size_t cap)
{
  size_t n = 0;
  size_t plen = prefix ? strlen(prefix) : 0;
  for (size_t i = 0; i < property_count() && n < cap; i++)
  {
    const char *name = property_at(i)->name;
    if (strncmp(name, prefix ? prefix : "", plen) == 0) out[n++] = name;
  }
  return n;
}
