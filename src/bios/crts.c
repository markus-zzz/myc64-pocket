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

struct crt_chip {
  uint32_t packet;
  uint16_t kind, bank, address, size;
};

static uint8_t read_chip(uint16_t slot_id, uint32_t offset, uint32_t length,
                         struct crt_chip *chip) {
  if (offset > length || length - offset < 0x10 ||
      swap32(bridge_ds_get_uint32(slot_id, offset)) != 0x43484950)
    return 0;
  chip->packet = swap32(bridge_ds_get_uint32(slot_id, offset + 4));
  chip->kind = swap16(bridge_ds_get_uint16(slot_id, offset + 8));
  chip->bank = swap16(bridge_ds_get_uint16(slot_id, offset + 10));
  chip->address = swap16(bridge_ds_get_uint16(slot_id, offset + 12));
  chip->size = swap16(bridge_ds_get_uint16(slot_id, offset + 14));
  return chip->packet >= 0x10 && chip->packet <= length - offset &&
         chip->size && chip->size <= chip->packet - 0x10;
}

static uint8_t generic_config(uint16_t slot_id, uint32_t length) {
  uint32_t offset = 0x40;
  uint8_t windows = 0, mode = 0;
  while (offset < length) {
    struct crt_chip chip;
    if (!read_chip(slot_id, offset, length, &chip) || chip.kind || chip.bank)
      return 0;
    if (chip.address == 0x8000 &&
        (chip.size == 0x2000 || chip.size == 0x4000)) {
      uint8_t used = chip.size == 0x4000 ? 3 : 1;
      if (windows & used)
        return 0;
      windows |= used;
    } else if ((chip.address == 0xa000 || chip.address == 0xe000) &&
               chip.size == 0x2000) {
      if (windows & 2)
        return 0;
      windows |= 2;
      mode = chip.address == 0xe000 ? 7 : 6;
    } else {
      return 0;
    }
    offset += chip.packet;
  }
  // Packet layout, not potentially misleading GAME bytes, determines the mode.
  return mode == 7 ? 7 : windows == 3 ? 6 : windows == 1 ? 5 : 0;
}

static uint8_t ocean_config(uint16_t slot_id, uint32_t length,
                            uint8_t *bank_mask) {
  uint32_t offset = 0x40;
  uint32_t seen[2] = {0, 0};
  uint8_t count = 0, highest = 0;
  while (offset < length) {
    struct crt_chip chip;
    if (!read_chip(slot_id, offset, length, &chip) || chip.size != 0x2000 ||
        chip.kind || chip.bank > 63 ||
        (chip.address != 0x8000 && chip.address != 0xa000))
      return 0;
    uint16_t bank = chip.bank;
    uint32_t bit = 1u << (bank & 31);
    if (seen[bank >> 5] & bit)
      return 0;
    seen[bank >> 5] |= bit;
    highest = MAX(highest, bank);
    count++;
    offset += chip.packet;
  }
  // Accept the 32/64 KiB conversions as well as 128/256/512 KiB images.
  if (count < 4 || count > 64 || (count & (count - 1)) ||
      highest != count - 1)
    return 0;
  *bank_mask = count - 1;
  return 1;
}

static uint16_t funplay_bank(uint16_t bank) {
  return ((bank >> 3) & 7) | ((bank & 1) << 3);
}

// Reviewed full-size layouts only: validate everything before touching ROM.
static uint8_t banked_config(uint16_t slot_id, uint32_t length, uint8_t type) {
  uint32_t seen[2] = {0, 0};
  unsigned count = 0, expected = type == 9 ? 64 : type == 11 ? 4 :
                                type == 12 ? 3 : 16;
  uint32_t offset = 0x40;
  if (swap32(bridge_ds_get_uint32(slot_id, 0x10)) != 0x40)
    return 0;
  static const char signature[] = "C64 CARTRIDGE   ";
  for (unsigned i = 0; i < 16; i += 4) {
    uint32_t word = (uint8_t)signature[i] | ((uint32_t)signature[i + 1] << 8) |
                    ((uint32_t)signature[i + 2] << 16) |
                    ((uint32_t)signature[i + 3] << 24);
    if (bridge_ds_get_uint32(slot_id, i) != word)
      return 0;
  }
  if (bridge_ds_get_uint16(slot_id, 0x18) & 0xfefe)
    return 0;
  while (offset < length) {
    struct crt_chip chip;
    if (!read_chip(slot_id, offset, length, &chip) || chip.kind ||
        chip.packet != chip.size + 0x10u)
      return 0;
    unsigned bank = chip.bank;
    if (type == 10) {
      if (bank & ~0x39u)
        return 0;
      bank = funplay_bank(bank);
    }
    if (type == 12) {
      if (chip.address == 0x8000 && !bank && chip.size == 0x1000)
        bank = 2;
      else if (chip.address != 0xa000 || bank > 1 || chip.size != 0x2000)
        return 0;
    } else if (chip.address != 0x8000 || bank >= expected ||
               chip.size != (type == 11 ? 0x4000 : 0x2000)) {
      return 0;
    }
    uint32_t bit = 1u << (bank & 31);
    if (seen[bank >> 5] & bit)
      return 0;
    seen[bank >> 5] |= bit;
    count++;
    offset += chip.packet;
  }
  // Unique in-range banks plus the exact count guarantees no missing banks.
  return count == expected;
}

static void load_crt(uint16_t slot_id) {
  uint8_t *ROM_LO = (uint8_t *)0x51000000;
  uint8_t *ROM_HI = ROM_LO + (1 << 19);
  uint32_t slot_length = bridge_ds_get_length(slot_id);
  if (slot_length < 0x40)
    return;

  uint8_t crt_type = 0;
  uint16_t cart_hw_type = swap16(bridge_ds_get_uint16(slot_id, 0x16));
  switch (cart_hw_type) {
  case 0: // Generic 8K/16K or Ultimax
    crt_type = generic_config(slot_id, slot_length);
    if (!crt_type)
      return;
    break;
  case 19: // Magic Desk
    crt_type = 1;
    break;
  case 32: // Easy Flash
    crt_type = 2;
    break;
  case 1: // Action Replay
    crt_type = 3;
    break;
  case 5: // Ocean
    crt_type = 4;
    break;
  case 17: // Dinamic
    crt_type = 8;
    break;
  case 15: // C64 Game System
    crt_type = 9;
    break;
  case 7: // Fun Play / Power Play
    crt_type = 10;
    break;
  case 8: // Super Games
    crt_type = 11;
    break;
  case 18: // Zaxxon
    crt_type = 12;
    break;
  default: // Unsupported format
    return;
  }

  uint8_t ocean_bank_mask = 0;
  if (crt_type == 4 && !ocean_config(slot_id, slot_length, &ocean_bank_mask))
    return;

  if (crt_type >= 8 && !banked_config(slot_id, slot_length, crt_type))
    return;

  if (crt_type >= 5 && crt_type <= 7) {
    // Initialize absent windows rather than exposing a previously mounted ROM.
    volatile uint8_t *lo = ROM_LO, *hi = ROM_HI;
    for (unsigned i = 0; i < 0x2000; i++)
      lo[i] = hi[i] = 0xff;
  }
  uint32_t chip_packet_base = 0x40;
  while (chip_packet_base < slot_length) {
    struct crt_chip chip;
    if (!read_chip(slot_id, chip_packet_base, slot_length, &chip))
      return;

    // Ocean's ROML and ROMH packets describe one shared banked ROM array.
    uint8_t *ROM = (crt_type == 4 || chip.address == 0x8000) ? ROM_LO : ROM_HI;
    if (crt_type == 10)
      chip.bank = funplay_bank(chip.bank);
    if ((crt_type >= 5 && crt_type <= 7 && chip.size == 0x4000) ||
        crt_type == 11) {
      bridge_ds_read(slot_id, chip_packet_base + 0x10, 0x2000,
                     &ROM_LO[chip.bank * 0x2000]);
      bridge_ds_read(slot_id, chip_packet_base + 0x2010, 0x2000,
                     &ROM_HI[chip.bank * 0x2000]);
    } else {
      bridge_ds_read(slot_id, chip_packet_base + 0x10, chip.size,
                     &ROM[chip.bank * chip.size]);
    }
    chip_packet_base += chip.packet;
  }

  if (crt_type == 4) {
    *C64_CTRL = bits_set(*C64_CTRL, C64_OCEAN_MASK_SHIFT, 6, ocean_bank_mask);
    *C64_CTRL = bits_set(*C64_CTRL, C64_OCEAN_8K_SHIFT, 1,
                        ocean_bank_mask == 63);
  }
  misc_reset_core(crt_type); // Reset C64 and 1541
}

void crts_irq() {
  if (updated_slots & (1 << CRT_SLOT_ID)) {
    load_crt(CRT_SLOT_ID);
  }
}
