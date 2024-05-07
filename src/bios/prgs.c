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

#define PRG_SLOT_ID 0

static uint32_t get_ds_length(uint16_t slot_id) {
  volatile uint32_t *p = BRIDGE_DS_TABLE;

  for (unsigned idx = 0; idx < 32; idx++) {
    uint16_t ds_slot_id = p[idx * 2 + 0] & 0xffff;
    uint32_t ds_length = p[idx * 2 + 1];
    if (ds_slot_id == slot_id)
      return ds_length;
  }
  return 0;
}

static void load_prg(uint16_t slot_id) {
  volatile uint8_t *p = (volatile uint8_t *)0x70000000;
  volatile uint8_t *RAM = (volatile uint8_t *)0x50000000;
  uint16_t slot_length = get_ds_length(slot_id);
  if (!slot_length)
    return;

  // First load the 16 bit header with PrgStartAddr
  *TARGET_20 = slot_id; // slot-id
  *TARGET_24 = 0;       // slot-offset
  *TARGET_28 = 0x70000000;
  *TARGET_2C = 2; // length
  *TARGET_0 = 0x636D0180;
  while ((*TARGET_0 >> 16) != 0x6F6B)
    ;
  uint16_t PrgSize = slot_length - 2;
  uint16_t PrgStartAddr = (p[1] << 8) | p[0];

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

  volatile uint8_t *q = &RAM[PrgStartAddr];

  const uint32_t buf_size = 256;
  uint32_t slot_offset = 2;
  uint32_t idx = 0;

  while (slot_offset < slot_length) {
    uint32_t chunk_size = MIN(slot_length - slot_offset, buf_size);
    *TARGET_20 = slot_id;
    *TARGET_24 = slot_offset;
    *TARGET_28 = 0x70000000;
    *TARGET_2C = chunk_size;
    *TARGET_0 = 0x636D0180;
    while ((*TARGET_0 >> 16) != 0x6F6B)
      ;

    // Write into C64 RAM
    for (uint32_t i = 0; i < chunk_size; i++) {
      if (idx < PrgSize)
        q[idx++] = p[i];
    }

    slot_offset += chunk_size;
  }
}

static int scroll_offset;
static int scroll_dir;
static uint32_t ticks_next_update;
static char prg_path[64];

static unsigned strlen(const char *p) {
  unsigned len = 0;
  while (*p++ != '\0')
    len++;
  return len;
}

void prgs_init() {
  scroll_offset = 0;
  scroll_dir = 1;
  ticks_next_update = 0;
  prg_path[0] = '\0';
}

void prgs_irq() {
  if (updated_slots & (1 << PRG_SLOT_ID)) {
    while ((*TARGET_0 >> 16) != 0x6F6B)
      ;
    volatile uint8_t *p = (volatile uint8_t *)0x70000000;
    *TARGET_20 = PRG_SLOT_ID; // slot-id
    *TARGET_24 = 0x70000000;
    *TARGET_0 = 0x636D0190; // Get filename of data slot
    while ((*TARGET_0 >> 16) != 0x6F6B)
      ;

    unsigned off = MAX((int)strlen((const char *)p) + 1 - 64, 0);
    for (unsigned i = 0; i < 64; i++) {
      prg_path[i] = p[off + i];
    }
  }
}

void prgs_handle() {
  if (KEYB_POSEDGE(face_a)) {
    load_prg(PRG_SLOT_ID);
  }
}

void prgs_draw() {
  osd_put_str(2, 20, "INJECT PRG SLOT", 0);
  unsigned offset = osd_put_str(10, 30, "#0:", 0);

  const char *q = prg_path;
  const int disp_len = 26;
  int len = strlen(q);
  for (int i = 0; i < disp_len; i++) {
    int idx = scroll_offset + i;
    osd_put_char(offset, 30, idx < len ? q[idx] : ' ', 1);
    offset += 8;
  }
  if (timer_ticks >= ticks_next_update) {
    ticks_next_update = timer_ticks + 10;
    if (scroll_dir > 0) {
      if (scroll_offset + disp_len < len) {
        scroll_offset++;
      } else {
        scroll_dir = -1;
      }
    } else if (scroll_dir < 0) {
      if (scroll_offset > 0) {
        scroll_offset--;
      } else {
        scroll_dir = 1;
      }
    }
  }
}
