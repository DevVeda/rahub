# RA Hub para PS5.
#   make                compila build/rahub.elf y build/rahub-test.elf
#   make test           prueba la página del hub (node, en/es)
#   make install        sube el hub a Payload Manager (payloads locales), para lanzarlo desde ahí
#   make install-test   lo mismo con "RA Hub Test" (¿ve una app la sesión del hub?)
#   make run            manda el hub directo al elfldr (puerto 9021), sin instalar
# Funciona en macOS y Linux (en Windows, con WSL). Cada ruta se puede pasar a mano: make PS5_PAYLOAD_SDK=… CACERT=…
# SDK: el de la variable de entorno, o el primero que exista.
PS5_PAYLOAD_SDK ?= $(firstword $(wildcard $(HOME)/ps5-payload-dev/opt/ps5-payload-sdk /opt/ps5-payload-sdk))
# LLVM: en macOS el de Homebrew; en Linux el SDK encuentra solo llvm-config-18 (o el más nuevo que haya).
LLVM_CONFIG     ?= $(firstword $(wildcard /opt/homebrew/opt/llvm@18/bin/llvm-config /usr/local/opt/llvm@18/bin/llvm-config))
export LLVM_CONFIG
ifeq ($(PS5_PAYLOAD_SDK),)
$(error No encuentro el ps5-payload-sdk: make PS5_PAYLOAD_SDK=/ruta/al/sdk)
endif
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

# IP de la PS5 para install/run: make install PS5_HOST=…, o en config.mk (fuera de git): PS5_HOST := 192.168.x.x
-include config.mk
need-host = $(if $(PS5_HOST),,$(error Falta la IP de la PS5: make $@ PS5_HOST=<ip> o ponla en config.mk))
.DEFAULT_GOAL := all
CFLAGS  := -Wall -O2 -Ibuild `$(PKG_CONFIG) --cflags libcurl`
LDLIBS  := `$(PKG_CONFIG) --libs --static libcurl openssl` -lSceSystemService -lSceAppInstUtil -lSceUserService
COMMON  := src/http.c src/ra.c src/ps5.c src/util.c src/lang.c
HEADERS := $(wildcard src/*.h)
LANGS   := $(wildcard web/lang/*.json)
SHARED  := build/cacert.pem build/langs.h $(LANGS) web/common.css web/lib.js

all: build/rahub.elf build/rahub-test.elf

build/rahub.elf: src/hub.c $(COMMON) $(HEADERS) $(SHARED) web/hub.html web/hub.css web/hub.js assets/icon0.png
	$(CC) $(CFLAGS) src/hub.c $(COMMON) -o $@ $(LDLIBS)

build/rahub-test.elf: src/test_app.c $(COMMON) $(HEADERS) $(SHARED) web/test.html web/test.js assets/icon_test.png
	$(CC) $(CFLAGS) src/test_app.c $(COMMON) -o $@ $(LDLIBS)

# Idiomas: cada web/lang/<código>.json va dentro del ELF (ver src/lang.h). Para añadir uno basta con el archivo.
build/langs.h: $(LANGS)
	mkdir -p build && i=0 && for f in $(LANGS); do echo "LANG(lang_$$i, \"$$(basename $$f .json)\", \"$$f\")"; i=$$((i+1)); done > $@

# Certificados raíz del sistema (van dentro del ELF, ver src/ra.c): el primero que exista
# (macOS/Alpine, Debian/Ubuntu/Arch, Fedora/RHEL, openSUSE). Si no hay: make CACERT=… (p. ej. https://curl.se/ca/cacert.pem).
CACERT ?= $(firstword $(wildcard /etc/ssl/cert.pem /etc/ssl/certs/ca-certificates.crt /etc/pki/tls/certs/ca-bundle.crt /etc/ssl/ca-bundle.pem))
build/cacert.pem:
	@[ -n "$(CACERT)" ] || { echo "No encuentro certificados raíz: make CACERT=/ruta/a/cacert.pem"; exit 1; }
	mkdir -p build && cp $(CACERT) $@

test:
	node tests/page.test.mjs en && node tests/page.test.mjs es

run: build/rahub.elf
	$(need-host)
	nc -w 1 $(PS5_HOST) 9021 < $<

# Payload Manager (itsPLK): un payload local por subcarpeta (local/<nombre>/<archivo>.elf; meta.json opcional).
PLDMGR := ftp://$(PS5_HOST):2121/data/pldmgr/payloads/local
UPLOAD := curl -sS --fail --ftp-create-dirs --disable-epsv --connect-timeout 5 -T
install: build/rahub.elf
	$(need-host)
	$(UPLOAD) $< $(PLDMGR)/rahub/rahub.elf

install-test: build/rahub-test.elf
	$(need-host)
	$(UPLOAD) $< $(PLDMGR)/rahub-test/rahub-test.elf

clean:
	rm -rf build

.PHONY: all test run install install-test clean
