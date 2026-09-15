#include "properties_console.h"
#include "properties.h"
#include "ansi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *type_name(property_type_t t)
{
  return t == PROPERTY_TYPE_BOOL ? "bool" : "u16";
}

/* Current value of a property regardless of type (bool -> 0/1). */
static uint16_t current_value(size_t index)
{
  const property_desc_t *d = property_at(index);
  if (d->type == PROPERTY_TYPE_BOOL)
  {
    bool b = false;
    property_get_bool(index, &b);
    return b ? 1u : 0u;
  }
  uint16_t v = 0;
  property_get_u16(index, &v);
  return v;
}

/* "name = value  (min M, max X, default D)" */
static void print_one(size_t index)
{
  const property_desc_t *d = property_at(index);
  printf("%s = %u  (min %u, max %u, default %u)\r\n",
         d->name, current_value(index), d->min, d->max, d->default_value);
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

static bool property_cmd_get(int argc, const char *const *argv)
{
  if (argc < 2) { printf("usage: get <name>\r\n"); return false; }
  if (has_wildcard(argv[1]))
  {
    size_t matched = 0;
    for (size_t i = 0; i < property_count(); i++)
      if (name_match(argv[1], property_at(i)->name)) { print_one(i); matched++; }
    if (!matched) { printf("no property matches: %s\r\n", argv[1]); return false; }
    return true;
  }
  size_t i;
  if (!property_by_name(argv[1], &i)) { printf("unknown property: %s\r\n", argv[1]); return false; }
  print_one(i);
  return true;
}

/* Parse a property value string. Returns false (with a message) if not a number. */
static bool parse_value(const char *s, unsigned long *out)
{
  char *end;
  unsigned long v = strtoul(s, &end, 0);
  if (s[0] == '\0' || *end != '\0') { printf("not a number: %s\r\n", s); return false; }
  *out = v;
  return true;
}

static void set_one(size_t i, unsigned long v)
{
  if (property_at(i)->type == PROPERTY_TYPE_BOOL) property_set_bool(i, v != 0);
  else property_set_u16(i, v > 0xFFFFu ? 0xFFFFu : (uint16_t)v);
  print_one(i);
}

static bool property_cmd_set(int argc, const char *const *argv)
{
  if (argc < 3) { printf("usage: set <name> <value>\r\n"); return false; }
  unsigned long v;
  if (!parse_value(argv[2], &v)) return false;
  if (has_wildcard(argv[1]))
  {
    size_t matched = 0;
    for (size_t i = 0; i < property_count(); i++)
      if (name_match(argv[1], property_at(i)->name)) { set_one(i, v); matched++; }
    if (!matched) { printf("no property matches: %s\r\n", argv[1]); return false; }
    return true;
  }
  size_t i;
  if (!property_by_name(argv[1], &i)) { printf("unknown property: %s\r\n", argv[1]); return false; }
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

static void property_cmd_show(void)
{
  printf("%-18s %6s %6s %6s %8s\r\n", "name", "value", "min", "max", "default");
  for (size_t i = 0; i < property_count(); i++)
  {
    const property_desc_t *d = property_at(i);
    printf("%-18s %6u %6u %6u %8u\r\n", d->name, current_value(i), d->min, d->max, d->default_value);
  }
}

bool properties_execute(int argc, const char *const *argv)
{
  if (argc < 1) return false;
  if (strcmp(argv[0], "show") == 0)  { property_cmd_show();        return true; }
  if (strcmp(argv[0], "get") == 0)   { property_cmd_get(argc, argv);   return true; }
  if (strcmp(argv[0], "set") == 0)   { property_cmd_set(argc, argv);   return true; }
  if (strcmp(argv[0], "reset") == 0) { property_cmd_reset(argc, argv); return true; }
  return false;
}

void properties_help(const char *pattern)
{
  if (!pattern)
  {
    printf("Property commands:\r\n");
    printf("  show                 list all properties with current value\r\n");
    printf("  get <name>           show value, min, max and default (name may glob, e.g. log_*)\r\n");
    printf("  set <name> <value>   set a property, clamped to [min,max] (name may glob, e.g. log_*)\r\n");
    printf("  reset <name>         restore default(s) (name may glob, e.g. reset *)\r\n");
    printf("  help [name]          this help, or details of matching properties (name may glob)\r\n");
    return;
  }
  printf("Properties:\r\n");
  printf("%-28s %5s %6s %6s  %s\r\n", "name", "type", "min", "max", "description");
  size_t matched = 0;
  for (size_t i = 0; i < property_count(); i++)
  {
    const property_desc_t *d = property_at(i);
    if (!name_match(pattern, d->name)) continue;
    const char *bg = (matched++ & 1) ? ANSI_BG_GREY236 : "";
    printf("%s%-28s %5s %6u %6u  %s" ANSI_RESET "\r\n", bg, d->name, type_name(d->type), d->min, d->max, d->description);
  }
  if (!matched) printf("no property matches: %s\r\n", pattern);
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
