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
    self.i_track_len = Signal(13)

    self.o_track_no = Signal(7)
    self.o_led_on = Signal()
    self.o_motor_on = Signal()

    self.i_clk_1mhz_ph1_en = Signal()
    self.i_clk_1mhz_ph2_en = Signal()

    self.i_iec_atn_in = Signal()
    self.i_iec_data_in = Signal()
    self.o_iec_data_out = Signal()
    self.i_iec_clock_in = Signal()
    self.o_iec_clock_out = Signal()

    self.ports = [
        self.o_addr, self.i_rom_data, self.i_ram_data, self.o_ram_data, self.o_ram_we, self.o_track_addr,
        self.i_track_data, self.i_track_len, self.o_track_no, self.o_led_on, self.o_motor_on, self.i_clk_1mhz_ph1_en,
        self.i_clk_1mhz_ph2_en, self.i_iec_atn_in, self.i_iec_data_in, self.o_iec_data_out, self.i_iec_clock_in,
        self.o_iec_clock_out
    ]

  def elaborate(self, platform):
    m = Module()

    # 6502 CPU.
    m.submodules.u_cpu_ = u_cpu_ = Cpu6502()

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
        # CPU
        u_cpu_.i_data.eq(cpu_di),
        u_cpu_.i_irq.eq(u_via1.o_irq | u_via2.o_irq),
        cpu_addr.eq(u_cpu_.o_addr),
        cpu_we.eq(u_cpu_.o_we),
        cpu_do.eq(u_cpu_.o_data),
        u_cpu_.clk_1mhz_ph1_en.eq(self.i_clk_1mhz_ph1_en),
        u_cpu_.clk_1mhz_ph2_en.eq(self.i_clk_1mhz_ph2_en),
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
        #
        self.o_addr.eq(cpu_addr),
        self.o_ram_data.eq(cpu_do),
        self.o_ram_we.eq(cpu_we & ram_cs),
    ]

    decoder_enable = Signal()
    decoder_enable_cntr = Signal(2)
    m.d.comb += decoder_enable.eq(self.i_clk_1mhz_ph2_en & (decoder_enable_cntr == 0))
    with m.If(self.i_clk_1mhz_ph2_en):
      m.d.sync += decoder_enable_cntr.eq(decoder_enable_cntr + 1)
    #
    # Handle track memory
    #
    track_bit_cntr = Signal(range(8192 * 8))
    track_bit_cntr_p = Signal(3)
    track_byte_shift = Signal(8)

    m.d.comb += self.o_track_addr.eq(track_bit_cntr[5:])
    with m.If(decoder_enable):
      m.d.sync += [track_bit_cntr.eq(track_bit_cntr + 1), track_bit_cntr_p.eq(track_bit_cntr)]

      with m.If(track_bit_cntr[3:] == self.i_track_len):
        m.d.sync += track_bit_cntr.eq(0)

      with m.If(track_bit_cntr_p == 0):
        m.d.sync += track_byte_shift.eq(self.i_track_data.word_select(track_bit_cntr[3:5], 8))
      with m.Else():
        m.d.sync += track_byte_shift.eq(Cat(C(0b0, 1), track_byte_shift[0:7]))

    #
    # Sample incomming bits
    #
    block_sync = Signal()
    byte_sync = Signal()
    byte = Signal(8)

    read_bits = Signal(10)
    bit_cntr = Signal(3)

    m.d.comb += byte_sync.eq(decoder_enable & (bit_cntr == 0b111))
    m.d.comb += [byte.eq(read_bits[0:8]), block_sync.eq(read_bits.all())]
    with m.If(decoder_enable):
      m.d.sync += [bit_cntr.eq(bit_cntr + 1), read_bits.eq(Cat(track_byte_shift[7], read_bits[0:9]))]
      with m.If(block_sync):
        m.d.sync += bit_cntr.eq(0)

    # XXX:TODO: Wire these signals to VIA2
    head_step_dir = Signal(2)
    head_step_dir_p = Signal(2)
    motor_ctrl = Signal()
    led_ctrl = Signal()
    write_protect = Signal()
    data_density = Signal(2)

    atna = Signal()

    m.d.comb += [
        # VIA-1
        u_via1.i_pb[0].eq(~self.i_iec_data_in),
        self.o_iec_data_out.eq(~u_via1.o_pb[1] & ~(atna ^ ~self.i_iec_atn_in)),
        u_via1.i_pb[2].eq(~self.i_iec_clock_in),
        self.o_iec_clock_out.eq(~u_via1.o_pb[3]),
        atna.eq(u_via1.o_pb[4]),
        u_via1.i_pb[7].eq(~self.i_iec_atn_in),
        u_via1.i_ca1.eq(~self.i_iec_atn_in),
        # VIA-2
        u_via2.i_pa.eq(byte),
        u_via2.i_ca1.eq(~byte_sync),
        u_cpu_.i_so.eq(byte_sync),
        head_step_dir.eq(u_via2.o_pb[0:2]),
        motor_ctrl.eq(u_via2.o_pb[2]),
        led_ctrl.eq(u_via2.o_pb[3]),
        u_via2.i_pb[4].eq(write_protect),
        data_density.eq(u_via2.o_pb[5:7]),
        u_via2.i_pb[7].eq(~block_sync),
        self.o_led_on.eq(led_ctrl),
        self.o_motor_on.eq(motor_ctrl),
    ]

    #
    # Seek track
    #
    track_no = Signal(7)
    m.d.comb += self.o_track_no.eq(track_no)
    m.d.sync += head_step_dir_p.eq(head_step_dir)
    with m.If((track_no < 84) & (head_step_dir == (head_step_dir_p + 1)[0:2])):
      m.d.sync += track_no.eq(track_no + 1)
    with m.Elif((track_no > 0) & (head_step_dir == (head_step_dir_p - 1)[0:2])):
      m.d.sync += track_no.eq(track_no - 1)

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
