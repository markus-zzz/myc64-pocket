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

static volatile uint8_t track_no = 0xff;
static volatile uint8_t led_on;
static volatile uint8_t motor_on;
static volatile uint8_t g64_loaded = 0;

static volatile uint32_t status_bar_timeout;

static void load_track(uint8_t track_id) {
  while ((*TARGET_0 >> 16) != 0x6F6B)
    ;
  // Write to track size register before we fire off the refill
  *C1541_TRACK_LEN = track_sizes[track_id];

  *TARGET_20 = G64_SLOT_ID;             // slot-id
  *TARGET_24 = track_offsets[track_id]; // slot-offset
  *TARGET_28 = 0x90000000;              // DP track memory
  *TARGET_2C = track_sizes[track_id];   // length
  *TARGET_0 = 0x636D0180;
}

void load_g64() {
  // Setup track offsets
  for (unsigned i = 0; i < G64_NUM_TRACKS; i++) {
    uint32_t track_off =
        bridge_ds_get_uint32(G64_SLOT_ID, G64_TRACK_OFFSET_TABLE + i * 4);
    track_offsets[i] =
        track_off ? track_off + 2
                  : 0; // skip the 16 bit track length field (if defined)
  }

  // Setup track sizes
  for (unsigned i = 0; i < G64_NUM_TRACKS; i++) {
    if (!track_offsets[i])
      continue;
    track_sizes[i] = bridge_ds_get_uint16(G64_SLOT_ID, track_offsets[i] - 2);
  }

  track_no = 0xff;
  g64_loaded = 1;
}

void g64_irq() {
  if (updated_slots & (1 << G64_SLOT_ID)) {
    load_g64();
    return;
  }

  if (!g64_loaded)
    return;
  uint32_t status = *C1541_STATUS;
  uint8_t req_track_no = status & 0x7f;
  led_on = (status >> 7) & 1;
  motor_on = (status >> 8) & 1;
  if (req_track_no != track_no) {
    load_track(req_track_no);
    track_no = req_track_no;
  }

  if (osd_mode == OSD_OFF || osd_mode == OSD_STATUS_BAR) {
    if (led_on || motor_on) {
      osd_mode = OSD_STATUS_BAR;
      status_bar_timeout = timer_ticks + 300;
    }

    if (osd_mode == OSD_STATUS_BAR && timer_ticks > status_bar_timeout) {
      osd_mode = OSD_OFF;
    }
  }
}

void g64_draw_status_bar() {
  osd_printf(0, 0, "[G64 MOTOR:%c LED:%c TRACK:$%x] ", motor_on ? '1' : '0',
             led_on ? '1' : '0', track_no);
}
