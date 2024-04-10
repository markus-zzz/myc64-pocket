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
from cpu6502 import Cpu6502
from via import VIA


class My1541(Elaboratable):

  def __init__(self):

    self.o_addr = Signal(16)

    self.i_rom_data = Signal(8)

    self.i_ram_data = Signal(8)
    self.o_ram_data = Signal(8)
    self.o_ram_we = Signal()

    self.o_track_addr = Signal(11)
    self.i_track_data = Signal(32)

    self.i_clk_1mhz_ph1_en = Signal()
    self.i_clk_1mhz_ph2_en = Signal()

    self.ports = [
        self.o_addr, self.i_rom_data, self.i_ram_data, self.o_ram_data, self.o_ram_we, self.o_track_addr,
        self.i_track_data, self.i_clk_1mhz_ph1_en, self.i_clk_1mhz_ph2_en
    ]

  def elaborate(self, platform):
    m = Module()

    # 6502 CPU.
    m.submodules.u_cpu = u_cpu = Cpu6502()

    # VIA-1.
    m.submodules.u_via1 = u_via1 = VIA()

    # VIA-2.
    m.submodules.u_via2 = u_via2 = VIA()

    cpu_di = Signal(8)
    ram_cs = Signal()
    rom_cs = Signal()
    via1_cs = Signal()
    via2_cs = Signal()

    cpu_addr = Signal(16)
    cpu_we = Signal()
    cpu_di = Signal(8)
    cpu_do = Signal(8)

    # RAM (2KB)
    with m.If(cpu_addr <= 0x07FF):
      m.d.comb += [cpu_di.eq(self.i_ram_data), ram_cs.eq(1)]
    # VIA1
    with m.Elif((0x1800 <= cpu_addr) & (cpu_addr <= 0x180F)):
      m.d.comb += [cpu_di.eq(u_via1.o_data), via1_cs.eq(1)]
    # VIA2
    with m.Elif((0x1C00 <= cpu_addr) & (cpu_addr <= 0x1C0F)):
      m.d.comb += [cpu_di.eq(u_via2.o_data), via2_cs.eq(1)]
    # ROM (16KB)
    with m.Elif((0xC000 <= cpu_addr) & (cpu_addr <= 0xFFFF)):
      m.d.comb += [cpu_di.eq(self.i_rom_data), rom_cs.eq(1)]

    #
    #
    #

    m.d.comb += [
        u_cpu.i_data.eq(cpu_di),
        u_cpu.i_irq.eq(0b0),
        cpu_addr.eq(u_cpu.o_addr),
        cpu_we.eq(u_cpu.o_we),
        cpu_do.eq(u_cpu.o_data),
        u_cpu.clk_1mhz_ph1_en.eq(self.i_clk_1mhz_ph1_en),
        u_cpu.clk_1mhz_ph2_en.eq(self.i_clk_1mhz_ph2_en),
        # VIA-1
        u_via1.clk_1mhz_ph_en.eq(self.i_clk_1mhz_ph2_en),
        u_via1.i_cs.eq(via1_cs),
        u_via1.i_addr.eq(cpu_addr),
        u_via1.i_we.eq(cpu_we),
        u_via1.i_data.eq(cpu_do),
        # VIA-2
        u_via2.clk_1mhz_ph_en.eq(self.i_clk_1mhz_ph2_en),
        u_via2.i_cs.eq(via2_cs),
        u_via2.i_addr.eq(cpu_addr),
        u_via2.i_we.eq(cpu_we),
        u_via2.i_data.eq(cpu_do),
        u_via2.i_pa.eq(C(0xff, 8)),
        #
        self.o_addr.eq(cpu_addr),
        self.o_ram_data.eq(cpu_do),
        self.o_ram_we.eq(cpu_we & ram_cs)
    ]

    return m


#
# Generate verilog
#

from amaranth.back import verilog
import sys
import os

if __name__ == "__main__":

  my1541 = My1541()

  with open("my1541.v", "w") as f:
    f.write(verilog.convert(elaboratable=my1541, name='my1541_top', ports=my1541.ports))
