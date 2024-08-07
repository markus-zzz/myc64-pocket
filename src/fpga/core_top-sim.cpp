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

#define CLK_32MHZ 1

#define CRT_SLOT_ID 0
#define PRG_SLOT_ID 1
#define G64_SLOT_ID 2

static uint64_t g_ticks = 0;
static uint32_t g_frame_idx = 0;

static std::unique_ptr<Vcore_top> dut;

using Memory = std::array<uint8_t, 0x10000>;
unsigned disasm(FILE *fp, const Memory &mem, uint16_t addr);

class SimplePSRAM {
public:
  SimplePSRAM() {}
  void Tick() {
    if (dut->clk_32mhz) {
      if (!dut->cram0_adv_n) {
        addr_ = (dut->cram0_a << 16) | dut->cram0_dq;
        assert(addr_ < mem_.size());
      } else {
        if (!dut->cram0_we_n) {
          // Write
          if (!dut->cram0_ub_n)
            mem_[addr_] = (mem_[addr_] & 0x00ff) | (dut->cram0_dq & 0xff00);
          if (!dut->cram0_lb_n)
            mem_[addr_] = (mem_[addr_] & 0xff00) | (dut->cram0_dq & 0x00ff);
        } else {
          // Read
          dut->cram0_dq = mem_[addr_];
        }
      }
    }
  }

private:
  std::array<uint16_t, 2 * 1024 * 1024> mem_;
  uint32_t addr_ = 0;
};

class Trace6502 {
public:
  Trace6502(const std::string &path, const uint8_t &debug_cpu_valid,
            const uint8_t &debug_cpu_sync, const uint16_t &debug_cpu_addr,
            const uint8_t &debug_cpu_data, const uint64_t &debug_cpu_regs)
      : debug_cpu_valid_(debug_cpu_valid), debug_cpu_sync_(debug_cpu_sync),
        debug_cpu_addr_(debug_cpu_addr), debug_cpu_data_(debug_cpu_data),
        debug_cpu_regs_(debug_cpu_regs) {
    fp_ = fopen(path.c_str(), "w");
  }
  void Tick() {
    if (debug_cpu_valid_) {
      mem_[debug_cpu_addr_] = debug_cpu_data_;
      if (debug_cpu_sync_) {
        auto pos = disasm(fp_, mem_, prev_sync_addr);
        prev_sync_addr = debug_cpu_addr_;
        while (pos++ < 40)
          putc(' ', fp_);
        uint8_t reg_a = debug_cpu_regs_;
        uint8_t reg_x = debug_cpu_regs_ >> 8;
        uint8_t reg_y = debug_cpu_regs_ >> 16;
        uint8_t reg_p = debug_cpu_regs_ >> 24;
        uint8_t reg_sp = debug_cpu_regs_ >> 32;
        uint16_t reg_pc = debug_cpu_regs_ >> 48;
        fprintf(fp_, "[A:$%02X X:$%02X Y:$%02X SP:$%02X ", reg_a, reg_x, reg_y,
                reg_sp);

        fprintf(fp_, " SR:%c%c-%c%c%c%c%c] ", reg_p & 0x80 ? 'N' : '-',
                reg_p & 0x40 ? 'V' : '-', reg_p & 0x10 ? 'B' : '-',
                reg_p & 0x08 ? 'D' : '-', reg_p & 0x04 ? 'I' : '-',
                reg_p & 0x02 ? 'Z' : '-', reg_p & 0x01 ? 'C' : '-');

        fprintf(fp_, "[F:%u C:%lu]\n", g_frame_idx, g_ticks);
      }
    }
  }

private:
  FILE *fp_;
  Memory mem_;
  uint16_t prev_sync_addr;
  const uint8_t &debug_cpu_valid_;
  const uint8_t &debug_cpu_sync_;
  const uint16_t &debug_cpu_addr_;
  const uint8_t &debug_cpu_data_;
  const uint64_t &debug_cpu_regs_;
};

class TraceRTL {
public:
  TraceRTL(const std::string &out_path, const std::vector<std::string> &modules,
           uint32_t begin_frame)
      : begin_frame_(begin_frame) {
    trace = new VerilatedFstC;
    trace->set_time_unit("1ps");
    trace->set_time_resolution("1ps");
    for (auto &module : modules) {
      trace->dumpvars(1, module);
    }
    dut->trace(trace, 99);
    trace->open(out_path.c_str());
  }
  void Tick() {
    if (g_frame_idx >= begin_frame_) {
      trace->dump(g_ticks);
      if (g_frame_idx > last_flush_frame_) {
        trace->flush();
        last_flush_frame_ = g_frame_idx;
      }
    }
  }

private:
  VerilatedFstC *trace;
  uint32_t begin_frame_;
  uint32_t last_flush_frame_ = 0;
};

class FrameDumper {
public:
  FrameDumper() {
    m_FramePixBuf =
        gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, c_Xres, c_Yres);
    gdk_pixbuf_fill(m_FramePixBuf, 0);
  }
  void Tick() {
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
    if (0 <= m_HCntrShifted && m_HCntrShifted < c_Xres && 0 <= m_VCntrShifted &&
        m_VCntrShifted < c_Yres) {
      guchar Red = dut->video_rgb >> 16;
      guchar Green = dut->video_rgb >> 8;
      guchar Blue = dut->video_rgb & 0xff;
      PutPixel(m_FramePixBuf, m_HCntrShifted, m_VCntrShifted, Red, Green, Blue);
    }

    m_HCntr++;

    if (FrameDone) {
      char buf[32];
      snprintf(buf, sizeof(buf), "vicii-%04d.png", g_frame_idx);
      gdk_pixbuf_save(m_FramePixBuf, buf, "png", NULL, NULL);
      printf("%s\n", buf);
    }
  }

private:
  void PutPixel(GdkPixbuf *pixbuf, int x, int y, guchar red, guchar green,
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

  const unsigned c_Xres = 504;
  const unsigned c_Yres = 312;
  GdkPixbuf *m_FramePixBuf;
  unsigned m_HCntr = 0;
  unsigned m_VCntr = 0;
};

class TraceIEC {
public:
  TraceIEC(std::string &path, uint32_t begin_frame)
      : begin_frame_(begin_frame) {
    fp_ = fopen(path.c_str(), "w");
    fprintf(fp_, "atn,clk,dat\n");
  }
  void Tick() {
    if (g_frame_idx >= begin_frame_ && dut->debug_1mhz_ph1_en) {
      fprintf(fp_, "%d,%d,%d\n", dut->debug_iec_atn, dut->debug_iec_clock,
              dut->debug_iec_data);
    }
  }

private:
  FILE *fp_;
  uint32_t begin_frame_;
};

class KeyInject {
public:
  KeyInject(const std::string &keys) {
    std::map<std::string, uint16_t> key_map;
#define DEF_KEY(a, b) key_map[std::string(a)] = b;
#include "keys.def"
#undef DEF_KEY

    std::string::size_type p1 = 0;
    while (p1 < keys.size()) {
      if (keys[p1] == '[') {
        auto p2 = keys.find("]", p1);
        if (p2 == std::string::npos) {
          key_cmds_iter = key_cmds.end();
          return;
        }
        auto frame = keys.substr(p1 + 1, p2 - p1 - 1);
        key_cmds.push_back(std::make_pair(std::stoi(frame), 0));
        p1 = p2 + 1;
      } else if (keys[p1] == '<') {
        auto p2 = keys.find(">", p1);
        if (p2 == std::string::npos) {
          key_cmds_iter = key_cmds.end();
          return;
        }
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
    key_cmds_iter = key_cmds.begin();
  }
  void Tick() {
#if 0
    // Appropriate FIRE sequences to load up 'A Pig Quest' from EasyFlash
    dut->cont1_key = 0;
    if (900 <= g_frame_idx && g_frame_idx < 950) {
      dut->cont1_key = 0x10;
    }
    if (1100 <= g_frame_idx && g_frame_idx < 1150) {
      dut->cont1_key = 0x10;
    }

    if (1285 <= g_frame_idx && g_frame_idx < 1325) {
      dut->cont1_key = 0x10;
    }

    if (1500 <= g_frame_idx && g_frame_idx < 1550) {
      dut->cont1_key = 0x10;
    }
#endif
    // Handle key injection
    if (key_cmds_iter != key_cmds.end()) {
      auto key_cmd = *key_cmds_iter;
      if (key_cmd.first) {
        if (g_frame_idx >= key_cmd.first) {
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
          key_cmds_wait = g_frame_idx + 2;
          key_state = KeyState::Press;
          break;
        case KeyState::Press:
          if (g_frame_idx >= key_cmds_wait) {
            key_cmds_wait = g_frame_idx + 2;
            key_state = KeyState::Release;
          }
          break;
        case KeyState::Release:
          dut->cont3_joy = 0;
          dut->cont3_key = 0;
          if (g_frame_idx >= key_cmds_wait) {
            key_cmds_iter++;
            key_state = KeyState::Idle;
          }
          break;
        }
      }
    }
  }

private:
  enum class KeyState { Idle, Press, Release } key_state;
  unsigned key_cmds_wait = 0;
  std::vector<std::pair<unsigned, uint16_t>> key_cmds;
  decltype(key_cmds)::iterator key_cmds_iter;
};

class BridgeHandler {
public:
  void Tick() {
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
        auto &fs = dataslots[ds_read_slot_id].first;
        for (unsigned i = 0; i < 4; i++) {
          uint8_t byte;
          fs.seekg(ds_read_slot_offset + ds_read_cntr + i, std::ios::beg);
          fs.read(reinterpret_cast<char *>(&byte), 1);
          dut->bridge_wr_data |= static_cast<uint32_t>(byte) << (8 * (3 - i));
        }
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

  void RegisterDataSlot(uint16_t id, const std::string &path) {
    std::ifstream instream(path, std::ios::in | std::ios::binary);
    if (!instream) {
      std::cerr << "Unable to open '" << path << "'\n";
      exit(1);
    }
    instream.seekg(0, std::ios::end);
    auto size = instream.tellg();
    dataslots[id] = std::make_pair(std::move(instream), size);
    if (id < 16) { // Only lower 16 are mapped to update register
      updated_dataslots.push_back(id);
    }
  }
  void Finalize() { updated_dataslots_iter = updated_dataslots.begin(); }

private:
  int bridge_state = 0;
  uint32_t ds_read_slot_id;
  uint32_t ds_read_slot_offset;
  uint32_t ds_read_bridge_address;
  uint32_t ds_read_length;
  uint32_t ds_read_cntr;

  unsigned cntr = 0;

  std::map<uint16_t, std::pair<std::ifstream, uint32_t>> dataslots;
  decltype(dataslots)::iterator ds_it;
  std::vector<uint16_t> updated_dataslots;
  decltype(updated_dataslots)::iterator updated_dataslots_iter;
};

double sc_time_stamp() { return 0; }

int main(int argc, char *argv[]) {
  uint32_t exit_frame = 0;
  bool dump_video = false;

  std::string prg_path;
  std::string g64_path;
  std::string crt_path;

  std::string trace_path;
  std::vector<std::string> trace_modules;
  uint32_t trace_begin_frame = 0;

  std::string cpu_c64_trace_path;
  std::string cpu_c1541_trace_path;

  std::string iec_trace_path;
  uint32_t iec_trace_begin_frame = 0;

  std::string keys_str;

  CLI::App app{"Verilator based MyC64-pocket simulator"};
  app.add_flag("--dump-video", dump_video, "Dump video output as .png");
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
  app.add_option("--cpu-c64-trace", cpu_c64_trace_path,
                 "Instruction trace of the C64 6510 CPU to file");
  app.add_option("--cpu-c1541-trace", cpu_c1541_trace_path,
                 "Instruction trace of the C1541 6502 CPU to file");
  app.add_option(
      "--keys", keys_str,
      "Key input string of the form "
      "'[150]10<SPACE>PRINT<LSHIFT>2HELLO<SPACE>WORLD<LSHIFT>2<RETURN>"
      "20<SPACE>GOTO<SPACE>10<RETURN>RUN<RETURN>'");
  CLI11_PARSE(app, argc, argv);

  // Initialize Verilators variables
  Verilated::commandArgs(argc, argv);
  Verilated::traceEverOn(!trace_path.empty());

  dut = std::make_unique<Vcore_top>();

  //
  // Bridge mockup - always present
  //
  BridgeHandler bridge;

  bridge.RegisterDataSlot(200, "basic.bin");
  bridge.RegisterDataSlot(201, "characters.bin");
  bridge.RegisterDataSlot(202, "kernal.bin");
  bridge.RegisterDataSlot(203, "1540-c000.bin");
  bridge.RegisterDataSlot(204, "1541-e000.bin");

  // Load .prg into slot
  if (!prg_path.empty()) {
    bridge.RegisterDataSlot(PRG_SLOT_ID, prg_path);
  }
  // Load .g64 into slot
  if (!g64_path.empty()) {
    bridge.RegisterDataSlot(G64_SLOT_ID, g64_path);
  }
  // Load .crt into slot
  if (!crt_path.empty()) {
    bridge.RegisterDataSlot(CRT_SLOT_ID, crt_path);
  }

  bridge.Finalize();

#if CLK_32MHZ
  SimplePSRAM psram;
#endif

  std::unique_ptr<TraceRTL> trace_rtl;
  if (!trace_path.empty()) {
    trace_rtl = std::make_unique<TraceRTL>(trace_path, trace_modules,
                                           trace_begin_frame);
  }
  std::unique_ptr<Trace6502> trace_cpu_c64;
  if (!cpu_c64_trace_path.empty()) {
    trace_cpu_c64 = std::make_unique<Trace6502>(
        cpu_c64_trace_path, dut->debug_c64_cpu_valid, dut->debug_c64_cpu_sync,
        dut->debug_c64_cpu_addr, dut->debug_c64_cpu_data,
        dut->debug_c64_cpu_regs);
  }
  std::unique_ptr<Trace6502> trace_cpu_c1541;
  if (!cpu_c1541_trace_path.empty()) {
    trace_cpu_c1541 = std::make_unique<Trace6502>(
        cpu_c1541_trace_path, dut->debug_c1541_cpu_valid,
        dut->debug_c1541_cpu_sync, dut->debug_c1541_cpu_addr,
        dut->debug_c1541_cpu_data, dut->debug_c1541_cpu_regs);
  }
  std::unique_ptr<KeyInject> key_inject;
  if (!keys_str.empty()) {
    key_inject = std::make_unique<KeyInject>(keys_str);
  }

  std::unique_ptr<FrameDumper> framedumper;
  if (dump_video) {
    framedumper = std::make_unique<FrameDumper>();
  }

  std::unique_ptr<TraceIEC> iec_trace;
  if (!iec_trace_path.empty()) {
    iec_trace =
        std::make_unique<TraceIEC>(iec_trace_path, iec_trace_begin_frame);
  }

  dut->reset_n = 0;
  dut->eval();

  unsigned reset_cntr = 0;
  while (!Verilated::gotFinish()) {
    if (reset_cntr++ > 320) {
      dut->reset_n = 1;
    }
#if CLK_32MHZ
    dut->clk_32mhz = !dut->clk_32mhz;
    if (dut->clk_32mhz) {
      psram.Tick();
    }
    if (g_ticks % 4 == 0) {
#endif
      dut->clk_74a = !dut->clk_74a;
      if (dut->clk_74a) {
        // Handle mockup bridge
        bridge.Tick();
        // Key injection
        if (key_inject)
          key_inject->Tick();
        // Frame dumper
        if (framedumper)
          framedumper->Tick();
        // Trace C64 CPU
        if (trace_cpu_c64)
          trace_cpu_c64->Tick();
        // Trace C1541 CPU
        if (trace_cpu_c1541)
          trace_cpu_c1541->Tick();
        // Trace IEC bus
        if (iec_trace)
          iec_trace->Tick();
        // Frame index increment if vsync comes after all handlers
        if (dut->video_vs) {
          g_frame_idx++;
          if (exit_frame != 0 && exit_frame == g_frame_idx) {
            exit(0);
          }
        }
      }
#if CLK_32MHZ
    }
#endif
    dut->eval();
    dut->eval();
    if (trace_rtl) {
      trace_rtl->Tick();
    }
    g_ticks++;
  }

  return 0;
}
