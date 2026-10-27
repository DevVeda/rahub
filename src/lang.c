// SPDX-License-Identifier: GPL-3.0-or-later
#include "lang.h"
#include "embed.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

// build/langs.h: una línea LANG(símbolo, "código", "web/lang/código.json") por archivo (lo genera el Makefile).
#define LANG(sym, code, file) EMBED(sym, file);
#include "langs.h"
#undef LANG

static const struct lang_file {
  const char *code, *data, *end;
} langs[] = {
#define LANG(sym, code, file) { code, sym, sym##_end },
#include "langs.h"
#undef LANG
};
#define NLANGS (sizeof langs / sizeof *langs)

static const struct lang_file *find(const char *code, size_t len) {
  for(size_t i = 0; i < NLANGS; i++)
    if(strlen(langs[i].code) == len && !strncasecmp(langs[i].code, code, len)) return &langs[i];
  return NULL;
}

const char *lang_pick(const char *locale) {
  const struct lang_file *l = find(locale, strlen(locale));
  if(!l) l = find(locale, strcspn(locale, "-_")); // idioma base: "es-419" -> "es"
  return l ? l->code : "en";
}

void lang_text(const char *lang, const char *key, char *out, size_t n, const char *a0, const char *a1) {
  char tpl[256];
  const struct lang_file *l = find(lang, strlen(lang)), *en = find("en", 2);
  if(!(l && json_str(l->data, key, tpl, sizeof tpl)) && !(en && json_str(en->data, key, tpl, sizeof tpl)))
    snprintf(tpl, sizeof tpl, "%s", key);
  size_t o = 0;
  for(const char *p = tpl; *p && o + 1 < n; p++) {
    const char *arg = p[0] == '{' && p[1] && p[2] == '}' ? (p[1] == '0' ? a0 : p[1] == '1' ? a1 : NULL) : NULL;
    if(arg) { o += snprintf(out + o, n - o, "%s", arg); p += 2; }
    else out[o++] = *p;
    if(o >= n) o = n - 1;
  }
  out[o] = 0;
}

int lang_serve(int fd, const struct req *r) {
  size_t len = strlen(r->path);
  if(strcmp(r->method, "GET") || strncmp(r->path, "/lang/", 6) || len < 12 || strcmp(r->path + len - 5, ".json")) return 0;
  const struct lang_file *l = find(r->path + 6, len - 11);
  if(!l) return 0;
  http_send(fd, 200, "application/json; charset=utf-8", l->data, (size_t)(l->end - l->data));
  return 1;
}
