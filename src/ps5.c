// SPDX-License-Identifier: GPL-3.0-or-later
#include "ps5.h"
#include "util.h"
#include <ps5/kernel.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int sceSystemServiceParamGetInt(int param_id, int *value); // 1 = idioma del sistema
int sceUserServiceInitialize(const void *params);
int sceUserServiceGetForegroundUser(int32_t *user_id);
int sceUserServiceGetUserName(int32_t user_id, char *name, size_t size);
int sceAppInstUtilInitialize(void);
int sceAppInstUtilTerminate(void);
int sceAppInstUtilAppInstallAll(void *);
// Notificación del sistema (mismo struct que ps5-payload-manager).
typedef struct { char useless[45]; char message[3075]; } notify_request_t;
int sceKernelSendNotificationRequest(int device, notify_request_t *req, size_t size, int unused);

const char *ps5_locale(void) {
  // Índice = SCE_SYSTEM_PARAM_LANG_*.
  static const char *tags[] = { "ja-JP", "en-US", "fr-FR", "es-ES", "de-DE", "it-IT", "nl-NL", "pt-PT", "ru-RU", "ko-KR",
                                "zh-TW", "zh-CN", "fi-FI", "sv-SE", "da-DK", "nb-NO", "pl-PL", "pt-BR", "en-GB", "tr-TR",
                                "es-419", "ar", "fr-CA", "cs-CZ", "hu-HU", "el-GR", "ro-RO", "th-TH", "vi-VN", "id-ID", "uk-UA" };
  int v = -1;
  if(sceSystemServiceParamGetInt(1, &v) || v < 0 || v >= (int)(sizeof tags / sizeof *tags)) return "en-US";
  return tags[v];
}

uint32_t ps5_user(char *name, size_t n) {
  static int init;
  int32_t u = -1;
  if(!init) init = 1, sceUserServiceInitialize(NULL); // ponytail: dos hilos a la vez sólo lo llaman dos veces, sin daño
  if(sceUserServiceGetForegroundUser(&u) || u == -1) u = -1;
  if(name && n) {
    name[0] = 0;
    if(u != -1 && sceUserServiceGetUserName(u, name, n)) name[0] = 0;
    for(char *c = name; *c; c++) if(*c == '"' || *c == '\\' || (unsigned char)*c < 32) *c = '?'; // va dentro de JSON
  }
  return (uint32_t)u;
}

void ps5_notify(const char *msg) {
  notify_request_t nr = { 0 };
  snprintf(nr.message, sizeof nr.message, "%s", msg);
  sceKernelSendNotificationRequest(0, &nr, sizeof nr, 0);
}

// Una "web app" del sistema (categoría 65536): el ícono abre deeplinkUri en el navegador de la PS5.
// Mismo método que Payload Manager; sólo se reinstala si param.json o icon0.png cambiaron.
void ps5_install_webapp(const char *title_id, const char *name, int port, const char *icon, size_t icon_n) {
  char dir[64], sys[80], param_path[96], icon_path[96], param[512];
  int (*install_dir)(const char *, const char *, int) = NULL;
  uint32_t h;
  snprintf(dir, sizeof dir, "/user/app/%s", title_id);
  snprintf(sys, sizeof sys, "%s/sce_sys", dir);
  snprintf(param_path, sizeof param_path, "%s/param.json", sys);
  snprintf(icon_path, sizeof icon_path, "%s/icon0.png", sys);
  int pn = snprintf(param, sizeof param,
                    "{\"titleId\":\"%s\",\"applicationCategoryType\":65536,\"deeplinkUri\":\"http://127.0.0.1:%d/\","
                    "\"localizedParameters\":{\"defaultLanguage\":\"en-US\",\"en-US\":{\"titleName\":\"%s\"}}}",
                    title_id, port, name);
  if(file_equals(param_path, param, pn) && file_equals(icon_path, icon, icon_n)) return;
  mkdir("/user/app", 0755);
  mkdir(dir, 0755);
  mkdir(sys, 0755);
  if(file_write(icon_path, icon, icon_n) || file_write(param_path, param, pn)) {
    printf("[rahub] no se pudo escribir %s\n", sys);
    return;
  }
  int r = sceAppInstUtilInitialize();
  if(!r) {
    // sceAppInstUtilAppInstallTitleDir no está en el SDK: se busca por NID, como Payload Manager.
    if(!kernel_dynlib_handle(getpid(), "libSceAppInstUtil.sprx", &h)) install_dir = (void *)kernel_dynlib_resolve(getpid(), h, "Wudg3Xe3heE");
    r = install_dir ? install_dir(title_id, "/user/app/", 0) : sceAppInstUtilAppInstallAll(NULL);
    sceAppInstUtilTerminate();
  }
  if(r) unlink(param_path); // así se reintenta en el próximo arranque
  printf("[rahub] ícono %s en Multimedia: %s (0x%x)\n", title_id, r ? "falló" : "instalado", r);
}
