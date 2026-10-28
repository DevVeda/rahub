// SPDX-License-Identifier: GPL-3.0-or-later
// RA Hub: guarda la sesión de RetroAchievements en la PS5 y se la da a otras apps.
//   GET  /              página (web/hub.*), desde la PS5 o la red local: http://<ip-ps5>:8045
//   GET  /api/state     {"lang","port","user","key"}: idioma (con archivo en web/lang) y si hay sesión / Web API Key
//   GET  /lang/<x>.json textos de la página
//   GET  /api/overview  resumen del perfil (API web de RA, con caché); ?refresh=1 adelanta el resumen
//   POST /login         u, p -> guarda usuario + token (nunca la contraseña)
//   POST /key           k = Web API Key (vacía = quitarla)
//   POST /logout        borra la sesión
//   GET  /session       {"user","token","key"}  sólo desde 127.0.0.1 (apps en la misma PS5)
#include "embed.h"
#include "http.h"
#include "lang.h"
#include "ps5.h"
#include "ra.h"
#include "util.h"
#include <curl/curl.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PORT 8045
#define DATA_DIR "/data/rahub"
#define SESSION_FILE DATA_DIR "/session"

EMBED(hub_html, "web/hub.html");
EMBED(hub_css, "web/hub.css");
EMBED(hub_js, "web/hub.js");
EMBED(common_css, "web/common.css");
EMBED(lib_js, "web/lib.js");
EMBED(icon_png, "assets/icon0.png");

static const struct asset assets[] = {
  { "/", "text/html; charset=utf-8", hub_html, hub_html_end },
  { "/hub.css", "text/css; charset=utf-8", hub_css, hub_css_end },
  { "/hub.js", "text/javascript; charset=utf-8", hub_js, hub_js_end },
  { "/common.css", "text/css; charset=utf-8", common_css, common_css_end },
  { "/lib.js", "text/javascript; charset=utf-8", lib_js, lib_js_end },
};
#define NASSETS (sizeof assets / sizeof *assets)

// Dos locks, y nunca se responde con uno tomado (un cliente lento no frena a nadie):
//   ses_mx    la sesión; se toma un instante (copiar, cambiar, guardar).
//   cache_mx  la caché; quien la refresca la retiene mientras habla con RA, así dos visitas a la vez
//             no repiten consultas. /session y /api/state no la tocan: una app nunca espera a RA.
// Orden: cache_mx antes que ses_mx.
static pthread_mutex_t ses_mx = PTHREAD_MUTEX_INITIALIZER, cache_mx = PTHREAD_MUTEX_INITIALIZER;
static struct ra_session ses;
static const char *lang = "en"; // idioma de la consola con archivo (ver lang.h)

// ---------- Sesión en disco: usuario, token y key, una por línea ("-" = sin key) ----------

// Con ses_mx tomado. Se crea ya con permisos 0600 (el token no queda legible ni un instante).
static void session_save(void) {
  int fd = open(SESSION_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  FILE *f = fd < 0 ? NULL : fdopen(fd, "w");
  if(!f) { perror("[rahub] " SESSION_FILE); if(fd >= 0) close(fd); return; }
  fchmod(fd, 0600); // por si ya existía con otros permisos
  fprintf(f, "%s\n%s\n%s\n", ses.user, ses.token, ses.key[0] ? ses.key : "-");
  fclose(f);
}

static void session_load(void) {
  char u[64] = "", t[64] = "", k[64] = "";
  FILE *f = fopen(SESSION_FILE, "r");
  if(!f) return;
  if(fscanf(f, "%63s %63s %63s", u, t, k) >= 2) {
    sanitize(ses.user, sizeof ses.user, u);
    sanitize(ses.token, sizeof ses.token, t);
    sanitize(ses.key, sizeof ses.key, strcmp(k, "-") ? k : "");
  }
  fclose(f);
}

// ---------- Caché de la API web: cada cosa con su vida según cuánto cambia ----------
// Sólo se pide a RA cuando alguien abre la página; nada en segundo plano.

static struct item {
  const char *name, *api, *extra;
  int ttl;
  char *body;
  time_t at;
} items[] = {
  { "summary", "API_GetUserSummary.php", "g=5&a=10", 120 },
  { "awards", "API_GetUserAwards.php", "", 3600 },
  { "aotw", "API_GetAchievementOfTheWeek.php", "", 6 * 3600 },
  { "wtp", "API_GetUserWantToPlayList.php", "c=5", 3600 },
};
#define NITEMS (sizeof items / sizeof *items)

// Con cache_mx tomado.
static void cache_drop(void) {
  for(size_t i = 0; i < NITEMS; i++) { free(items[i].body); items[i].body = NULL; items[i].at = 0; }
}

// Con cache_mx tomado. Refresca lo caducado con la sesión s. ?refresh=1 sólo adelanta el resumen, y como
// mucho una vez cada 30 s. Si algo falla (key mala, 429, caída) no se reintenta hasta dentro de 1 min.
// err: el primer fallo.
static void cache_refresh(const struct ra_session *s, int refresh, char *err, size_t en) {
  time_t now = time(NULL);
  char e[128];
  for(size_t i = 0; i < NITEMS; i++) {
    struct item *it = &items[i];
    if(now - it->at < it->ttl && !(refresh && i == 0 && now - it->at >= 30)) continue;
    struct buf b;
    long code = ra_api(it->api, s, it->extra, &b, e, sizeof e);
    if(code == 200 && b.d && (b.d[0] == '{' || b.d[0] == '[')) {
      free(it->body);
      it->body = b.d;
      it->at = now;
      continue;
    }
    free(b.d);
    it->at = now - it->ttl + 60;
    if(code == 401 || code == 403) snprintf(err, en, "badKey");
    else if(err[0]) continue;
    else if(code > 0) snprintf(err, en, "RetroAchievements: HTTP %ld", code);
    else snprintf(err, en, "RetroAchievements: %s", e);
  }
}

// Con cache_mx tomado. {"summary":…,"awards":…,"aotw":…,"wtp":…,"error":"…"} (lo libera quien llama).
static char *cache_json(const char *err) {
  char e[160];
  size_t len = 64 + sizeof e;
  for(size_t i = 0; i < NITEMS; i++) len += strlen(items[i].body ? items[i].body : "null") + 16;
  char *out = malloc(len), *p = out;
  if(!out) return NULL;
  *p++ = '{';
  for(size_t i = 0; i < NITEMS; i++) p += sprintf(p, "\"%s\":%s,", items[i].name, items[i].body ? items[i].body : "null");
  json_esc(e, sizeof e, err);
  sprintf(p, "\"error\":\"%s\"}", e);
  return out;
}

// ---------- Rutas: preparan la respuesta con el lock y la envían sin él ----------

static struct ra_session session_copy(void) {
  pthread_mutex_lock(&ses_mx);
  struct ra_session s = ses;
  pthread_mutex_unlock(&ses_mx);
  return s;
}

static void on_state(int fd) {
  char js[256];
  struct ra_session s = session_copy();
  snprintf(js, sizeof js, "{\"lang\":\"%s\",\"port\":%d,\"user\":\"%s\",\"key\":%s}", lang, PORT, s.user, s.key[0] ? "true" : "false");
  http_json(fd, 200, js);
}

static void on_overview(int fd, const struct req *r) {
  char err[128] = "";
  pthread_mutex_lock(&cache_mx);
  struct ra_session s = session_copy();
  char *js = NULL;
  if(s.user[0] && s.key[0]) {
    cache_refresh(&s, strstr(r->query, "refresh=1") != NULL, err, sizeof err);
    js = cache_json(err);
  }
  pthread_mutex_unlock(&cache_mx);
  if(!s.user[0] || !s.key[0]) http_error(fd, 409, "badKey");
  else if(js) http_json(fd, 200, js);
  else http_error(fd, 500, "sin memoria");
  free(js);
}

static void on_session(int fd, const struct req *r) {
  char js[256];
  if(!r->local) { http_error(fd, 403, "solo desde la PS5 (127.0.0.1)"); return; }
  struct ra_session s = session_copy();
  if(!s.user[0]) { http_error(fd, 404, "sin sesion"); return; }
  snprintf(js, sizeof js, "{\"user\":\"%s\",\"token\":\"%s\",\"key\":\"%s\"}", s.user, s.token, s.key);
  http_json(fd, 200, js);
  memset(js, 0, sizeof js);
}

// La consulta a RA va sin locks; sólo el cambio de sesión los toma.
static void on_login(int fd, const struct req *r) {
  char u[64] = "", p[256] = "", err[256] = "";
  struct ra_session s = { 0 };
  int ok = 0;
  if(!form_get(r->body, "u", u, sizeof u) || !form_get(r->body, "p", p, sizeof p) || !u[0] || !p[0]) snprintf(err, sizeof err, "missing");
  else if(!ra_login(u, p, &s, err, sizeof err)) ok = 1;
  memset(p, 0, sizeof p);
  if(!ok) { http_error(fd, 200, err); return; }
  pthread_mutex_lock(&cache_mx);
  pthread_mutex_lock(&ses_mx);
  ses = s;
  session_save();
  cache_drop();
  pthread_mutex_unlock(&ses_mx);
  pthread_mutex_unlock(&cache_mx);
  printf("[rahub] sesión de %s guardada\n", s.user);
  http_json(fd, 200, "{\"ok\":true}");
}

static void on_key(int fd, const struct req *r) {
  char k[128] = "";
  int ok;
  form_get(r->body, "k", k, sizeof k);
  pthread_mutex_lock(&cache_mx);
  pthread_mutex_lock(&ses_mx);
  if((ok = ses.user[0] != 0)) {
    sanitize(ses.key, sizeof ses.key, k);
    session_save();
    cache_drop();
  }
  pthread_mutex_unlock(&ses_mx);
  pthread_mutex_unlock(&cache_mx);
  if(ok) http_json(fd, 200, "{\"ok\":true}");
  else http_error(fd, 409, "sin sesion");
}

static void on_logout(int fd) {
  pthread_mutex_lock(&cache_mx);
  pthread_mutex_lock(&ses_mx);
  memset(&ses, 0, sizeof ses);
  unlink(SESSION_FILE);
  cache_drop();
  pthread_mutex_unlock(&ses_mx);
  pthread_mutex_unlock(&cache_mx);
  http_json(fd, 200, "{\"ok\":true}");
}

// Proxy para las apps de la PS5 (GB5): mandan aquí el cuerpo de su petición a dorequest.php
// (rcheevos) y RA Hub la pasa a RA por su HTTPS. Así la app no necesita TLS propio. Sólo 127.0.0.1.
static void on_dorequest(int fd, const struct req *r) {
  char err[256] = "";
  struct buf b;
  if(!r->local) { http_error(fd, 403, "solo desde la PS5 (127.0.0.1)"); return; }
  long code = fetch(RA_URL "/dorequest.php", r->body, &b, err, sizeof err);
  if(code < 0) http_error(fd, 502, err);
  else http_send(fd, (int)code, "application/json", b.d ? b.d : "", b.d ? b.n : 0);
  free(b.d);
}

// La imagen (badge) de un logro, para las apps de la PS5 que la quieren mostrar (GB5 la pinta en el
// juego). Sólo nombres de badge (dígitos, minúsculas, _): no es un proxy abierto. Sólo 127.0.0.1.
static void on_badge(int fd, const struct req *r) {
  char name[32] = "", url[128], err[256] = "";
  struct buf b;
  if(!r->local) { http_error(fd, 403, "solo desde la PS5 (127.0.0.1)"); return; }
  if(!form_get(r->query, "u", name, sizeof name) || !name[0]) { http_error(fd, 400, "falta u"); return; }
  for(const char *c = name; *c; c++)
    if(!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') || *c == '_')) { http_error(fd, 400, "badge invalido"); return; }
  snprintf(url, sizeof url, "https://media.retroachievements.org/Badge/%s.png", name);
  long code = fetch(url, NULL, &b, err, sizeof err);
  if(code < 0) http_error(fd, 502, err);
  else http_send(fd, (int)code, code == 200 ? "image/png" : "text/plain", b.d ? b.d : "", b.d ? b.n : 0);
  free(b.d);
}

// El archivo de cheats de GBA de libretro-database con ese nombre (GB5 lo pide desde el menú del
// juego para actualizar sus cheats). Sólo nombres de archivo .cht de esa carpeta. Sólo 127.0.0.1.
static void on_cheats(int fd, const struct req *r) {
  char name[200] = "", url[700], err[256] = "";
  struct buf b;
  size_t n;
  if(!r->local) { http_error(fd, 403, "solo desde la PS5 (127.0.0.1)"); return; }
  if(!form_get(r->query, "f", name, sizeof name) || (n = strlen(name)) < 5 || strcmp(name + n - 4, ".cht") ||
     strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) { http_error(fd, 400, "archivo invalido"); return; }
  char *e = curl_easy_escape(NULL, name, 0);
  if(!e) { http_error(fd, 500, "escape"); return; }
  snprintf(url, sizeof url, "https://raw.githubusercontent.com/libretro/libretro-database/master/cht/Nintendo%%20-%%20Game%%20Boy%%20Advance/%s", e);
  curl_free(e);
  long code = fetch(url, NULL, &b, err, sizeof err);
  if(code < 0) http_error(fd, 502, err);
  else http_send(fd, (int)code, "text/plain; charset=utf-8", b.d ? b.d : "", b.d ? b.n : 0);
  free(b.d);
}

static void route(int fd, const struct req *r) {
  if(http_asset(fd, r, assets, NASSETS) || lang_serve(fd, r)) return;
  if(req_is(r, "GET", "/api/state")) on_state(fd);
  else if(req_is(r, "GET", "/api/overview")) on_overview(fd, r);
  else if(req_is(r, "GET", "/session")) on_session(fd, r);
  else if(req_is(r, "POST", "/dorequest")) on_dorequest(fd, r);
  else if(req_is(r, "GET", "/badge")) on_badge(fd, r);
  else if(req_is(r, "GET", "/cheats")) on_cheats(fd, r);
  else if(req_is(r, "POST", "/login")) on_login(fd, r);
  else if(req_is(r, "POST", "/key")) on_key(fd, r);
  else if(req_is(r, "POST", "/logout")) on_logout(fd);
  else http_error(fd, 404, "no existe");
}

int main(void) {
  signal(SIGPIPE, SIG_IGN);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  mkdir(DATA_DIR, 0755);
  lang = lang_pick(ps5_locale());
  session_load();
  int s = http_listen(PORT);
  if(s < 0) return 1;
  printf("[rahub] escuchando en :%d (%s)%s%s\n", PORT, lang, ses.user[0] ? ", sesión de " : "", ses.user);
  ps5_install_webapp("RAHB00001", "RA Hub", PORT, icon_png, (size_t)(icon_png_end - icon_png));
  char msg[160], port[8];
  snprintf(port, sizeof port, "%d", PORT);
  lang_text(lang, "notify", msg, sizeof msg, "RA Hub", port);
  ps5_notify(msg);
  http_loop(s, route);
}
