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

#include "CLI11.hpp"

#include "Vcore_top.h"
#include "Vcore_top_core_top.h"
#include "Vcore_top_spram__A10_D8.h"
#include "verilated.h"
#include "verilated_fst_c.h"
#include <assert.h>
#include <fstream>
#include <functional>
#include <gtk/gtk.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "basic.h"
#include "characters.h"
#include "kernal.h"

#include "1540-c000.h"
#include "1541-e000.h"

#define CRT_SLOT_ID 0
#define PRG_SLOT_ID 1
#define G64_SLOT_ID 2

static unsigned trace_begin_frame = 0;
static std::string iec_trace_path;
static unsigned iec_trace_begin_frame = 0;
static FILE *iec_trace_fp = nullptr;
static std::string prg_path;
static std::string g64_path;
static std::string crt_path;
static bool dump_video = false;
static bool dump_ram = false;
static unsigned exit_frame = 0;

static std::unique_ptr<Vcore_top> dut;

std::map<uint16_t, std::pair<const uint8_t *, uint32_t>> dataslots;
std::vector<uint16_t> updated_dataslots;
std::vector<uint16_t>::iterator updated_dataslots_iter;

std::vector<std::pair<unsigned, uint16_t>> key_cmds;
std::vector<std::pair<unsigned, uint16_t>>::iterator key_cmds_iter;

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

  VerilatedFstC *m_trace = nullptr;
  bool m_trace_enabled = false;

public:
  void addClock(CData *clk_net, double freq, uint64_t offset_ps,
                ClockCB CallBack = std::function<void(void)>()) {
    m_Clocks.emplace_back(Clock(clk_net, freq, offset_ps, CallBack));
  }
  void doWork() {
    Clock *C = getNext();
    m_CurrTimePS = C->m_next_time_ps;
    *C->m_clk_net = !(*C->m_clk_net);
    dut->eval();
    dut->eval();
    if (m_trace_enabled)
      m_trace->dump(m_CurrTimePS);
    C->m_next_time_ps += C->m_cycle_time_ps / 2;

    if (C->m_CallBack)
      C->m_CallBack();
  }
  uint64_t CurrTimePS() { return m_CurrTimePS; }

  void setupTrace(const std::string &out_path,
                  const std::vector<std::string> &trace_modules) {
    m_trace = new VerilatedFstC;
    m_trace->set_time_unit("1ps");
    m_trace->set_time_resolution("1ps");
    for (auto &trace_module : trace_modules) {
      m_trace->dumpvars(1, trace_module);
    }
    dut->trace(m_trace, 99);
    m_trace->open(out_path.c_str());
  }
  void enableTrace() {
    if (m_trace)
      m_trace_enabled = true;
  }
  void flushTrace() {
    if (m_trace && m_trace_enabled)
      m_trace->flush();
  }
};

static ClockManager CM;

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
        if (dump_video) {
          char buf[32];
          snprintf(buf, sizeof(buf), "vicii-%04d.png", m_FrameIdx);
          gdk_pixbuf_save(m_FramePixBuf, buf, "png", NULL, NULL);
          printf("%s\n", buf);
        }

        if (dump_ram) {
          uint8_t *ram = &dut->core_top->u_c64_main_ram->mem[0];
          char buf[32];
          snprintf(buf, sizeof(buf), "ram-%04d.bin", m_FrameIdx);
          std::fstream file(buf,
                            std::ios::out | std::ios::binary | std::ios::trunc);
          file.write(reinterpret_cast<char *>(ram), 0x10000);
          file.close();
          printf("%s\n", buf);
        }

        // Handle key injection
        enum class KeyState { Idle, Press, Release };
        static KeyState key_state;
        if (key_cmds_iter != key_cmds.end()) {
          auto key_cmd = *key_cmds_iter;
          if (key_cmd.first) {
            if (m_FrameIdx >= key_cmd.first) {
              key_cmds_iter++;
              key_state = KeyState::Idle;
            }
          } else {
            switch (key_state) {
            case KeyState::Idle:
              if (key_cmd.second >= 0x100) { // Modifier key
                dut->cont3_key = key_cmd.second;
                key_cmd = *(++key_cmds_iter);
              }
              dut->cont3_joy = key_cmd.second;
              key_cmds_wait = m_FrameIdx + 2;
              key_state = KeyState::Press;
              break;
            case KeyState::Press:
              if (m_FrameIdx >= key_cmds_wait) {
                key_cmds_wait = m_FrameIdx + 2;
                key_state = KeyState::Release;
              }
              break;
            case KeyState::Release:
              dut->cont3_joy = 0;
              dut->cont3_key = 0;
              if (m_FrameIdx >= key_cmds_wait) {
                key_cmds_iter++;
                key_state = KeyState::Idle;
              }
              break;
            }
          }
        }

        CM.flushTrace();
        if (exit_frame != 0 && exit_frame == m_FrameIdx) {
          exit(0);
        }
        m_FrameIdx++;
      }

      if (!iec_trace_path.empty() && m_FrameIdx >= iec_trace_begin_frame &&
          dut->debug_1mhz_ph1_en) {
        fprintf(iec_trace_fp, "%d,%d,%d\n", dut->debug_iec_atn,
                dut->debug_iec_clock, dut->debug_iec_data);
      }
    }
  }

private:
  void HandleBridge() {
    if (m_FrameIdx == trace_begin_frame)
      CM.enableTrace();
    dut->bridge_addr = 0;
    dut->bridge_rd = 0;
    dut->bridge_wr = 0;
    dut->bridge_wr_data = 0;

    switch (bridge_state) {
    case 0: // Wait for reset to release
      if (dut->reset_n) {
        cntr = 0;
        ds_it = dataslots.begin();
        bridge_state = 100;
      }
      break;
    case 100: // Write Data Slot Size table (slot id)
      if (ds_it == dataslots.end()) {
        bridge_state = 1;
      } else {
        auto &dse = *ds_it;
        dut->bridge_addr = 0xf8002000 + cntr * 8 + 0;
        dut->bridge_wr = 1;
        dut->bridge_wr_data = dse.first;
        bridge_state = 101;
      }
      break;
    case 101: { // Write Data Slot Size table (size)
      auto &dse = *ds_it;
      dut->bridge_addr = 0xf8002000 + cntr * 8 + 4;
      dut->bridge_wr = 1;
      dut->bridge_wr_data = dse.second.second;
      cntr++;
      ds_it++;
      bridge_state = 100;
      break;
    }
    case 200: // Data slot update
      dut->bridge_addr = 0xf8000020;
      dut->bridge_wr = 1;
      dut->bridge_wr_data = *updated_dataslots_iter++; // slot id
      bridge_state = 201;
      break;
    case 201: // Data slot update
      dut->bridge_addr = 0xf8000000;
      dut->bridge_wr = 1;
      dut->bridge_wr_data = 0x434d008a;
      bridge_state = 202;
      break;
    case 202: // Data slot update
      if (updated_dataslots_iter != updated_dataslots.end()) {
        bridge_state = 200;
      } else {
        bridge_state = 2;
      }
      break;
    case 1: // Write status OK
      dut->bridge_addr = 0xf8001000;
      dut->bridge_wr = 1;
      dut->bridge_wr_data = 0x6f6b1234;
      if (updated_dataslots_iter != updated_dataslots.end()) {
        bridge_state = 200;
      } else {
        bridge_state = 2;
      }
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
  unsigned key_cmds_wait = 0;
  unsigned m_HCntr = 0;
  unsigned m_VCntr = 0;

  int bridge_state = 0;
  uint32_t ds_read_slot_id;
  uint32_t ds_read_slot_offset;
  uint32_t ds_read_bridge_address;
  uint32_t ds_read_length;
  uint32_t ds_read_cntr;

  unsigned cntr = 0;
  decltype(dataslots)::iterator ds_it;
};

double sc_time_stamp() { return CM.CurrTimePS(); }

int main(int argc, char *argv[]) {
  std::string trace_path;
  std::string keys_str;
  std::vector<std::string> trace_modules;
  CLI::App app{"Verilator based MyC64-pocket simulator"};
  app.add_flag("--dump-video", dump_video, "Dump video output as .png");
  app.add_flag("--dump-ram", dump_ram, "Dump RAM after each frame");
  app.add_option("--exit-frame", exit_frame, "Exit frame");
  app.add_option("--trace", trace_path, ".fst trace output");
  app.add_option("--trace-begin-frame", trace_begin_frame,
                 "Start trace on given frame")
      ->needs("--trace");
  app.add_option("--trace-modules", trace_modules, "Specify modules to trace")
      ->needs("--trace");
  app.add_option("--prg", prg_path, ".prg file to put in slot")
      ->check(CLI::ExistingFile);
  app.add_option("--g64", g64_path, ".g64 file to put in slot")
      ->check(CLI::ExistingFile);
  app.add_option("--crt", crt_path, ".crt file to put in slot")
      ->check(CLI::ExistingFile);
  app.add_option("--iec-trace", iec_trace_path, "IEC trace output to .csv");
  app.add_option("--iec-trace-begin-frame", iec_trace_begin_frame,
                 "Start IEC trace on given frame");
  app.add_option("--keys", keys_str,
                 "Key input string of the form '[150]10 PRINT<LSHIFT>2HELLO "
                 "WORLD<LSHIFT>2<RET>20 GOTO 10<RET>RUN<RET>'");
  CLI11_PARSE(app, argc, argv);

  // Initialize Verilators variables
  Verilated::commandArgs(argc, argv);
  Verilated::traceEverOn(!trace_path.empty());

  dut = std::make_unique<Vcore_top>();

  if (!keys_str.empty()) {
    std::map<std::string, uint16_t> key_map;
#define DEF_KEY(a, b) key_map[std::string(a)] = b;
#include "keys.def"
#undef DEF_KEY
    std::string &keys = keys_str;

    std::string::size_type p1 = 0;
    while (p1 < keys.size()) {
      if (keys[p1] == '[') {
        auto p2 = keys.find("]", p1);
        if (p2 == std::string::npos)
          return -1;
        auto frame = keys.substr(p1 + 1, p2 - p1 - 1);
        key_cmds.push_back(std::make_pair(std::stoi(frame), 0));
        p1 = p2 + 1;
      } else if (keys[p1] == '<') {
        auto p2 = keys.find(">", p1);
        if (p2 == std::string::npos)
          return -1;
        auto longkey = keys.substr(p1, p2 - p1 + 1);
        assert(key_map.count(longkey) > 0);
        key_cmds.push_back(std::make_pair(0, key_map[longkey]));
        p1 = p2 + 1;
      } else {
        auto key = keys.substr(p1, 1);
        assert(key_map.count(key) > 0);
        key_cmds.push_back(std::make_pair(0, key_map[key]));
        p1++;
      }
    }
  }
  key_cmds_iter = key_cmds.begin();

  dataslots[200] = std::make_pair(basic_bin, basic_bin_len);
  dataslots[201] = std::make_pair(characters_bin, characters_bin_len);
  dataslots[202] = std::make_pair(kernal_bin, kernal_bin_len);
  dataslots[203] = std::make_pair(__1540_c000_bin, __1540_c000_bin_len);
  dataslots[204] = std::make_pair(__1541_e000_bin, __1541_e000_bin_len);

  // Load .prg into slot
  std::vector<uint8_t> prg_slot;
  if (!prg_path.empty()) {
    std::ifstream instream(prg_path, std::ios::in | std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(instream)),
                              std::istreambuf_iterator<char>());
    prg_slot = std::move(data);
    dataslots[PRG_SLOT_ID] = std::make_pair(prg_slot.data(), prg_slot.size());
    updated_dataslots.push_back(PRG_SLOT_ID);
  }

  // Load .g64 into slot
  std::vector<uint8_t> g64_slot;
  if (!g64_path.empty()) {
    std::ifstream instream(g64_path, std::ios::in | std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(instream)),
                              std::istreambuf_iterator<char>());
    g64_slot = std::move(data);
    dataslots[G64_SLOT_ID] = std::make_pair(g64_slot.data(), g64_slot.size());
    updated_dataslots.push_back(G64_SLOT_ID);
  }

  // Load .crt into slot
  std::vector<uint8_t> crt_slot;
  if (!crt_path.empty()) {
    std::ifstream instream(crt_path, std::ios::in | std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(instream)),
                              std::istreambuf_iterator<char>());
    crt_slot = std::move(data);
    dataslots[CRT_SLOT_ID] = std::make_pair(crt_slot.data(), crt_slot.size());
    updated_dataslots.push_back(CRT_SLOT_ID);
  }

  updated_dataslots_iter = updated_dataslots.begin();

  if (!iec_trace_path.empty()) {
    iec_trace_fp = fopen(iec_trace_path.c_str(), "w");
    fprintf(iec_trace_fp, "atn,clk,dat\n");
  }

  VICIIFrameDumper myVICIIFrameDumper;

  CM.addClock(&dut->clk_74a, 8e6, 0, myVICIIFrameDumper);
  if (!trace_path.empty())
    CM.setupTrace(trace_path, trace_modules);

  dut->reset_n = 0;
  dut->eval();

  unsigned idx = 0;
  while (!Verilated::gotFinish()) {
    if (idx++ > 32) {
      dut->reset_n = 1;
    }
    CM.doWork();
  }

  return 0;
}
