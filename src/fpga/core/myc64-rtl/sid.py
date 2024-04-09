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

class Sid(Elaboratable):
  def __init__(self):

    self.i_clk_1mhz_ph1_en = Signal()
    self.i_cs = Signal()
    self.i_addr = Signal(5)
    self.i_we = Signal()
    self.i_data = Signal(8)
    self.o_data = Signal(8)
    self.o_wave = Signal(16)

  def elaborate(self, platform):
    m = Module()

    clk = ClockSignal('sync')
    rst = ResetSignal('sync')

    m.submodules.u_sid = Instance('sid',
                                  i_clk=clk,
                                  i_rst=rst,
                                  i_clk_1mhz_ph1_en=self.i_clk_1mhz_ph1_en,
                                  i_i_cs=self.i_cs,
                                  i_i_addr=self.i_addr,
                                  i_i_we=self.i_we,
                                  i_i_data=self.i_data,
                                  o_o_data=self.o_data,
                                  o_o_wave=self.o_wave)

    return m
