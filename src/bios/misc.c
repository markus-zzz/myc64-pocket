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

static volatile int sel_row;

uint8_t joystick1;
uint8_t joystick2;

void misc_reset_core(uint8_t cart_type) {
  // Clear any "CBM80" cartridge auto start signature at $8000
  // XXX: Note that we currently cannot do that during reset since we depend on
  // the 1MHz phase enable signals during C64 RAM access.
  volatile uint8_t *C64_RAM = (volatile uint8_t *)0x50000000;
  for (unsigned i = 0; i < 16; i++) {
    C64_RAM[0x8000 + i] = 0;
  }
  *C64_CTRL = bits_set(*C64_CTRL, 0, 1, 0);     // Assert reset for MyC64
  *C64_CTRL = bits_set(*C64_CTRL, 5, 2, cart_type); // Set cartridge type
  *C64_CTRL = bits_set(*C64_CTRL, 0, 1, 1);     // Release reset for MyC64
}

void misc_init() { sel_row = 0; }

void misc_handle() {
  if (KEYB_POSEDGE(dpad_up)) {
    if (sel_row > 0)
      sel_row--;
  } else if (KEYB_POSEDGE(dpad_down)) {
    if (sel_row < 2)
      sel_row++;
  }

  if (KEYB_POSEDGE(face_a)) {
    switch (sel_row) {
    case 0:
      misc_reset_core(0); // Reset with no cartridge
      break;
    case 1:
      joystick1 = joystick1 < 2 ? joystick1 + 1 : 0;
      break;
    case 2:
      joystick2 = joystick2 < 2 ? joystick2 + 1 : 0;
      break;
    }
  }
}

void misc_draw() {
  IRQ_DISABLE();
  int sel_row_tmp = sel_row;
  IRQ_ENABLE();

  const char *inputs[] = {"N/C  ", "CONT1", "CONT2"};
  int offset;
  int x = 2, y = 12;
  osd_put_str(x, y, "RESET", sel_row_tmp == 0);
  y += 10;
  osd_put_str(x, y, "INPUTS", 0);
  x += 8;
  y += 10;
  offset = osd_put_str(x, y, "JOYSTICK1: ", 0);
  osd_put_str(offset, y, inputs[joystick1], sel_row_tmp == 1);
  y += 10;
  offset = osd_put_str(x, y, "JOYSTICK2: ", 0);
  osd_put_str(offset, y, inputs[joystick2], sel_row_tmp == 2);
}
