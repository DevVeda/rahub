// SPDX-License-Identifier: GPL-3.0-or-later
// RA Hub Test: hace lo que haría una app que quiere la cuenta de RetroAchievements del usuario.
//   1. Pide la sesión al hub: GET http://127.0.0.1:8045/session (sólo responde dentro de la PS5).
//   2. Comprueba el usuario + token con RA (r=login2 con t=token, como los emuladores).
//   GET /           página (web/test.*), http://127.0.0.1:8046
//   GET /api/state  {"lang","user"}: user vacío = sin sesión válida
//   GET /lang/<x>.json textos de la página
#include "embed.h"
#include "http.h"
#include "lang.h"
#include "ps5.h"
#include "ra.h"
#include "util.h"
#include <curl/curl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define PORT 8046
#define HUB_SESSION "http://127.0.0.1:8045/session"

EMBED(test_html, "web/test.html");
EMBED(test_js, "web/test.js");
EMBED(common_css, "web/common.css");
EMBED(lib_js, "web/lib.js");
EMBED(icon_png, "assets/icon_test.png");

static const struct asset assets[] = {
  { "/", "text/html; charset=utf-8", test_html, test_html_end },
  { "/test.js", "text/javascript; charset=utf-8", test_js, test_js_end },
  { "/common.css", "text/css; charset=utf-8", common_css, common_css_end },
  { "/lib.js", "text/javascript; charset=utf-8", lib_js, lib_js_end },
};
#define NASSETS (sizeof assets / sizeof *assets)

static const char *lang = "en";

// 1 = el hub dio una sesión y RA la acepta.
static int hub_session(struct ra_session *s) {
  char v[64], e[128];
  struct buf b;
  long code = fetch(HUB_SESSION, NULL, &b, e, sizeof e);
  int ok = code == 200 && b.d && json_str(b.d, "user", v, sizeof v);
  sanitize(s->user, sizeof s->user, ok ? v : "");
  ok = ok && json_str(b.d, "token", v, sizeof v);
  sanitize(s->token, sizeof s->token, ok ? v : "");
  free(b.d);
  ok = ok && ra_token_ok(s);
  printf("[rahub-test] hub: %ld, sesión %s\n", code, ok ? s->user : "no válida");
  return ok;
}

static void route(int fd, const struct req *r) {
  struct ra_session s = { 0 };
  char js[160];
  if(http_asset(fd, r, assets, NASSETS) || lang_serve(fd, r)) return;
  if(!req_is(r, "GET", "/api/state")) { http_error(fd, 404, "no existe"); return; }
  snprintf(js, sizeof js, "{\"lang\":\"%s\",\"user\":\"%s\"}", lang, hub_session(&s) ? s.user : "");
  http_json(fd, 200, js);
}

int main(void) {
  signal(SIGPIPE, SIG_IGN);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  lang = lang_pick(ps5_locale());
  int s = http_listen(PORT);
  if(s < 0) return 1;
  ps5_install_webapp("RAHT00001", "RA Hub Test", PORT, icon_png, (size_t)(icon_png_end - icon_png));
  char msg[160], port[8];
  snprintf(port, sizeof port, "%d", PORT);
  lang_text(lang, "notify", msg, sizeof msg, "RA Hub Test", port);
  ps5_notify(msg);
  http_loop(s, route);
}
