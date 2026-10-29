// SPDX-License-Identifier: GPL-3.0-or-later
#include "ra.h"
#include "embed.h"
#include "util.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Certificados raíz (copia de /etc/ssl/cert.pem del Mac): la PS5 no le da unos a libcurl.
EMBED(ca_pem, "build/cacert.pem");

static size_t on_data(char *p, size_t sz, size_t nm, void *ud) {
  struct buf *b = ud;
  size_t len = sz * nm;
  if(b->n + len > (4u << 20)) return 0; // ponytail: tope de 4 MB por respuesta
  char *d = realloc(b->d, b->n + len + 1);
  if(!d) return 0;
  memcpy(d + b->n, p, len);
  b->d = d;
  b->n += len;
  d[b->n] = 0;
  return len;
}

long fetch(const char *url, const char *post, struct buf *b, char *err, size_t en) {
  return fetch_ua(url, post, NULL, b, err, en);
}

long fetch_ua(const char *url, const char *post, const char *ua, struct buf *b, char *err, size_t en) {
  struct curl_blob ca = { (void *)ca_pem, (size_t)(ca_pem_end - ca_pem), CURL_BLOB_NOCOPY };
  long code = -1;
  CURL *c = curl_easy_init();
  b->d = NULL;
  b->n = 0;
  if(!c) { snprintf(err, en, "curl_easy_init"); return -1; }
  curl_easy_setopt(c, CURLOPT_URL, url);
  if(post) curl_easy_setopt(c, CURLOPT_POSTFIELDS, post);
  curl_easy_setopt(c, CURLOPT_USERAGENT, ua ? ua : "RAHub/1.0.0 (PS5)");
  curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, ""); // gzip si RA lo da: menos tráfico para los dos
  curl_easy_setopt(c, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(c, CURLOPT_CAINFO_BLOB, &ca);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_data);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, b);
  CURLcode rc = curl_easy_perform(c);
  if(rc == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
  else snprintf(err, en, "%s", curl_easy_strerror(rc));
  curl_easy_cleanup(c);
  return code;
}

int ra_login(const char *user, const char *pass, struct ra_session *out, char *err, size_t en) {
  char body[1100], tok[64], usr[64]; // u y p escapados pueden ocupar el triple
  struct buf b;
  char *eu = curl_easy_escape(NULL, user, 0), *ep = curl_easy_escape(NULL, pass, 0);
  snprintf(body, sizeof body, "r=login2&u=%s&p=%s", eu ? eu : "", ep ? ep : "");
  curl_free(eu);
  curl_free(ep);
  long code = fetch(RA_URL "/dorequest.php", body, &b, err, en);
  memset(body, 0, sizeof body); // la contraseña no se queda en memoria más de lo necesario
  int ok = code > 0 && b.d && strstr(b.d, "\"Success\":true") && json_str(b.d, "Token", tok, sizeof tok) && json_str(b.d, "User", usr, sizeof usr);
  if(!ok && code > 0 && !(b.d && json_str(b.d, "Error", err, en))) snprintf(err, en, "HTTP %ld", code);
  free(b.d);
  if(!ok) return -1;
  sanitize(out->user, sizeof out->user, usr);
  sanitize(out->token, sizeof out->token, tok);
  out->key[0] = 0;
  return 0;
}

int ra_token_ok(const struct ra_session *s) {
  char body[256], e[128];
  struct buf b;
  snprintf(body, sizeof body, "r=login2&u=%s&t=%s", s->user, s->token);
  int ok = fetch(RA_URL "/dorequest.php", body, &b, e, sizeof e) == 200 && b.d && strstr(b.d, "\"Success\":true");
  free(b.d);
  return ok;
}

long ra_api(const char *api, const struct ra_session *s, const char *extra, struct buf *b, char *err, size_t en) {
  char url[512];
  snprintf(url, sizeof url, RA_URL "/API/%s?u=%s&y=%s&%s", api, s->user, s->key, extra);
  return fetch(url, NULL, b, err, en);
}
