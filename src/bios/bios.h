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

#pragma once

#include <stdint.h>

#define NULL ((void *)0)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define CONT1_KEY ((volatile uint32_t *)0x20000000)

#define CONT1_KEY ((volatile uint32_t *)0x20000000)
#define CONT2_KEY ((volatile uint32_t *)0x20000004)
#define CONT3_KEY ((volatile uint32_t *)0x20000008)
#define CONT4_KEY ((volatile uint32_t *)0x2000000c)
#define CONT1_JOY ((volatile uint32_t *)0x20000010)
#define CONT2_JOY ((volatile uint32_t *)0x20000014)
#define CONT3_JOY ((volatile uint32_t *)0x20000018)
#define CONT4_JOY ((volatile uint32_t *)0x2000001c)
#define CONT1_TRIG ((volatile uint32_t *)0x20000020)
#define CONT2_TRIG ((volatile uint32_t *)0x20000024)
#define CONT3_TRIG ((volatile uint32_t *)0x20000028)
#define CONT4_TRIG ((volatile uint32_t *)0x2000002c)

#define OSD_CTRL ((volatile uint32_t *)0x30000000)
#define KEYB_MASK_0 ((volatile uint32_t *)0x30000004)
#define KEYB_MASK_1 ((volatile uint32_t *)0x30000008)
#define C64_CTRL ((volatile uint32_t *)0x3000000c)

#define C1541_STATUS ((volatile uint32_t *)0x30000100)
#define C1541_TRACK_LEN ((volatile uint32_t *)0x30000104)

#define TARGET_0 ((volatile uint32_t *)0x40000000)
#define TARGET_4 ((volatile uint32_t *)0x40000004)
#define TARGET_8 ((volatile uint32_t *)0x40000008)
#define TARGET_20 ((volatile uint32_t *)0x40000020)
#define TARGET_24 ((volatile uint32_t *)0x40000024)
#define TARGET_28 ((volatile uint32_t *)0x40000028)
#define TARGET_2C ((volatile uint32_t *)0x4000002c)
#define TARGET_40 ((volatile uint32_t *)0x40000040)
#define TARGET_44 ((volatile uint32_t *)0x40000044)
#define TARGET_48 ((volatile uint32_t *)0x40000048)
#define TARGET_4C ((volatile uint32_t *)0x4000004c)

#define UPDATED_SLOTS ((volatile uint32_t *)0x40000080)

#define BRIDGE_DPRAM ((volatile uint8_t *)0x70000000)
#define BRIDGE_DS_TABLE ((volatile uint32_t *)0x90000000)

#define KEYB_BIT_dpad_up 0
#define KEYB_BIT_dpad_down 1
#define KEYB_BIT_dpad_left 2
#define KEYB_BIT_dpad_right 3
#define KEYB_BIT_face_a 4
#define KEYB_BIT_face_b 5
#define KEYB_BIT_face_x 6
#define KEYB_BIT_face_y 7
#define KEYB_BIT_trig_l1 8
#define KEYB_BIT_trig_r1 9
#define KEYB_BIT_trig_l2 10
#define KEYB_BIT_trig_r2 11
#define KEYB_BIT_trig_l3 12
#define KEYB_BIT_trig_r3 13
#define KEYB_BIT_face_select 14
#define KEYB_BIT_face_start 15

#define KEYB_POSEDGE(bit)                                                      \
  ((~cont1_key_p & (1 << (KEYB_BIT_##bit))) &&                                 \
   (cont1_key & (1 << (KEYB_BIT_##bit))))

#define KEYB_DOWN(bit) (cont1_key & (1 << (KEYB_BIT_##bit)))

#define C64_KEYB_MASK_KEY(x) (1ULL << ((((x) >> 4) & 0xf) * 8 + ((x)&0xf)))

#define IRQ_ENABLE() irq_mask(0)
#define IRQ_DISABLE() irq_mask(-1)

extern volatile uint32_t timer_ticks;

extern uint32_t cont1_key_p;
extern uint32_t cont1_key;

extern uint64_t c64_keyb_mask;
extern uint64_t c64_isr_keyb_mask;
extern uint8_t updated_slots;

void osd_clear();
void osd_put_char(int x, int y, char c, int invert);
unsigned osd_put_str(int x, int y, const char *str, int invert);
unsigned osd_put_hex8(int x, int y, uint8_t val, int invert);
unsigned osd_put_hex16(int x, int y, uint16_t val, int invert);

void irq_mask(uint32_t mask);
void timer_start(uint32_t timeout);

static inline uint32_t bits_get(uint32_t in, uint32_t pos, uint32_t width) {
  uint32_t mask = (1 << width) - 1;
  return (in >> pos) & mask;
}

static inline uint32_t bits_set(uint32_t in, uint32_t pos, uint32_t width,
                                uint32_t val) {
  uint32_t mask = (1 << width) - 1;
  val &= mask;
  in &= ~(mask << pos);
  return in | (val << pos);
}
