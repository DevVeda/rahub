// SPDX-License-Identifier: GPL-3.0-or-later
// Idiomas: un JSON por idioma en web/lang/<código>.json. El Makefile los mete todos en el ELF
// (build/langs.h), así que para añadir uno basta con soltar el archivo. Lo que falte sale en inglés.
#pragma once
#include "http.h"
#include <stddef.h>

// Idioma con archivo más cercano al de la consola: "es-419" -> "es-419", si no "es", si no "en".
const char *lang_pick(const char *locale);
// Texto `key` en `lang` (o en inglés), con {0} y {1} sustituidos por a0 y a1.
void lang_text(const char *lang, const char *key, char *out, size_t n, const char *a0, const char *a1);
// GET /lang/<código>.json para las páginas. 1 = servido.
int lang_serve(int fd, const struct req *r);
