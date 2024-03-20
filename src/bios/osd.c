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

#define OSD_DIM_X 256
#define OSD_DIM_Y 64

static void set_pixel(unsigned x, unsigned y) {
  if (x >= OSD_DIM_X || y >= OSD_DIM_Y)
    return;
  volatile unsigned char *p = (volatile unsigned char *)0x10000000;
  unsigned idx = y * OSD_DIM_X / 8 + x / 8;
  unsigned bit = 7 - x % 8;
  p[idx] |= 1 << bit;
}

static void clr_pixel(unsigned x, unsigned y) {
  if (x >= OSD_DIM_X || y >= OSD_DIM_Y)
    return;
  volatile unsigned char *p = (volatile unsigned char *)0x10000000;
  unsigned idx = y * OSD_DIM_X / 8 + x / 8;
  unsigned bit = 7 - x % 8;
  p[idx] &= ~(1 << bit);
}

static void draw_char_bitmap(int x, int y, const unsigned char *p,
                             int selected) {
  for (int i = 0; i < 8; i++) {
    char q = p[i];
    for (int j = 0; j < 8; j++) {
      if (((q >> (7 - j)) & 1) != selected)
        set_pixel(x + j, y + i);
      else
        clr_pixel(x + j, y + i);
    }
  }
}

static char to_upper(char c) {
  if ('a' <= c && c <= 'z')
    return c - ('a' - 'A');
  else
    return c;
}

void osd_put_char(int x, int y, char c, int invert) {
  c = to_upper(c);
  const uint8_t *p = &chars_bin[(c & 0x3f) * 8];
  draw_char_bitmap(x, y, p, invert ? 1 : 0);
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
  volatile unsigned char *p = (volatile unsigned char *)0x10000000;
  for (int i = 0; i < OSD_DIM_X * OSD_DIM_Y / 8; i++) {
    p[i] = 0;
  }
}
