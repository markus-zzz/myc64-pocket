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

struct entry {
  const char *str;
  uint8_t ports;
};

static int sel_row;
static int sel_col;
static uint64_t sticky_keys;

const struct entry row_0[] = {
    {"F1", 0x04},   {"\x1f", 0x71}, {"1", 0x70}, {"2", 0x73}, {"3", 0x10},
    {"4", 0x13},    {"5", 0x20},    {"6", 0x23}, {"7", 0x30}, {"8", 0x33},
    {"9", 0x40},    {"0", 0x43},    {"+", 0x50}, {"-", 0x53}, {"\x1c", 0x60},
    {"HOME", 0x63}, {"DEL", 0x00},  {NULL}};
const struct entry row_1[] = {
    {"F3", 0x05},  {"CTRL", 0x72}, {"Q", 0x76}, {"W", 0x11}, {"E", 0x16},
    {"R", 0x21},   {"T", 0x26},    {"Y", 0x31}, {"U", 0x36}, {"I", 0x41},
    {"O", 0x46},   {"P", 0x51},    {"@", 0x56}, {"*", 0x61}, {"\x1e", 0x66},
    {"RES", 0xff}, {NULL}};
const struct entry row_2[] = {
    {"F5", 0x06}, {"STOP", 0x77}, {"A", 0x12},   {"S", 0x15},
    {"D", 0x22},  {"F", 0x25},    {"G", 0x32},   {"H", 0x35},
    {"J", 0x42},  {"K", 0x45},    {"L", 0x52},   {";", 0x62},
    {":", 0x55},  {"=", 0x65},    {"RET", 0x01}, {NULL}};
const struct entry row_3[] = {
    {"F7", 0x03}, {"C=", 0x75}, {"SHFT", 0x17}, {"Z", 0x14},    {"X", 0x27},
    {"C", 0x24},  {"V", 0x37},  {"B", 0x34},    {"N", 0x47},    {"M", 0x44},
    {",", 0x57},  {".", 0x54},  {"/", 0x67},    {"SHFT", 0x64}, {"DN", 0x07},
    {"RT", 0x02}, {NULL}};
const struct entry row_4[] = {{"SPACE", 0x74}, {NULL}};

const struct entry *const rows[] = {row_0, row_1, row_2, row_3, row_4};

unsigned row_length(unsigned row_idx) {
  const struct entry *p = rows[row_idx];
  unsigned len = 0;
  while (p->str) {
    len++;
    p++;
  }
  return len;
}

void keyboard_virt_init() {
  sel_row = 0;
  sel_col = 0;
  sticky_keys = 0;
}

void keyboard_virt_handle() {
  if (KEYB_POSEDGE(dpad_up)) {
    if (sel_row > 0)
      sel_row--;
    sel_col = MIN(sel_col, row_length(sel_row) - 1);
  } else if (KEYB_POSEDGE(dpad_down)) {
    if (sel_row < sizeof(rows) / sizeof(rows[0]) - 1)
      sel_row++;
    sel_col = MIN(sel_col, row_length(sel_row) - 1);
  } else if (KEYB_POSEDGE(dpad_left)) {
    if (sel_col > 0)
      sel_col--;
  } else if (KEYB_POSEDGE(dpad_right)) {
    sel_col++;
    sel_col = MIN(sel_col, row_length(sel_row) - 1);
  }

  const struct entry *row = rows[sel_row];
  const struct entry *e = &row[sel_col];
  if (KEYB_DOWN(face_a)) {
    c64_keyb_mask |= C64_KEYB_MASK_KEY(e->ports);
  }

  if (KEYB_POSEDGE(face_x)) {
    sticky_keys ^= C64_KEYB_MASK_KEY(e->ports);
  } else if (KEYB_POSEDGE(face_y)) {
    sticky_keys = 0;
  }

  c64_keyb_mask |= sticky_keys;
}

void keyboard_virt_draw(void) {
  // Draw keyboard to OSD buffer
  for (int i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
    const struct entry *row = rows[i];
    int offset = 0;
    for (int j = 0; row[j].str != NULL; j++) {
      const struct entry *e = &row[j];
      const char *str = e->str;
      for (int k = 0; str[k] != '\0'; k++) {
        int invert = (sel_row == i && sel_col == j) ||
                     (sticky_keys & C64_KEYB_MASK_KEY(e->ports));
        osd_put_char(offset, 10 + 2 + i * 10, str[k], invert);
        offset += 8;
      }
      offset += 2;
    }
  }
}
