// SPDX-License-Identifier: GPL-3.0-or-later
// Mete un archivo dentro del ELF. Rutas desde la raíz del repo (donde corre make).
// sym = inicio, sym_end = fin (sin contar el \0 que se añade detrás, útil para textos).
#pragma once

#define EMBED(sym, file)                                                                     \
  __asm__(".section .rodata\n" #sym ":\n.incbin \"" file "\"\n" #sym "_end:\n.byte 0\n.previous"); \
  extern const char sym[], sym##_end[]
