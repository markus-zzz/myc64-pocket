/*
 * Copyright (C) 2024 Markus Lavin (https://www.zzzconsulting.se/)
 *
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "bios.h"
#include "chars.h"
#include <stdarg.h>

#define OSD_DIM_X 256
#define OSD_DIM_Y 64

static void draw_char_bitmap(int x, int y, const unsigned char *bp,
                             int selected) {
  if (x + 8 >= OSD_DIM_X || y + 8 >= OSD_DIM_Y)
    return;
  // XXX: It would be more optimal to work at 32 bits at a time but then we
  // need to make the OSD pixshift register in RTL 32 bits as well. Otherwise
  // things get too weird!
  volatile uint8_t *osd_fb = (volatile uint8_t *)0x10000000;
  unsigned idx = y * OSD_DIM_X / 8 + x / 8;
  unsigned bit = x % 8;
  for (int i = 0; i < 8; i++) {
    uint8_t bitmap = bp[i];
    if (selected)
      bitmap = ~bitmap;
    uint8_t tmp = osd_fb[idx];
    tmp = (tmp & ~(0xff >> bit)) | (bitmap >> bit);
    osd_fb[idx] = tmp;
    if (bit > 0) {
      uint8_t tmp = osd_fb[idx + 1];
      unsigned bit2 = 8 - bit;
      tmp = (tmp & ~(0xff << bit2)) | (bitmap << bit2);
      osd_fb[idx + 1] = tmp;
    }
    idx += OSD_DIM_X / 8;
  }
}

static char to_upper(char c) {
  if ('a' <= c && c <= 'z')
    return c - ('a' - 'A');
  else
    return c;
}

unsigned osd_put_char(int x, int y, char c, int invert) {
  c = to_upper(c);
  const uint8_t *p = &chars_bin[(c & 0x3f) * 8];
  draw_char_bitmap(x, y, p, invert ? 1 : 0);
  return x + 8;
}

unsigned osd_put_str(int x, int y, const char *str, int invert) {
  while (*str != '\0') {
    osd_put_char(x, y, *str, invert);
    x += 8;
    str++;
  }
  return x;
}

unsigned osd_put_hex8(int x, int y, uint8_t val, int invert) {
  const char *digits = "0123456789ABCDEF";
  for (unsigned i = 0; i < 2; i++) {
    osd_put_char(x, y, digits[(val >> ((1 - i) * 4) & 0xf)], invert);
    x += 8;
  }
  return x;
}

unsigned osd_put_hex16(int x, int y, uint16_t val, int invert) {
  const char *digits = "0123456789ABCDEF";
  for (unsigned i = 0; i < 4; i++) {
    osd_put_char(x, y, digits[(val >> ((3 - i) * 4) & 0xf)], invert);
    x += 8;
  }
  return x;
}

void osd_clear() {
  volatile uint32_t *osd_fb = (volatile uint32_t *)0x10000000;
  for (int i = 0; i < OSD_DIM_X * OSD_DIM_Y / 32; i++) {
    osd_fb[i] = 0;
  }
}

void osd_printf(int x, int y, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  while (*fmt) {
    if (*fmt == '%') {
      int arg = va_arg(ap, int);
      fmt++;
      switch (*fmt++) {
      case 'c':
        x = osd_put_char(x, y, arg, 0);
        break;
      case 'x':
        x = osd_put_hex8(x, y, arg, 0);
        break;
      }
    } else {
      x = osd_put_char(x, y, *fmt++, 0);
    }
  }
  va_end(ap);
}
