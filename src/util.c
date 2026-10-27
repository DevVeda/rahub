// SPDX-License-Identifier: GPL-3.0-or-later
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sanitize(char *dst, size_t n, const char *src) {
  size_t i = 0;
  for(; *src && i + 1 < n; src++)
    if((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || strchr("_.-", *src))
      dst[i++] = *src;
  dst[i] = 0;
}

static const char *skip_space(const char *p) {
  while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  return p;
}

// ponytail: extractor de JSON plano (login2, /session, archivos de idioma); no decodifica \uXXXX.
int json_str(const char *js, const char *k, char *out, size_t n) {
  char pat[40];
  size_t i = 0;
  snprintf(pat, sizeof pat, "\"%s\"", k);
  for(const char *p = strstr(js, pat); p; p = strstr(p + 1, pat)) {
    const char *v = skip_space(p + strlen(pat));
    if(*v != ':') continue; // era un valor, no una clave
    v = skip_space(v + 1);
    if(*v != '"') return 0;
    for(v++; *v && *v != '"' && i + 1 < n; v++) {
      if(*v == '\\' && v[1]) v++;
      out[i++] = *v;
    }
    out[i] = 0;
    return 1;
  }
  return 0;
}

void json_esc(char *dst, size_t n, const char *s) {
  size_t i = 0;
  for(; *s && i + 7 < n; s++) {
    unsigned char ch = *s;
    if(ch == '"' || ch == '\\') { dst[i++] = '\\'; dst[i++] = ch; }
    else if(ch < 0x20) i += snprintf(dst + i, n - i, "\\u%04x", ch);
    else dst[i++] = ch;
  }
  dst[i] = 0;
}

int form_get(const char *body, const char *k, char *out, size_t n) {
  size_t kl = strlen(k), i = 0;
  const char *p = body;
  while(p && !(strncmp(p, k, kl) == 0 && p[kl] == '=')) { p = strchr(p, '&'); if(p) p++; }
  if(!p) return 0;
  for(p += kl + 1; *p && *p != '&' && i + 1 < n; p++) {
    unsigned h;
    if(*p == '+') out[i++] = ' ';
    else if(*p == '%' && p[1] && p[2] && sscanf(p + 1, "%2x", &h) == 1) { out[i++] = (char)h; p += 2; }
    else out[i++] = *p;
  }
  out[i] = 0;
  return 1;
}

int file_equals(const char *path, const char *data, size_t n) {
  FILE *f = fopen(path, "rb");
  if(!f) return 0;
  char *b = malloc(n + 1);
  size_t r = b ? fread(b, 1, n + 1, f) : 0;
  fclose(f);
  int eq = b && r == n && !memcmp(b, data, n);
  free(b);
  return eq;
}

int file_write(const char *path, const char *data, size_t n) {
  FILE *f = fopen(path, "wb");
  if(!f) return -1;
  size_t w = fwrite(data, 1, n, f);
  return fclose(f) || w != n ? -1 : 0;
}
