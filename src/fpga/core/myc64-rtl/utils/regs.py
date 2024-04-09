#
# Copyright (C) 2021-2022 Markus Lavin (https://www.zzzconsulting.se/)
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
from typing import NamedTuple

class RegisterFile():
  class RegisterInfoRW(NamedTuple):
    addr: int
    sig: Signal

    def genDefault(self, module):
      pass

    def genAddressed(self, module, bus_wen, bus_rdata, bus_wdata):
      module.d.comb += [bus_rdata.eq(Cat(self.sig, Repl(Const(1), 8 - self.sig.width)))]
      with module.If(bus_wen):
        module.d.sync += [self.sig.eq(bus_wdata[0:self.sig.width])]

  class RegisterInfo(NamedTuple):
    addr: int
    readStrobe: Signal
    readSignal: Signal
    writeStrobe: Signal
    writeSignal: Signal

    def genDefault(self, module):
      if self.readStrobe is not None:
        module.d.comb += [self.readStrobe.eq(0)]
      if self.writeStrobe is not None:
        module.d.comb += [self.writeStrobe.eq(0)]
      if self.writeSignal is not None:
        module.d.comb += [self.writeSignal.eq(0)]

    def genAddressed(self, module, bus_wen, bus_rdata, bus_wdata):
      if self.readStrobe is not None:
        module.d.comb += [self.readStrobe.eq(1)]
      module.d.comb += [bus_rdata.eq(self.readSignal)]
      with module.If(bus_wen):
        if self.writeStrobe is not None:
          module.d.comb += [self.writeStrobe.eq(1)]
        if self.writeSignal is not None:
          module.d.comb += [self.writeSignal.eq(bus_wdata)]

  def __init__(self, base):
    self._base = base
    self._regmap = {}

  def addRegRW(self, addr, width):
    sig_name = 'r_{:x}'.format(addr)
    reginfo = self.RegisterInfoRW(addr=addr, sig=Signal(width, name=sig_name))
    self._regmap[addr] = reginfo
    return reginfo.sig

  def addReg(self, addr, readStrobe, readSignal, writeStrobe, writeSignal):
    reginfo = self.RegisterInfo(addr=addr,
                                readStrobe=readStrobe,
                                readSignal=readSignal,
                                writeStrobe=writeStrobe,
                                writeSignal=writeSignal)
    self._regmap[addr] = reginfo

  def genInterface(self, module, bus_addr, bus_wen, bus_rdata, bus_wdata):
    # Setup default drivers.
    for addr, reginfo in self._regmap.items():
      reginfo.genDefault(module)
    # Address decode.
    with module.Switch(bus_addr):
      for addr, reginfo in self._regmap.items():
        with module.Case(addr - self._base):
          reginfo.genAddressed(module, bus_wen, bus_rdata, bus_wdata)
