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

static void load_prg(uint16_t slot_id) {
  volatile uint8_t *RAM = (volatile uint8_t *)0x50000000;
  uint16_t slot_length = bridge_ds_get_length(slot_id);
  if (!slot_length)
    return;

  // First load the 16 bit header with PrgStartAddr
  uint16_t PrgSize = slot_length - 2;
  uint16_t PrgStartAddr = bridge_ds_get_uint16(slot_id, 0);

  // Update various zero page pointers to adjust for loaded program.
  // - Pointer to beginning of variable area. (End of program plus 1.)
  // - Pointer to beginning of array variable area.
  // - Pointer to end of array variable area.
  // - Load address read from input file and pointer to current byte during
  // LOAD/VERIFY from serial bus.
  //   End address after LOAD/VERIFY from serial bus or datasette.
  // For details see https://sta.c64.org/cbm64mem.html and
  // VICE source: src/c64/c64mem.c:mem_set_basic_text()
  uint16_t PrgEndAddr = PrgStartAddr + PrgSize;
#define RAM_W16(addr, val)                                                     \
  RAM[addr] = (val)&0xff;                                                      \
  RAM[(addr) + 1] = (val) >> 8;
  RAM_W16(0x2d, PrgEndAddr);
  RAM_W16(0x2f, PrgEndAddr);
  RAM_W16(0x31, PrgEndAddr);
  RAM_W16(0xae, PrgEndAddr);

  // Finally load the .prg contents into C64 RAM
  bridge_ds_read(slot_id, 2, slot_length - 2, (uint8_t *)&RAM[PrgStartAddr]);
}

static enum {
  IS_IDLE,
  IS_WAIT_BOOT,
  IS_KEY_R,
  IS_KEY_U,
  IS_KEY_N,
  IS_KEY_RET
} inject_state = IS_IDLE;
static uint32_t inject_wait;

void prgs_irq() {
  switch (inject_state) {
  case IS_IDLE:
    c64_isr_keyb_mask = 0;
    if (updated_slots & (1 << PRG_SLOT_ID)) {
      misc_reset_core(0); // Reset C64 and 1541
      inject_wait = timer_ticks + 300;
      inject_state = IS_WAIT_BOOT;
    }
    break;
  case IS_WAIT_BOOT:
    if (timer_ticks >= inject_wait) {
      load_prg(PRG_SLOT_ID);
      inject_wait = timer_ticks + 40;
      inject_state = IS_KEY_R;
    }
    break;
  case IS_KEY_R:
    c64_isr_keyb_mask = C64_KEYB_MASK_KEY(0x21); // R
    if (timer_ticks >= inject_wait) {
      inject_wait = timer_ticks + 20;
      inject_state = IS_KEY_U;
    }
    break;
  case IS_KEY_U:
    c64_isr_keyb_mask = C64_KEYB_MASK_KEY(0x36); // U
    if (timer_ticks >= inject_wait) {
      inject_wait = timer_ticks + 20;
      inject_state = IS_KEY_N;
    }
    break;
  case IS_KEY_N:
    c64_isr_keyb_mask = C64_KEYB_MASK_KEY(0x47); // N
    if (timer_ticks >= inject_wait) {
      inject_wait = timer_ticks + 20;
      inject_state = IS_KEY_RET;
    }
    break;
  case IS_KEY_RET:
    c64_isr_keyb_mask = C64_KEYB_MASK_KEY(0x01); // <RET>
    if (timer_ticks >= inject_wait) {
      inject_wait = timer_ticks + 20;
      inject_state = IS_IDLE;
    }
    break;
  }
}
