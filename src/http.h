// SPDX-License-Identifier: GPL-3.0-or-later
// Servidor HTTP mínimo: un hilo por conexión, una petición por conexión.
//   - Los POST piden la cabecera X-RAHub (un formulario de otra web no puede ponerla): protección CSRF.
//   - Host tiene que ser una IP o localhost: una web con DNS rebinding no puede hablarle como si fuera suya.
//   - Como mucho MAX_CLIENTS a la vez, y cada petición tiene que llegar en REQ_SECONDS: nadie lo tumba
//     abriendo conexiones o mandando bytes muy despacio.
//   - POST /quit desde 127.0.0.1 cierra el proceso: así una versión nueva reemplaza a la vieja.
#pragma once
#include <stddef.h>

struct req {
  const char *method, *path, *query, *body;
  int local; // viene de 127.0.0.1 (otra app de la PS5)
};

// Archivo servido tal cual (ver EMBED).
struct asset {
  const char *path, *type, *data, *end;
};

typedef void (*http_handler)(int fd, const struct req *r);

// Abre el puerto; si está ocupado, pide a la copia anterior que salga y reintenta. -1 si no se pudo.
int http_listen(int port);
// Atiende para siempre.
void http_loop(int s, http_handler h);

int req_is(const struct req *r, const char *method, const char *path);
void http_send(int fd, int status, const char *type, const char *body, size_t n);
void http_json(int fd, int status, const char *json);
void http_error(int fd, int status, const char *msg); // {"error":"msg"}
// Sirve el asset de esa ruta si lo hay (GET). 1 = servido.
int http_asset(int fd, const struct req *r, const struct asset *a, size_t n);
