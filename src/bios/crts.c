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

uint16_t swap16(uint16_t x) { return (x << 8) | (x >> 8); }

uint32_t swap32(uint32_t x) {
  return ((uint32_t)swap16(x) << 16) | ((uint32_t)swap16(x >> 16));
}

static uint16_t get_ds_uint16(uint16_t slot_id, uint32_t offset) {
  volatile uint16_t *p = (volatile uint16_t *)0x70000000;
  *TARGET_20 = slot_id; // slot-id
  *TARGET_24 = offset;  // slot-offset
  *TARGET_28 = (uint32_t)p;
  *TARGET_2C = 2; // length
  *TARGET_0 = 0x636D0180;
  while ((*TARGET_0 >> 16) != 0x6F6B)
    ; // XXX: Maybe should read in one go and check the actual status as well.
  return *p;
}

static uint32_t get_ds_uint32(uint16_t slot_id, uint32_t offset) {
  volatile uint32_t *p = (volatile uint32_t *)0x70000000;
  *TARGET_20 = slot_id; // slot-id
  *TARGET_24 = offset;  // slot-offset
  *TARGET_28 = (uint32_t)p;
  *TARGET_2C = 4; // length
  *TARGET_0 = 0x636D0180;
  while ((*TARGET_0 >> 16) != 0x6F6B)
    ;
  return *p;
}
// XXX: Move to common and use elsewhere (same with the previous two)
static void ds_read(uint16_t slot_id, uint32_t slot_offset, uint32_t length,
                    uint8_t *dst) {
  const uint32_t buf_size = 256;

  while (length > 0) {
    uint32_t chunk_size = MIN(length, buf_size);
    *TARGET_20 = slot_id;
    *TARGET_24 = slot_offset;
    *TARGET_28 = 0x70000000;
    *TARGET_2C = chunk_size;
    *TARGET_0 = 0x636D0180;
    while ((*TARGET_0 >> 16) != 0x6F6B)
      ;

    volatile uint8_t *p = (volatile uint8_t *)0x70000000;
    for (uint32_t idx = 0; idx < chunk_size; idx++) {
      *dst++ = p[idx];
    }

    length -= chunk_size;
    slot_offset += chunk_size;
  }
}

static void load_crt(uint16_t slot_id) {
  uint8_t *ROM_LO = (uint8_t *)0x51000000;
  uint8_t *ROM_HI = (uint8_t *)0x52000000;
  uint32_t slot_length = get_ds_length(slot_id);
  if (!slot_length)
    return;

  uint16_t cart_hw_type = swap16(get_ds_uint16(slot_id, 0x16));
  if (cart_hw_type != 19 && cart_hw_type != 32) // Only Magic Desk and EasyFlash cartridges supported
    return;

  uint32_t chip_packet_base = 0x40;
  while (chip_packet_base < slot_length) {
    uint32_t signature = swap32(get_ds_uint32(slot_id, chip_packet_base));
    if (signature != 0x43484950) // Check for "CHIP" signature
      return;
    uint32_t chip_packet_length =
        swap32(get_ds_uint32(slot_id, chip_packet_base + 0x4));

    uint16_t bank_number =
        swap16(get_ds_uint16(slot_id, chip_packet_base + 0xa));
    uint16_t load_address =
        swap16(get_ds_uint16(slot_id, chip_packet_base + 0xc));
    uint16_t image_size =
        swap16(get_ds_uint16(slot_id, chip_packet_base + 0xe));

    uint8_t *ROM = (load_address == 0x8000) ? ROM_LO : ROM_HI;
    ds_read(slot_id, chip_packet_base + 0x10, image_size,
            &ROM[bank_number * image_size]);

    chip_packet_base += chip_packet_length;
  }

  switch (cart_hw_type) {
    case 19: // Magic Desk
      misc_reset_core(/*EXROM=*/0, /*GAME=*/1); // Reset C64 and 1541
      break;
    case 32: // Magic Desk (set Ultimax mode)
      misc_reset_core(/*EXROM=*/1, /*GAME=*/0); // Reset C64 and 1541
      break;
  }
}

void crts_init() {}

void crts_irq() {
  if (updated_slots & (1 << CRT_SLOT_ID)) {
    load_crt(CRT_SLOT_ID);
  }
}
