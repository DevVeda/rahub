// SPDX-License-Identifier: GPL-3.0-or-later
// Lo propio de la PS5: idioma, notificaciones e ícono en la pestaña Multimedia.
#pragma once
#include <stddef.h>

// Idioma de la consola como etiqueta BCP 47 ("en-US", "es-419", "pt-BR"…).
const char *ps5_locale(void);
// Notificación del sistema.
void ps5_notify(const char *msg);
// Instala/actualiza el ícono de la app en Multimedia; abre http://127.0.0.1:<port>/.
void ps5_install_webapp(const char *title_id, const char *name, int port, const char *icon, size_t icon_n);
