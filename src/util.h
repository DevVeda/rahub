// SPDX-License-Identifier: GPL-3.0-or-later
// Utilidades de texto y archivos.
#pragma once
#include <stddef.h>

// Copia sólo [A-Za-z0-9_.-]: lo que viene de fuera nunca llega crudo a HTML/JSON/URLs.
void sanitize(char *dst, size_t n, const char *src);
// Valor de "k": "…" en un JSON plano (sin anidar). 1 si estaba.
int json_str(const char *js, const char *k, char *out, size_t n);
// Escapa s para ir entre comillas en un JSON.
void json_esc(char *dst, size_t n, const char *s);
// Campo k de un form urlencoded, decodificado. 1 si estaba.
int form_get(const char *body, const char *k, char *out, size_t n);
// 1 si el archivo tiene exactamente esos bytes.
int file_equals(const char *path, const char *data, size_t n);
// 0 = escrito entero.
int file_write(const char *path, const char *data, size_t n);
