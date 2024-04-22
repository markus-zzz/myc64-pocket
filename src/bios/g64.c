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

#define G64_NUM_TRACKS 84
#define G64_TRACK_OFFSET_TABLE 12

static uint32_t track_offsets[G64_NUM_TRACKS];
static uint16_t track_sizes[G64_NUM_TRACKS];

static void load_track(uint8_t track_id) {
  while ((*TARGET_0 >> 16) != 0x6F6B)
    ;
  // XXX: Write to track size register before we fire off the refill

  *TARGET_20 = 110;                     // slot-id
  *TARGET_24 = track_offsets[track_id]; // slot-offset
  *TARGET_28 = 0x90000000;
  *TARGET_2C = track_sizes[track_id]; // length
  *TARGET_0 = 0x636D0180;
}

void g64_init() {
  // Setup track offsets
  for (unsigned i = 0; i < G64_NUM_TRACKS; i++) {
    *TARGET_20 = 110;                            // slot-id
    *TARGET_24 = G64_TRACK_OFFSET_TABLE + i * 4; // slot-offset
    *TARGET_28 = 0x70000000;
    *TARGET_2C = 4; // length
    *TARGET_0 = 0x636D0180;
    while ((*TARGET_0 >> 16) != 0x6F6B)
      ;
    volatile uint32_t *p32 = (volatile uint32_t *)0x70000000;
    track_offsets[i] =
        *p32 ? *p32 + 2 : 0; // skip the 16 bit track length field (if defined)
  }

  // Setup track sizes
  for (unsigned i = 0; i < G64_NUM_TRACKS; i++) {
    if (!track_offsets[i])
      continue;
    *TARGET_20 = 110;                  // slot-id
    *TARGET_24 = track_offsets[i] - 2; // slot-offset
    *TARGET_28 = 0x70000000;
    *TARGET_2C = 2; // length
    *TARGET_0 = 0x636D0180;
    while ((*TARGET_0 >> 16) != 0x6F6B)
      ;
    volatile uint16_t *p16 = (volatile uint16_t *)0x70000000;
    track_sizes[i] = *p16;
  }

  // XXX: For testing load a track
  load_track(2);
}

void g64_handle() {}

void g64_draw() {}
