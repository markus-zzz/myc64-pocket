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


class Cpu6502(Elaboratable):

  def __init__(self):
    self.clk_1mhz_ph1_en = Signal()
    self.clk_1mhz_ph2_en = Signal()
    self.o_addr = Signal(16)  # address bus
    self.i_data = Signal(8)  # data in, read bus
    self.o_data = Signal(8)  #data out, write bus
    self.o_we = Signal()  # write enable
    self.i_irq = Signal()  # interrupt request
    self.i_nmi = Signal()  # non-maskable interrupt request
    self.i_so = Signal()

  def elaborate(self, platform):
    m = Module()

    clk = ClockSignal('sync')
    rst = ResetSignal('sync')

    addr = Signal(24)
    data_i = Signal(8)
    data_o = Signal(8)
    we_n = Signal()

    data_ir = Signal(8)
    with m.If(self.clk_1mhz_ph2_en):
      m.d.sync += data_ir.eq(self.i_data)

    m.submodules.u_6502 = Instance('T65',
                                   i_Clk=clk,
                                   i_Enable=self.clk_1mhz_ph1_en,
                                   i_Res_n=~rst,
                                   i_Mode=C(0b00, 2),
                                   i_BCD_en=C(0b1, 1),
                                   i_Rdy=C(0b1, 1),
                                   i_Abort_n=C(0b1, 1),
                                   i_IRQ_n=~self.i_irq,
                                   i_NMI_n=~self.i_nmi,
                                   i_SO_n=C(1,1), #self.i_so,
                                   o_R_W_n=we_n,
                                   o_A=addr,
                                   i_DI=data_ir,
                                   o_DO=data_o)

    m.d.comb += [self.o_addr.eq(addr), self.o_data.eq(data_o), self.o_we.eq(~we_n)]

    return m
