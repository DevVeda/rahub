// SPDX-License-Identifier: GPL-3.0-or-later
#include "http.h"
#include "util.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 32
#define REQ_SECONDS 10
#define IO_SECONDS 5

static http_handler app;
static int clients; // conexiones abiertas (atómico)

static const char *reason(int status) {
  switch(status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 409: return "Conflict";
    default: return "Internal Server Error";
  }
}

// send() puede escribir menos de lo pedido; con SO_SNDTIMEO, un cliente que no lee no bloquea para siempre.
static void send_all(int fd, const char *p, size_t n) {
  while(n) {
    ssize_t w = send(fd, p, n, 0);
    if(w <= 0) return;
    p += w;
    n -= (size_t)w;
  }
}

void http_send(int fd, int status, const char *type, const char *body, size_t n) {
  char h[256];
  int hn = snprintf(h, sizeof h, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
                    status, reason(status), type, n);
  send_all(fd, h, (size_t)hn);
  send_all(fd, body, n);
}

void http_json(int fd, int status, const char *json) {
  http_send(fd, status, "application/json", json, strlen(json));
}

void http_error(int fd, int status, const char *msg) {
  char e[300], js[340];
  json_esc(e, sizeof e, msg);
  snprintf(js, sizeof js, "{\"error\":\"%s\"}", e);
  http_json(fd, status, js);
}

int req_is(const struct req *r, const char *method, const char *path) {
  return !strcmp(r->method, method) && !strcmp(r->path, path);
}

int http_asset(int fd, const struct req *r, const struct asset *a, size_t n) {
  for(size_t i = 0; i < n; i++)
    if(req_is(r, "GET", a[i].path)) {
      http_send(fd, 200, a[i].type, a[i].data, (size_t)(a[i].end - a[i].data));
      return 1;
    }
  return 0;
}

// Host aceptado: sin Host (HTTP/1.0), "localhost" o una IPv4. Un nombre de dominio = DNS rebinding.
static int host_ok(const char *req, const char *body) {
  const char *h = strcasestr(req, "\r\nHost:");
  char host[64] = "";
  struct in_addr a;
  if(!h || h > body) return 1;
  for(h += 7; *h == ' '; h++) {}
  size_t n = strcspn(h, ":\r\n");
  if(n == 0 || n >= sizeof host) return 0;
  memcpy(host, h, n);
  return !strcasecmp(host, "localhost") || inet_pton(AF_INET, host, &a) == 1;
}

static void handle(int fd, int local) {
  time_t start = time(NULL);
  char buf[8192], method[8] = "", path[128] = "";
  size_t n = 0;
  char *body = NULL;
  long want = 0;
  for(;;) { // cabeceras + cuerpo según Content-Length
    ssize_t r = recv(fd, buf + n, sizeof buf - 1 - n, 0);
    if(r <= 0) break;
    buf[n += r] = 0;
    if(!body && (body = strstr(buf, "\r\n\r\n"))) {
      body += 4;
      char *cl = strcasestr(buf, "\r\nContent-Length:");
      want = cl && cl < body ? strtol(cl + 17, NULL, 10) : 0;
    }
    if(body && (long)(buf + n - body) >= want) break;
    if(n == sizeof buf - 1 || time(NULL) - start > REQ_SECONDS) break;
  }
  if(!body) return;
  sscanf(buf, "%7s %127s", method, path);
  char *q = strchr(path, '?');
  if(q) *q++ = 0;
  char *csrf = strcasestr(buf, "\r\nX-RAHub:");
  char ua[128] = "", *u = strcasestr(buf, "\r\nUser-Agent:");
  if(u && u < body) {
    u += 13;
    while(*u == ' ') u++;
    size_t k = 0;
    while(u[k] && u[k] != '\r' && k < sizeof ua - 1) { ua[k] = u[k]; k++; }
    ua[k] = 0;
  }
  struct req r = { method, path, q ? q : "", body, local, ua };

  if(!host_ok(buf, body)) http_error(fd, 403, "Host no permitido");
  else if(!strcmp(method, "POST") && !(csrf && csrf < body)) http_error(fd, 403, "falta X-RAHub");
  else if(req_is(&r, "POST", "/quit") && local) {
    http_json(fd, 200, "{\"ok\":true}");
    printf("[rahub] cerrado por una versión nueva\n");
    exit(0);
  }
  else app(fd, &r);
  memset(buf, 0, sizeof buf); // puede llevar una contraseña
}

// Un hilo por conexión: un navegador que abre sockets y no manda nada no bloquea a los demás.
static void *client(void *arg) {
  long v = (long)arg;
  int fd = (int)(v >> 1);
  struct timeval tv = { IO_SECONDS, 0 };
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
  handle(fd, (int)(v & 1));
  close(fd);
  __atomic_sub_fetch(&clients, 1, __ATOMIC_SEQ_CST);
  return NULL;
}

int http_listen(int port) {
  static const char quit[] = "POST /quit HTTP/1.0\r\nX-RAHub: 1\r\n\r\n";
  struct sockaddr_in any = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_ANY) };
  struct sockaddr_in lo = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
  int s = socket(AF_INET, SOCK_STREAM, 0), one = 1;
  if(s < 0) return -1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  int bound = !bind(s, (struct sockaddr *)&any, sizeof any);
  for(int i = 0; !bound && i < 10; i++) { // ocupado = otra copia corriendo: que salga
    int c = socket(AF_INET, SOCK_STREAM, 0);
    if(!connect(c, (struct sockaddr *)&lo, sizeof lo)) send(c, quit, sizeof quit - 1, 0);
    close(c);
    sleep(1);
    bound = !bind(s, (struct sockaddr *)&any, sizeof any);
  }
  if(!bound || listen(s, 8)) {
    printf("[rahub] no se pudo abrir el puerto %d\n", port);
    close(s);
    return -1;
  }
  return s;
}

void http_loop(int s, http_handler h) {
  pthread_attr_t at;
  app = h;
  pthread_attr_init(&at);
  pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
  for(;;) {
    struct sockaddr_in peer;
    socklen_t pl = sizeof peer;
    pthread_t t;
    int c = accept(s, (struct sockaddr *)&peer, &pl);
    if(c < 0) continue;
    if(__atomic_add_fetch(&clients, 1, __ATOMIC_SEQ_CST) > MAX_CLIENTS) { // lleno: se corta sin más
      close(c);
      __atomic_sub_fetch(&clients, 1, __ATOMIC_SEQ_CST);
      continue;
    }
    long v = (long)c << 1 | (peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    if(pthread_create(&t, &at, client, (void *)v)) {
      close(c);
      __atomic_sub_fetch(&clients, 1, __ATOMIC_SEQ_CST);
    }
  }
}
