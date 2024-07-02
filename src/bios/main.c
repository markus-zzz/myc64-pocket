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

#define N_OSD_TABS sizeof(osd_tabs) / sizeof(osd_tabs[0])

#define TIMER_TIMEOUT 80000
volatile uint32_t timer_ticks;

void keyboard_virt_init();
void keyboard_virt_handle();
void keyboard_virt_draw();

void prgs_init();
void prgs_irq();

void crts_init();
void crts_irq();

void g64_init();
void g64_handle();
void g64_draw();
void g64_draw_status_bar();
void g64_irq();

void misc_init();
void misc_handle();
void misc_draw();

void keyboard_ext_handle();

static const struct osd_tab {
  const char *name;
  void (*init)(void);
  void (*handle)(void);
  void (*draw)(void);

} osd_tabs[] = {
    {"KEYBOARD", keyboard_virt_init, keyboard_virt_handle, keyboard_virt_draw},
    {"G64", g64_init, g64_handle, g64_draw},
    {"MISC", misc_init, misc_handle, misc_draw},
};

uint32_t cont1_key_p = 0;
uint32_t cont1_key = 0;

uint64_t c64_keyb_mask = 0;
uint64_t c64_isr_keyb_mask = 0;

uint8_t updated_slots;

static volatile int osd_idx;
osd_mode_t osd_mode;

void load_rom(uint16_t slot_id, volatile uint8_t *dst, uint32_t slot_length) {
  volatile uint8_t *p = (volatile uint8_t *)0x70000000;
  // uint16_t slot_length = get_ds_length(slot_id);
  if (!slot_length)
    return;

  const uint32_t buf_size = 256;
  uint32_t slot_offset = 0;

  while (slot_offset < slot_length) {
    uint32_t chunk_size = MIN(slot_length - slot_offset, buf_size);
    *TARGET_20 = slot_id;
    *TARGET_24 = slot_offset;
    *TARGET_28 = 0x70000000;
    *TARGET_2C = chunk_size;
    *TARGET_0 = 0x636D0180;
    while ((*TARGET_0 >> 16) != 0x6F6B)
      ;

    // Write into C64 ROM
    for (uint32_t i = 0; i < chunk_size; i++) {
      *dst++ = p[i];
    }

    slot_offset += chunk_size;
  }
}

int main(void) {

  // Initialize OSD tabs
  for (int i = 0; i < N_OSD_TABS; i++) {
    if (osd_tabs[i].init)
      osd_tabs[i].init();
  }
  prgs_init();
  crts_init();

  // Wait for previous command to finish
  while ((*TARGET_0 >> 16) != 0x6F6B)
    ;

  // Load C64 BASIC ROM
  load_rom(200, (volatile uint8_t *)0x50010000, 8192);
  // Load C64 CHAR ROM
  load_rom(201, (volatile uint8_t *)0x50020000, 4096);
  // Load C64 KERNAL ROM
  load_rom(202, (volatile uint8_t *)0x50030000, 8192);

  // Load 1540/1541 ROMs
  load_rom(203, (volatile uint8_t *)0x50040000, 8192);
  load_rom(204, (volatile uint8_t *)(0x50040000 + 8192), 8192);

  *C64_CTRL = bits_set(*C64_CTRL, 1, 2, 2); // Joystick1 = cont2
  *C64_CTRL = bits_set(*C64_CTRL, 3, 2, 1); // Joystick2 = cont1
  *C64_CTRL = bits_set(*C64_CTRL, 0, 1, 1); // Release reset for MyC64

  cont1_key_p = 0;
  cont1_key = 0;

  osd_idx = 0;
  osd_mode = OSD_OFF;

  osd_clear();
  int osd_idx_prev = osd_idx;
  osd_mode_t osd_mode_prev = osd_mode;

  timer_start(TIMER_TIMEOUT);
  IRQ_ENABLE();

  while (1) {
    if (osd_mode_prev != osd_mode) {
      osd_clear();
      osd_mode_prev = osd_mode;
    }
    switch (osd_mode) {
    case OSD_FULL: {
      int osd_idx_tmp = osd_idx;
      if (osd_idx_prev != osd_idx_tmp) {
        osd_clear();
        osd_idx_prev = osd_idx_tmp;
      }
      // Draw tab bar
      int offset = 0;
      for (int i = 0; i < N_OSD_TABS; i++) {
        offset = osd_put_str(offset, 1, osd_tabs[i].name, i == osd_idx_tmp);
        offset += 8;
      }
      // Handle active tab
      const struct osd_tab *tab = &osd_tabs[osd_idx_tmp];
      if (tab->draw)
        tab->draw();
      break;
    }
    case OSD_STATUS_BAR:
      g64_draw_status_bar();
      break;
    case OSD_OFF:
      break;
    }
  }

  return 0;
}

static uint32_t navigation_keys_prev;
static uint32_t navigation_timeout;

uint32_t *irq(uint32_t *regs, uint32_t irqs) {
  timer_start(TIMER_TIMEOUT);
  timer_ticks++;

  updated_slots = *UPDATED_SLOTS;

  prgs_irq();
  crts_irq();
  g64_irq();

  // Prologue
  cont1_key = *CONT1_KEY;
  c64_keyb_mask = 0;

  // Handle auto-repeat for navigation keys
  const uint32_t navigation_mask = 0x30f;
  uint32_t navigation_keys = cont1_key & navigation_mask;
  if (navigation_keys) {
    if (navigation_keys_prev != navigation_keys) {
      navigation_timeout = timer_ticks + 66; // initial delay
    } else if (navigation_timeout == timer_ticks) {
      // Fake a pos-edge
      cont1_key_p &= ~navigation_keys;
      navigation_timeout = timer_ticks + 10; // repeat delay
    }
  }
  navigation_keys_prev = navigation_keys;

  if (KEYB_POSEDGE(face_select)) {
    if (osd_mode == OSD_FULL) {
      osd_mode = OSD_OFF;
    } else {
      osd_mode = OSD_FULL;
    }
    if (osd_mode == OSD_FULL) {
      // Read joystick mapping when entering OSD
      joystick1 = bits_get(*C64_CTRL, 1, 2);
      joystick2 = bits_get(*C64_CTRL, 3, 2);
      // Disconnect both joysticks while in OSD
      *C64_CTRL = bits_set(*C64_CTRL, 1, 2, 0);
      *C64_CTRL = bits_set(*C64_CTRL, 3, 2, 0);
    } else {
      // Restore joystick mapping when leaving OSD
      *C64_CTRL = bits_set(*C64_CTRL, 1, 2, joystick1);
      *C64_CTRL = bits_set(*C64_CTRL, 3, 2, joystick2);
    }
  }

  if (osd_mode == OSD_FULL) {
    if (KEYB_POSEDGE(trig_l1)) {
      osd_idx--;
      osd_idx = MAX(osd_idx, 0);
    } else if (KEYB_POSEDGE(trig_r1)) {
      osd_idx++;
      osd_idx = MIN(osd_idx, N_OSD_TABS - 1);
    }

    // Handle active tab
    const struct osd_tab *tab = &osd_tabs[osd_idx];
    if (tab->handle)
      tab->handle();
  }

  switch (osd_mode) {
  case OSD_FULL:
    *OSD_CTRL = 1;
    break;
  case OSD_STATUS_BAR:
    *OSD_CTRL = 3;
    break;
  case OSD_OFF:
    *OSD_CTRL = 0;
    break;
  }

  // Always handle external keyboard
  if (1) { // (*CONT3_KEY >> 28) == 0x4) { // Docked keyboard
    keyboard_ext_handle();
  }

  c64_keyb_mask |= c64_isr_keyb_mask;

  // Epilogue
  *KEYB_MASK_0 = c64_keyb_mask;
  *KEYB_MASK_1 = c64_keyb_mask >> 32;

  cont1_key_p = cont1_key;

  return regs;
}
