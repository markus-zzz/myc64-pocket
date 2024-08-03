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

static void load_crt(uint16_t slot_id) {
  uint8_t *ROM_LO = (uint8_t *)0x51000000;
  uint8_t *ROM_HI = ROM_LO + (1 << 19);
  uint32_t slot_length = bridge_ds_get_length(slot_id);
  if (!slot_length)
    return;

  uint8_t crt_type = 0;
  uint16_t cart_hw_type = swap16(bridge_ds_get_uint16(slot_id, 0x16));
  switch (cart_hw_type) {
  case 19: // Magic Desk
    crt_type = 1;
    break;
  case 32: // Easy Flash
    crt_type = 2;
    break;
  case 1: // Action Replay
    crt_type = 3;
    break;
  default: // Unsupported format
    return;
  }

  uint32_t chip_packet_base = 0x40;
  while (chip_packet_base < slot_length) {
    uint32_t signature =
        swap32(bridge_ds_get_uint32(slot_id, chip_packet_base));
    if (signature != 0x43484950) // Check for "CHIP" signature
      return;
    uint32_t chip_packet_length =
        swap32(bridge_ds_get_uint32(slot_id, chip_packet_base + 0x4));

    uint16_t bank_number =
        swap16(bridge_ds_get_uint16(slot_id, chip_packet_base + 0xa));
    uint16_t load_address =
        swap16(bridge_ds_get_uint16(slot_id, chip_packet_base + 0xc));
    uint16_t image_size =
        swap16(bridge_ds_get_uint16(slot_id, chip_packet_base + 0xe));

    uint8_t *ROM = (load_address == 0x8000) ? ROM_LO : ROM_HI;
    bridge_ds_read(slot_id, chip_packet_base + 0x10, image_size,
                   &ROM[bank_number * image_size]);

    chip_packet_base += chip_packet_length;
  }

  misc_reset_core(crt_type); // Reset C64 and 1541
}

void crts_irq() {
  if (updated_slots & (1 << CRT_SLOT_ID)) {
    load_crt(CRT_SLOT_ID);
  }
}
