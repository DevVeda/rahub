// SPDX-License-Identifier: GPL-3.0-or-later
// Cliente HTTP(S) (libcurl, certificados raíz dentro del ELF) y las llamadas a RetroAchievements.
#pragma once
#include <stddef.h>

#define RA_URL "https://retroachievements.org"

struct ra_session {
  char user[64], token[64], key[64]; // key = Web API Key (vacía si no hay)
};

struct buf {
  char *d; // terminado en \0; lo libera quien llama
  size_t n;
};

// GET (post == NULL) o POST. Devuelve el código HTTP, o -1 sin red (motivo en err).
long fetch(const char *url, const char *post, struct buf *b, char *err, size_t en);

// Login con contraseña (r=login2). 0 = ok, rellena user y token; si no, motivo en err.
int ra_login(const char *user, const char *pass, struct ra_session *out, char *err, size_t en);
// 1 si RA acepta el usuario + token (lo que hace una app antes de usarlos).
int ra_token_ok(const struct ra_session *s);
// GET a la API web: /API/<api>?u=<user>&y=<key>&<extra>.
long ra_api(const char *api, const struct ra_session *s, const char *extra, struct buf *b, char *err, size_t en);
