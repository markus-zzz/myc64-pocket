#
# Copyright (C) 2020-2024 Markus Lavin (https://www.zzzconsulting.se/)
#
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# yapf --in-place --recursive --style="{indent_width: 2, column_limit: 120}"

from amaranth import *

from enum import IntEnum, unique
from typing import NamedTuple
from utils.regs import RegisterFile


class VicII(Elaboratable):
  def __init__(self):
    self.clk_8mhz_en = Signal()
    self.clk_1mhz_ph1_en = Signal()
    self.clk_1mhz_ph2_en = Signal()
    self.o_addr = Signal(14)
    self.i_data = Signal(12)
    self.i_reg_addr = Signal(6)
    self.i_reg_cs = Signal()
    self.i_reg_we = Signal()
    self.i_reg_data = Signal(8)
    self.o_reg_data = Signal(8)
    self.o_steal_bus = Signal()
    self.o_irq = Signal()
    self.o_color = Signal(4)
    self.o_hsync = Signal()
    self.o_vsync = Signal()
    self.o_visib = Signal()

    self.ports = [
        self.clk_8mhz_en, self.clk_1mhz_ph1_en, self.clk_1mhz_ph2_en, self.o_addr, self.i_data, self.i_reg_addr,
        self.i_reg_cs, self.i_reg_we, self.i_reg_data, self.o_reg_data, self.o_steal_bus, self.o_irq, self.o_color,
        self.o_hsync, self.o_vsync
    ]

  def elaborate(self, platform):
    m = Module()
    p_x_raster_last = 0x190

    x = Signal(9)
    y = Signal(9)
    vc = Signal(10)
    vcbase = Signal(10)
    rc = Signal(3)
    vml = Array([Signal(12) for _ in range(40)])
    vmli = Signal(range(40))
    cycle = Signal(6)

    bad_line_cond = Signal()
    display_window_x = Signal()
    display_window_y = Signal()
    display_not_idle_state = Signal()

    # Sprite registers (internal)
    mc = Array([Signal(6) for _ in range(8)])
    sprite_shift = Array([Signal(24) for _ in range(8)])
    sprite_shift_2msb = Array([Signal(2) for _ in range(8)])
    sprite_shift_toggle = Array([Signal(1) for _ in range(8)])
    sprite_out_color = Array([Signal(4) for _ in range(8)])
    sprite_out_valid = Signal(8)

    sprite_final_out_color = Signal(4)
    sprite_final_out_priority = Signal()
    sprite_final_out_valid = Signal()

    graph_color = Signal(4)
    graph_color_is_fg = Signal()

    # X and Y counters with wrap logic.
    with m.If(self.clk_8mhz_en):
      m.d.sync += [x.eq(Mux(x == 0x1f7, 0, x + 1))]
      with m.If(x == p_x_raster_last):
        m.d.sync += [y.eq(Mux(y == 311, 0, y + 1))]

    with m.If(self.clk_8mhz_en):
      with m.If(x == p_x_raster_last):
        m.d.sync += [cycle.eq(1)]
      with m.Elif(self.clk_1mhz_ph1_en):
        m.d.sync += [cycle.eq(cycle + 1)]

    with m.If(self.clk_1mhz_ph2_en):
      with m.If((cycle == 14) & bad_line_cond):
        m.d.sync += [rc.eq(0), display_not_idle_state.eq(1)]
      with m.Elif((cycle == 58)):
        m.d.sync += [rc.eq(rc + 1)]
        with m.If(rc == 7):
          m.d.sync += [display_not_idle_state.eq(0)]

    raster = Signal(9)
    raster_irq = Signal(9)
    irq = Signal(4)
    m.d.comb += [raster.eq(y[0:9])]

    rf = RegisterFile(base=0xd000)

    r_d011 = Signal(7)
    r_d011_writeStrobe = Signal()
    r_d011_writeSignal = Signal(8)
    rf.addReg(addr=0xd011,
              readStrobe=None,
              readSignal=Cat(r_d011[0:7], raster[8]),
              writeStrobe=r_d011_writeStrobe,
              writeSignal=r_d011_writeSignal)

    r_d012_writeStrobe = Signal()
    r_d012_writeSignal = Signal(8)
    rf.addReg(addr=0xd012,
              readStrobe=None,
              readSignal=raster[0:8],
              writeStrobe=r_d012_writeStrobe,
              writeSignal=r_d012_writeSignal)  # Raster line.

    r_d018 = rf.addRegRW(addr=0xd018, width=8)  # Memory setup.

    r_d019_writeStrobe = Signal()
    r_d019_writeSignal = Signal(4)
    rf.addReg(addr=0xd019,
              readStrobe=None,
              readSignal=Cat(irq, C(0b111, 3), irq.any()),
              writeStrobe=r_d019_writeStrobe,
              writeSignal=r_d019_writeSignal)

    r_d020 = rf.addRegRW(addr=0xd020, width=4)  # Border color.
    r_d021 = rf.addRegRW(addr=0xd021, width=4)  # Background color.
    r_d022 = rf.addRegRW(addr=0xd022, width=4)  # Extra background color #1.
    r_d023 = rf.addRegRW(addr=0xd023, width=4)  # Extra background color #2.
    r_d024 = rf.addRegRW(addr=0xd024, width=4)  # Extra background color #3.
    r_d025 = rf.addRegRW(addr=0xd025, width=4)  # Sprite extra color #1.
    r_d026 = rf.addRegRW(addr=0xd026, width=4)  # Sprite extra color #2.

    sprites_x_bit_0_7 = Array([rf.addRegRW(addr=addr, width=8) for addr in range(0xd000, 0xd010, 2)])
    sprites_y = Array([rf.addRegRW(addr=addr, width=8) for addr in range(0xd001, 0xd010, 2)])
    r_d010 = rf.addRegRW(addr=0xd010, width=8)
    r_d015 = rf.addRegRW(addr=0xd015, width=8)
    r_d016 = rf.addRegRW(addr=0xd016, width=8)
    r_d01a = rf.addRegRW(addr=0xd01a, width=4)
    r_d01b = rf.addRegRW(addr=0xd01b, width=8)
    r_d01c = rf.addRegRW(addr=0xd01c, width=8)
    sprites_color = Array([rf.addRegRW(addr=addr, width=4) for addr in range(0xd027, 0xd02f, 1)])

    mode_ecm = r_d011[6] # Extended Color Mode
    mode_bmm = r_d011[5] # Bit Map Mode
    mode_mcm = r_d016[4] # Multi Color Mode

    mode_screen_on = r_d011[4] # Screen on, normal content
    mode_40_columns = r_d016[3] # 40 coulmns mode

    with m.If(x == Mux(mode_40_columns, 0x14, 0x14 + 8)):
      m.d.sync += [display_window_x.eq(1)]
    with m.Elif(x == Mux(mode_40_columns, 0x14 + 8 * 40, 0x14 + 8 + 8 * 38)):
      m.d.sync += [display_window_x.eq(0)]

    # Sprite to sprite collisions
    sprite2sprite_col = Signal(8)
    r_d01e_readStrobe = Signal()
    rf.addReg(addr=0xd01e,
              readStrobe=r_d01e_readStrobe, # XXX: Fix regs.py to include self.i_reg_cs
              readSignal=sprite2sprite_col,
              writeStrobe=None,
              writeSignal=None)
    with m.If(~self.o_steal_bus & self.clk_1mhz_ph2_en & self.i_reg_cs & r_d01e_readStrobe):
      m.d.sync += sprite2sprite_col.eq(0)
    for idx in range(8):
      with m.If(sprite_out_valid[idx] & (sprite_out_valid & ~(C(1, 8) << idx)).any()):
        m.d.sync += sprite2sprite_col[idx].eq(1)

    with m.If(sprite2sprite_col.any()):
      m.d.sync += irq[2].eq(1)

    # Sprite to graphics collisions
    sprite2graph_col = Signal(8)
    r_d01f_readStrobe = Signal()
    rf.addReg(addr=0xd01f,
              readStrobe=r_d01f_readStrobe, # XXX: Fix regs.py to include self.i_reg_cs
              readSignal=sprite2graph_col,
              writeStrobe=None,
              writeSignal=None)
    with m.If(~self.o_steal_bus & self.clk_1mhz_ph2_en & self.i_reg_cs & r_d01f_readStrobe):
      m.d.sync += sprite2graph_col.eq(0)
    for idx in range(8):
      with m.If(sprite_out_valid[idx] & graph_color_is_fg):
        m.d.sync += sprite2graph_col[idx].eq(1)

    with m.If(sprite2graph_col.any()):
      m.d.sync += irq[1].eq(1)

    # Raster IRQ
    with m.If(self.clk_8mhz_en):
      with m.If(
          (cycle == 0) & (raster == raster_irq)
      ):  # XXX: should we check the corresponding interrupt enable bit here or is it just for the final IRQ gen?
        m.d.sync += irq[0].eq(1)

    with m.If(r_d011[3]):
      m.d.comb += [display_window_y.eq((raster >= 0x33) & (raster <= 0xfa))]
    with m.Else():
      m.d.comb += [display_window_y.eq((raster >= 0x37) & (raster <= 0xf6))]

    m.d.comb += [bad_line_cond.eq((raster >= 0x30) & (raster <= 0xf7) & (y[0:3] == r_d011[0:3]))]

    fgcolor = Signal(12)
    fgcolor_staging = Signal(12)

    pixshift = Signal(8)
    pixshift_staging = Signal(8)
    pixshift_2msb = Signal(2)
    pixshift_toggle = Signal()

    m.d.comb += [self.o_hsync.eq(x == p_x_raster_last),
                 self.o_vsync.eq((y == 0) & (x == (p_x_raster_last - 3))),
                 self.o_visib.eq((cycle >= 13) & (cycle < 13 + 4 + 40 + 4) & (raster >= 0x30 - 32) & (raster <= 0xf7 + 32))]

    sprite_idx = Signal(range(8))
    refresh_idx = Signal(range(5))
    sprite_ptr = Signal(8)
    sprite_dma_on = Signal(8)
    sprite_shift_on = Signal(8)

    with m.If(~pixshift_toggle):
      m.d.sync += pixshift_2msb.eq(pixshift[6:8])

    # Generate the eight sprites
    for idx in range(8):
      m.d.comb += sprite_out_valid[idx].eq(0)
      with m.If(sprite_shift_on[idx]):
        with m.If(r_d01c[idx]): # Multicolor
          sprite_bits = Mux(sprite_shift_toggle[idx], sprite_shift_2msb[idx], sprite_shift[idx][22:24])
          with m.Switch(sprite_bits):
            with m.Case(0b00):
              pass
            with m.Case(0b01):
              m.d.comb += [sprite_out_valid[idx].eq(1), sprite_out_color[idx].eq(r_d025)]
            with m.Case(0b10):
              m.d.comb += [sprite_out_valid[idx].eq(1), sprite_out_color[idx].eq(sprites_color[idx])]
            with m.Case(0b11):
              m.d.comb += [sprite_out_valid[idx].eq(1), sprite_out_color[idx].eq(r_d026)]
        with m.Else():
          sprite_bit = sprite_shift[idx][23]
          with m.If(sprite_bit):
            m.d.comb += [sprite_out_valid[idx].eq(1), sprite_out_color[idx].eq(sprites_color[idx])]

    # Sprite priority 0-7
    m.d.comb += sprite_final_out_valid.eq(0)
    with m.If(False):
      pass
    for idx in range(8):
      with m.Elif(sprite_out_valid[idx]):
        m.d.comb += [sprite_final_out_color.eq(sprite_out_color[idx]),
                     sprite_final_out_priority.eq(r_d01b[idx]),
                     sprite_final_out_valid.eq(1)]

    # Text / graphics
    m.d.comb += graph_color_is_fg.eq(0)
    with m.If(~mode_ecm & ~mode_bmm & ~mode_mcm): # Standard text mode (ECM/BMM/MCM=0/0/0)
      with m.If(pixshift[7]):
        m.d.comb += [graph_color.eq(fgcolor[8:12]), graph_color_is_fg.eq(1)]
      with m.Else():
        m.d.comb += graph_color.eq(r_d021)

    with m.Elif(~mode_ecm & ~mode_bmm & mode_mcm): # Multicolor text mode (ECM/BMM/MCM=0/0/1)
      with m.If(fgcolor[11]):
        with m.Switch(Mux(pixshift_toggle, pixshift_2msb, pixshift[6:8])):
          with m.Case(0b00):
            m.d.comb += graph_color.eq(r_d021)
          with m.Case(0b01):
            m.d.comb += graph_color.eq(r_d022)
          with m.Case(0b10):
            m.d.comb += [graph_color.eq(r_d023), graph_color_is_fg.eq(1)]
          with m.Case(0b11):
            m.d.comb += [graph_color.eq(fgcolor[8:11]), graph_color_is_fg.eq(1)]
      with m.Else():
        with m.If(pixshift[7]):
          m.d.comb += [graph_color.eq(fgcolor[8:12]), graph_color_is_fg.eq(1)]
        with m.Else():
          m.d.comb += graph_color.eq(r_d021)

    with m.Elif(~mode_ecm & mode_bmm & ~mode_mcm): # Standard bitmap mode (ECM/BMM/MCM=0/1/0)
      with m.If(pixshift[7]):
        m.d.comb += [graph_color.eq(fgcolor[4:8]), graph_color_is_fg.eq(1)]
      with m.Else():
        m.d.comb += graph_color.eq(fgcolor[0:4])

    with m.Elif(~mode_ecm & mode_bmm & mode_mcm): # Multicolor bitmap mode (ECM/BMM/MCM=0/1/1)
      with m.Switch(Mux(pixshift_toggle, pixshift_2msb, pixshift[6:8])):
        with m.Case(0b00):
          m.d.comb += graph_color.eq(r_d021)
        with m.Case(0b01):
          m.d.comb += graph_color.eq(fgcolor[4:8])
        with m.Case(0b10):
          m.d.comb += [graph_color.eq(fgcolor[0:4]), graph_color_is_fg.eq(1)]
        with m.Case(0b11):
          m.d.comb += [graph_color.eq(fgcolor[8:12]), graph_color_is_fg.eq(1)]

    with m.Elif(mode_ecm & ~mode_bmm & ~mode_mcm): # ECM text mode (ECM/BMM/MCM=1/0/0)
      with m.If(pixshift[7]):
        m.d.comb += graph_color.eq(fgcolor[8:12])
      with m.Else():
        with m.Switch(fgcolor[6:8]):
          with m.Case(0b00):
            m.d.comb += graph_color.eq(r_d021)
          with m.Case(0b01):
            m.d.comb += graph_color.eq(r_d022)
          with m.Case(0b10):
            m.d.comb += graph_color.eq(r_d023)
          with m.Case(0b11):
            m.d.comb += graph_color.eq(r_d024)


    m.d.comb += [self.o_color.eq(r_d020)]  # Border color.
    with m.If(mode_screen_on & display_window_x & display_window_y):
      m.d.comb += self.o_color.eq(graph_color)
      with m.If(sprite_final_out_valid):
        with m.If(~sprite_final_out_priority | ~graph_color_is_fg):
          m.d.comb += self.o_color.eq(sprite_final_out_color)

    vic_owns_ph1 = Signal()

    m.d.comb += [self.o_steal_bus.eq(vic_owns_ph1)]

    # DEBUG - begin
    for idx in range(8):
      s = Signal(24, name='debug_sprite_shift_{}'.format(idx))
      m.d.comb += s.eq(sprite_shift[idx])
      s = Signal(8, name='debug_sprites_y_{}'.format(idx))
      m.d.comb += s.eq(sprites_y[idx])
      s = Signal(9, name='debug_sprites_x_{}'.format(idx))
      m.d.comb += s.eq(Cat(sprites_x_bit_0_7[idx], r_d010[idx]))
      s = Signal(6, name='debug_mc_{}'.format(idx))
      m.d.comb += s.eq(mc[idx])
      s = Signal(1, name='debug_sprite_shift_toggle_{}'.format(idx))
      m.d.comb += s.eq(sprite_shift_toggle[idx])
      s = Signal(2, name='debug_sprite_shift_2msb_{}'.format(idx))
      m.d.comb += s.eq(sprite_shift_2msb[idx])
      sprite_bits = Mux(sprite_shift_toggle[idx], sprite_shift_2msb[idx], sprite_shift[idx][22:24])
      s = Signal(2, name='debug_sprite_bits_{}'.format(idx))
      m.d.comb += s.eq(sprite_bits)
    # DEBUG - end

    with m.If(self.clk_8mhz_en):
      for idx in range(8):
        with m.If(r_d015[idx] & (Cat(sprites_x_bit_0_7[idx], r_d010[idx]) == x + 5)): # XXX: The +5 part is a hack.
          m.d.sync += [sprite_shift_on[idx].eq(1), sprite_shift_toggle[idx].eq(0)]

    with m.If(self.clk_8mhz_en):
      m.d.sync += pixshift.eq(Cat(Const(0, 1), pixshift[0:7]))
      for idx in range(8):
        with m.If(sprite_shift_on[idx]):
          m.d.sync += [sprite_shift[idx].eq(Cat(Const(0, 1), sprite_shift[idx][0:23])),
                       sprite_shift_toggle[idx].eq(~sprite_shift_toggle[idx])]
          with m.If(~sprite_shift_toggle[idx]):
            m.d.sync += sprite_shift_2msb[idx].eq(sprite_shift[idx][22:24])

    m.d.comb += [vic_owns_ph1.eq(0)]

    #
    # Sequencer.
    #
    with m.FSM(reset='idle') as fsm:

      with m.State('idle'):
        with m.If(self.clk_1mhz_ph2_en):
          m.d.sync += [sprite_idx.eq(0), refresh_idx.eq(0), vmli.eq(0)]
          with m.If(y == 0):
            m.d.sync += [vcbase.eq(0)]
          with m.If(cycle == 58):
            # Disable all sprite shifters.
            m.d.sync += [sprite_shift_on.eq(0)]
            # In parallel; activate the sprite DMA if sprite enabled ($d015)
            # and Y-coordinate matches.
            for idx in range(8):
              with m.If(r_d015[idx] & (sprites_y[idx] == raster)):
                m.d.sync += [sprite_dma_on[idx].eq(1), mc[idx].eq(0)]
            m.next = 'p-access'

      with m.State('p-access'):
        m.d.comb += [self.o_addr.eq(Cat(sprite_idx, C(0b11_1111_1, 7), r_d018[4:8]))]  # Sprite Pointers.
        with m.If(self.clk_1mhz_ph1_en):
          m.d.sync += [sprite_ptr.eq(self.i_data[0:8])]
          m.next = 's-access-0'

      with m.State('s-access-0'):
        with m.If(sprite_dma_on.bit_select(sprite_idx, 1)):
          m.d.comb += [vic_owns_ph1.eq(1)]
          m.d.comb += [self.o_addr.eq(Cat(mc[sprite_idx], sprite_ptr))]
        with m.If(self.clk_1mhz_ph2_en):
          with m.If(sprite_dma_on.bit_select(sprite_idx, 1)):
            m.d.sync += [sprite_shift[sprite_idx][16:24].eq(self.i_data[0:8]), mc[sprite_idx].eq(mc[sprite_idx] + 1)]
          m.next = 's-access-1'

      with m.State('s-access-1'):
        with m.If(sprite_dma_on.bit_select(sprite_idx, 1)):
          m.d.comb += [vic_owns_ph1.eq(1)]
          m.d.comb += [self.o_addr.eq(Cat(mc[sprite_idx], sprite_ptr))]
        with m.If(self.clk_1mhz_ph1_en):
          with m.If(sprite_dma_on.bit_select(sprite_idx, 1)):
            m.d.sync += [sprite_shift[sprite_idx][8:16].eq(self.i_data[0:8]), mc[sprite_idx].eq(mc[sprite_idx] + 1)]
          m.next = 's-access-2'

      with m.State('s-access-2'):
        with m.If(sprite_dma_on.bit_select(sprite_idx, 1)):
          m.d.comb += [vic_owns_ph1.eq(1)]
          m.d.comb += [self.o_addr.eq(Cat(mc[sprite_idx], sprite_ptr))]
        with m.If(self.clk_1mhz_ph2_en):
          with m.If(sprite_dma_on.bit_select(sprite_idx, 1)):
            m.d.sync += [sprite_shift[sprite_idx][0:8].eq(self.i_data[0:8]), mc[sprite_idx].eq(mc[sprite_idx] + 1)]
            with m.If(mc[sprite_idx] == 3 * 21 - 1):
              m.d.sync += [sprite_dma_on.bit_select(sprite_idx, 1).eq(0)]
          with m.If(sprite_idx == 7):
            m.next = 'refresh'
          with m.Else():
            m.d.sync += [sprite_idx.eq(sprite_idx + 1)]
            m.next = 'p-access'

      with m.State('refresh'):
        with m.If(self.clk_1mhz_ph1_en):
          with m.If(refresh_idx == 4):
            m.d.sync += vc.eq(vcbase)
            m.next = 'c-access'
          with m.Else():
            m.d.sync += [refresh_idx.eq(refresh_idx + 1)]
            m.next = 'refresh'

      with m.State('c-access'):
        m.d.comb += [vic_owns_ph1.eq(bad_line_cond)]
        m.d.comb += [self.o_addr.eq(Cat(vc, r_d018[4:8]))]  # Video Matrix.
        with m.If(self.clk_1mhz_ph2_en):
          m.next = 'g-access'
          with m.If(bad_line_cond):
            m.d.sync += [vml[vmli].eq(self.i_data)]

      with m.State('g-access'):
        m.d.comb += [vic_owns_ph1.eq(bad_line_cond)]
        with m.If(mode_bmm):
          m.d.comb += [self.o_addr.eq(Cat(rc[0:3], vc[0:10], r_d018[3]))]
        with m.Else():
          with m.If(mode_ecm):
            m.d.comb += [self.o_addr.eq(Cat(rc[0:3], Cat(vml[vmli][0:6], C(0b00, 2)), r_d018[1:4]))]
          with m.Else():
            m.d.comb += [self.o_addr.eq(Cat(rc[0:3], vml[vmli][0:8], r_d018[1:4]))]
        with m.If(self.clk_1mhz_ph1_en):
          with m.If(display_not_idle_state):
            m.d.sync += [pixshift_staging.eq(self.i_data[0:8])]
            m.d.sync += fgcolor_staging.eq(vml[vmli])
          m.d.sync += [vc.eq(vc + 1)]
          with m.If(vmli == 39):
            m.next = 'eol'
          with m.Else():
            m.d.sync += [vmli.eq(vmli + 1)]
            m.next = 'c-access'

      with m.State('eol'):
        with m.If(self.clk_1mhz_ph1_en):
          with m.If(display_not_idle_state & (rc == 0b111)):
            m.d.sync += vcbase.eq(vc)
          m.next = 'idle'

    m.d.sync += pixshift_toggle.eq(~pixshift_toggle)
    fudge = C(4, 3)
    with m.If(x[0:3] == (r_d016[0:3] + fudge)[0:3]):
      m.d.sync += [pixshift.eq(pixshift_staging), pixshift_toggle.eq(0), fgcolor.eq(fgcolor_staging)]


    #
    # Register interface.
    #
    tmp_bus_wen = (self.clk_1mhz_ph2_en & self.i_reg_cs & self.i_reg_we)
    rf.genInterface(module=m,
                    bus_addr=self.i_reg_addr,
                    bus_wen=tmp_bus_wen,
                    bus_rdata=self.o_reg_data,
                    bus_wdata=self.i_reg_data)

    with m.If(tmp_bus_wen & r_d011_writeStrobe):
      m.d.sync += [r_d011.eq(r_d011_writeSignal[0:7]), raster_irq[8].eq(r_d011_writeSignal[7])]
    with m.If(tmp_bus_wen & r_d012_writeStrobe):
      m.d.sync += [raster_irq[0:8].eq(r_d012_writeSignal)]
    with m.If(tmp_bus_wen & r_d019_writeStrobe): # XXX: Bug: acknowledge should not have priority over the source that sets
      m.d.sync += [irq.eq(irq ^ (irq & r_d019_writeSignal))]

    m.d.comb += [self.o_irq.eq((irq & r_d01a).any())]

    return m
