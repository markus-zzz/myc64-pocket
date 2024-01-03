/*
 * Copyright (C) 2019-2020 Markus Lavin (https://www.zzzconsulting.se/)
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

#include "Vcore_top.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <assert.h>
#include <fstream>
#include <functional>
#include <gtk/gtk.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "supermon64.h"

#include "core/myc64/roms/kernal.h"
#include "core/myc64/roms/basic.h"
#include "core/myc64/roms/characters.h"

static Vcore_top *dut = NULL;
static VerilatedVcdC *trace = NULL;
static unsigned TraceTick = 0;

double sc_time_stamp() { return TraceTick; }

std::map<uint16_t, std::pair<const uint8_t *, uint32_t>> dataslots;

class ClockManager {
  using ClockCB = std::function<void(void)>;
  struct Clock {
    Clock(CData *clk_net, double freq, uint64_t offset_ps, ClockCB CallBack) {
      m_clk_net = clk_net;
      m_cycle_time_ps = 1e12 / freq;
      m_next_time_ps = offset_ps;
      m_CallBack = CallBack;
    }
    CData *m_clk_net;
    double m_freq;
    uint64_t m_cycle_time_ps;
    uint64_t m_next_time_ps;
    ClockCB m_CallBack;
  };

  std::vector<Clock> m_Clocks;
  uint64_t m_CurrTimePS = 0;

  Clock *getNext() {
    Clock *FirstClock = &m_Clocks[0];
    for (Clock &C : m_Clocks)
      if (C.m_next_time_ps < FirstClock->m_next_time_ps)
        FirstClock = &C;
    return FirstClock;
  }

public:
  void addClock(CData *clk_net, double freq, uint64_t offset_ps,
                ClockCB CallBack = std::function<void(void)>()) {
    m_Clocks.emplace_back(Clock(clk_net, freq, offset_ps, CallBack));
  }
  void doWork() {
    Clock *C = getNext();
    dut->eval();
    dut->eval();
    if (trace)
      trace->dump(m_CurrTimePS);
    m_CurrTimePS = C->m_next_time_ps;
    *C->m_clk_net = !(*C->m_clk_net);
    dut->eval();
    dut->eval();
    if (trace)
      trace->dump(m_CurrTimePS);
    C->m_next_time_ps += C->m_cycle_time_ps / 2;

    if (C->m_CallBack)
      C->m_CallBack();
  }
};

static void put_pixel(GdkPixbuf *pixbuf, int x, int y, guchar red, guchar green,
                      guchar blue) {
  int width, height, rowstride, n_channels;
  guchar *pixels, *p;

  n_channels = gdk_pixbuf_get_n_channels(pixbuf);

  g_assert(gdk_pixbuf_get_colorspace(pixbuf) == GDK_COLORSPACE_RGB);
  g_assert(gdk_pixbuf_get_bits_per_sample(pixbuf) == 8);
  g_assert(!gdk_pixbuf_get_has_alpha(pixbuf));
  g_assert(n_channels == 3);

  width = gdk_pixbuf_get_width(pixbuf);
  height = gdk_pixbuf_get_height(pixbuf);

  g_assert(x >= 0 && x < width);
  g_assert(y >= 0 && y < height);

  rowstride = gdk_pixbuf_get_rowstride(pixbuf);
  pixels = gdk_pixbuf_get_pixels(pixbuf);

  p = pixels + y * rowstride + x * n_channels;
  p[0] = red;
  p[1] = green;
  p[2] = blue;
}

struct VICIIFrameDumper {
  VICIIFrameDumper() {
    m_FramePixBuf =
        gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, c_Xres, c_Yres);
  }
  void operator()() {
    if (dut->clk_74a) {
      HandleBridge();
      bool FrameDone = false;

      if (dut->video_hs) {
        m_HCntr = 0;
        m_VCntr++;
      }
      if (dut->video_vs) {
        m_VCntr = 0;
        FrameDone = true;
      }

      unsigned m_HCntrShifted = m_HCntr - 70;
      unsigned m_VCntrShifted = m_VCntr - 10;
      if (0 <= m_HCntrShifted && m_HCntrShifted < c_Xres &&
          0 <= m_VCntrShifted && m_VCntrShifted < c_Yres) {
        guchar Red = dut->video_rgb >> 16;
        guchar Green = dut->video_rgb >> 8;
        guchar Blue = dut->video_rgb & 0xff;
        put_pixel(m_FramePixBuf, m_HCntrShifted, m_VCntrShifted, Red, Green,
                  Blue);
      }

      m_HCntr++;

      if (FrameDone) {
        char buf[32];
        snprintf(buf, sizeof(buf), "vicii-%03d.png", m_FrameIdx++);
        gdk_pixbuf_save(m_FramePixBuf, buf, "png", NULL, NULL);
        printf("%s\n", buf);
      }
    }
  }

private:
  void HandleBridge() {
 //   if (m_FrameIdx < 150) return;
    dut->bridge_addr = 0;
    dut->bridge_rd = 0;
    dut->bridge_wr = 0;
    dut->bridge_wr_data = 0;

    switch (bridge_state) {
    case 0: // Wait for reset to release
      if (dut->reset_n)
        bridge_state = 1;
      break;
    case 1: // Write status OK
      dut->bridge_addr = 0xf8001000;
      dut->bridge_wr = 1;
      dut->bridge_wr_data = 0x6f6b1234;
      bridge_state = 2;
      break;
    case 2: // Wait for data-slot-read command
      dut->bridge_addr = 0xf8001000;
      dut->bridge_rd = 1;
      if (dut->bridge_rd_data == 0x636D0180) {
        dut->bridge_addr = 0xf8001020;
        bridge_state = 3;
      }
      break;
    case 3: // Latch slot_id
      ds_read_slot_id = dut->bridge_rd_data;
      dut->bridge_addr = 0xf8001024;
      dut->bridge_rd = 1;
      bridge_state = 4;
      break;
    case 4: // Latch slot_offset
      ds_read_slot_offset = dut->bridge_rd_data;
      dut->bridge_addr = 0xf8001028;
      dut->bridge_rd = 1;
      bridge_state = 5;
      break;
    case 5: // Latch bridge address
      ds_read_bridge_address = dut->bridge_rd_data;
      dut->bridge_addr = 0xf800102c;
      dut->bridge_rd = 1;
      bridge_state = 6;
      break;
    case 6: // Latch length
      ds_read_length = dut->bridge_rd_data;
      ds_read_cntr = 0;
      bridge_state = 7;
      printf("data-slot-read: id=%d, offset=%d, bridge_addr=0x%x, length=%d\n",
             ds_read_slot_id, ds_read_slot_offset, ds_read_bridge_address,
             ds_read_length);
      break;
    case 7: // Write data / status
      if (ds_read_cntr < ds_read_length) {
        dut->bridge_addr = ds_read_bridge_address + ds_read_cntr;
        dut->bridge_wr_data = 0;
        auto *data = dataslots[ds_read_slot_id].first;
        for (unsigned i = 0; i < 4; i++)
          dut->bridge_wr_data |= data[ds_read_slot_offset + ds_read_cntr + i]
                               << (8 * (3 - i));
        dut->bridge_wr = 1;
        ds_read_cntr += 4;
      } else {
        dut->bridge_addr = 0xf8001000;
        dut->bridge_wr = 1;
        dut->bridge_wr_data = 0x6f6b0000;
        bridge_state = 2;
      }
      break;
    default:
      break;
    }
  }
  const unsigned c_Xres = 504;
  const unsigned c_Yres = 312;
  GdkPixbuf *m_FramePixBuf;
  unsigned m_FrameIdx = 0;
  unsigned m_HCntr = 0;
  unsigned m_VCntr = 0;

  int bridge_state = 0;
  uint32_t ds_read_slot_id;
  uint32_t ds_read_slot_offset;
  uint32_t ds_read_bridge_address;
  uint32_t ds_read_length;
  uint32_t ds_read_cntr;
};

int main(int argc, char *argv[]) {
  bool TraceOn = false;
  // Initialize Verilators variables
  Verilated::commandArgs(argc, argv);
  Verilated::traceEverOn(TraceOn);

  dut = new Vcore_top;

  if (TraceOn) {
    trace = new VerilatedVcdC;
    trace->set_time_unit("1ps");
    trace->set_time_resolution("1ps");
    dut->trace(trace, 99);
    trace->open("dump.vcd");
  }

  dataslots[200] = std::make_pair(basic_bin, basic_bin_len);
  dataslots[201] = std::make_pair(characters_bin, characters_bin_len);
  dataslots[202] = std::make_pair(kernal_bin, kernal_bin_len);

  VICIIFrameDumper myVICIIFrameDumper;

  ClockManager CM;
  CM.addClock(&dut->clk_74a, 15e6, 0, myVICIIFrameDumper);

  dut->reset_n = 0;
  dut->eval();
  if (trace)
    trace->dump(0);

  unsigned idx = 0;
  while (!Verilated::gotFinish()) {
    if (idx++ > 32) {
      dut->reset_n = 1;
    }
    CM.doWork();
    if (trace)
      trace->flush();
  }

  return 0;
}
