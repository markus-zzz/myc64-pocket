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
from enum import Enum

# https://www.zimmers.net/anonftp/pub/cbm/documents/chipdata/6522-VIA.txt


class VIA(Elaboratable):

  def __init__(self):
    self.clk_1mhz_ph_en = Signal()
    self.i_cs = Signal()
    self.i_addr = Signal(4)
    self.i_we = Signal()
    self.i_data = Signal(8)
    self.o_data = Signal(8)

    self.i_pa = Signal(8)
    self.o_pa = Signal(8)
    self.i_pb = Signal(8)
    self.o_pb = Signal(8)

    self.i_ca1 = Signal()
    self.i_ca2 = Signal()
    self.o_ca2 = Signal()

    self.i_cb1 = Signal()
    self.o_cb1 = Signal()
    self.i_cb2 = Signal()
    self.o_cb2 = Signal()

    self.o_irq = Signal()

  def elaborate(self, platform):
    m = Module()

    class Reg(Enum):
      ORB_IRB = 0
      ORA_IRA = 1
      DDRB = 2
      DDRA = 3
      T1C_L = 4
      T1C_H = 5
      T1L_L = 6
      T1L_H = 7
      T2C_L = 8
      T2C_H = 9
      SR = 10
      ACR = 11
      PCR = 12
      IFR = 13
      IER = 14
      ORA_IRA_ = 15

    class Int(Enum):
      CA2 = 0
      CA1 = 1
      SHIFT = 2
      CB2 = 3
      CB1 = 4
      TIMER2 = 5
      TIMER1 = 6

    reg_ora = Signal(8)
    reg_orb = Signal(8)
    #reg_ira = Signal(8)
    #reg_irb = Signal(8)
    reg_ddra = Signal(8)
    reg_ddrb = Signal(8)
    reg_acr = Signal(8)
    reg_pcr = Signal(8)
    reg_ifr = Signal(7)
    reg_ier = Signal(7)
    reg_t1l_l = Signal(8)
    reg_t1l_h = Signal(8)

    acr_pa_latching = Signal()
    acr_pb_latching = Signal()

    timer1_cntr = Signal(16)
    timer1_en = Signal()
    irq = Signal()

    #
    # PA/PB latching
    #
    pa_latch = Signal(8)
    pb_latch = Signal(8)

    ca1_p = Signal()
    cb1_p = Signal()

    with m.If(self.clk_1mhz_ph_en):
      m.d.sync += [ca1_p.eq(self.i_ca1), cb1_p.eq(self.i_cb1)]
      with m.If(ca1_p & ~self.i_ca1):  # XXX: The active edge is same as interrupt edge?
        m.d.sync += pa_latch.eq(self.i_pa)
      with m.If(cb1_p & ~self.i_cb1):  # XXX: ^
        m.d.sync += pb_latch.eq(self.i_pb)

      with m.If(timer1_en):
        m.d.sync += timer1_cntr.eq(timer1_cntr - 1)
        with m.If(timer1_cntr == 0):
          m.d.sync += reg_ifr[6].eq(1)
          with m.Switch(reg_acr[6:8]):
            with m.Case(C(0b00, 2)):  # Timed interrupt each time T1 is loaded
              m.d.sync += timer1_en.eq(0)
            with m.Case(C(0b01, 2)):  # Continious interrupts
              m.d.sync += timer1_cntr.eq(Cat(reg_t1l_l, reg_t1l_h))

    m.d.comb += [acr_pa_latching.eq(reg_acr[0]), acr_pb_latching.eq(reg_acr[1])]
    m.d.comb += [self.o_pa.eq(reg_ora), self.o_pb.eq(reg_orb)]
    m.d.comb += self.o_irq.eq(irq)

    # bus writes
    with m.If(self.clk_1mhz_ph_en & self.i_cs & self.i_we):
      with m.Switch(self.i_addr):
        with m.Case(Reg.ORB_IRB):
          m.d.sync += reg_orb.eq(self.i_data)
        with m.Case(Reg.ORA_IRA):
          m.d.sync += reg_ora.eq(self.i_data)
          m.d.sync += reg_ifr[1].eq(0)
        with m.Case(Reg.DDRB):
          m.d.sync += reg_ddrb.eq(self.i_data)
        with m.Case(Reg.DDRA):
          m.d.sync += reg_ddra.eq(self.i_data)
        with m.Case(Reg.ACR):
          m.d.sync += reg_acr.eq(self.i_data)
        with m.Case(Reg.PCR):
          m.d.sync += reg_pcr.eq(self.i_data)
        with m.Case(Reg.IFR):
          m.d.sync += reg_ifr.eq(reg_ifr & ~self.i_data[0:7])
        with m.Case(Reg.IER):
          with m.If(self.i_data[7]):
            m.d.sync += reg_ier.eq(reg_ier | self.i_data[0:7])
          with m.Else():
            m.d.sync += reg_ier.eq(reg_ier & ~self.i_data[0:7])
        with m.Case(Reg.T1C_L):
          m.d.sync += reg_t1l_l.eq(self.i_data)
        with m.Case(Reg.T1C_H):
          m.d.sync += [timer1_cntr.eq(Cat(reg_t1l_l, self.i_data)), timer1_en.eq(1)]
          m.d.sync += reg_ifr[6].eq(0)
        with m.Case(Reg.T1L_L):
          m.d.sync += reg_t1l_l.eq(self.i_data)
        with m.Case(Reg.T1L_H):
          m.d.sync += reg_t1l_h.eq(self.i_data)
          m.d.sync += reg_ifr[6].eq(0)

    # bus reads
    with m.If(self.clk_1mhz_ph_en & self.i_cs & ~self.i_we):
      with m.Switch(self.i_addr):
        with m.Case(Reg.ORB_IRB):
          for idx in range(8):
            m.d.comb += self.o_data[idx].eq(
                Mux(reg_ddrb[idx], reg_orb[idx],
                    Mux(acr_pb_latching, pb_latch, self.i_pb)[idx]))
        with m.Case(Reg.ORA_IRA):
          for idx in range(8):
            m.d.comb += self.o_data[idx].eq(
                Mux(reg_ddra[idx], reg_ora[idx],
                    Mux(acr_pa_latching, pa_latch, self.i_pa)[idx]))
          m.d.sync += reg_ifr[1].eq(0)
        with m.Case(Reg.DDRB):
          m.d.comb += self.o_data.eq(reg_ddrb)
        with m.Case(Reg.DDRA):
          m.d.comb += self.o_data.eq(reg_ddra)
        with m.Case(Reg.ACR):
          m.d.comb += self.o_data.eq(reg_acr)
        with m.Case(Reg.PCR):
          m.d.comb += self.o_data.eq(reg_pcr)
        with m.Case(Reg.IFR):
          m.d.comb += self.o_data.eq(Cat(reg_ifr, irq))
        with m.Case(Reg.IER):
          m.d.comb += self.o_data.eq(Cat(reg_ier, C(0b1, 1)))
        with m.Case(Reg.T1C_L):
          m.d.comb += self.o_data.eq(timer1_cntr[0:8])
          m.d.sync += reg_ifr[6].eq(0)
        with m.Case(Reg.T1C_H):
          m.d.comb += self.o_data.eq(timer1_cntr[8:16])
        with m.Case(Reg.T1L_L):
          m.d.comb += self.o_data.eq(reg_t1l_l)
          m.d.sync += reg_ifr[6].eq(0)
        with m.Case(Reg.T1L_H):
          m.d.comb += self.o_data.eq(reg_t1l_h)

    # IFR setting logic
    with m.If((ca1_p != self.i_ca1) & (self.i_ca1 == reg_pcr[0])):
      m.d.sync += reg_ifr[1].eq(1)

    m.d.comb += irq.eq((reg_ifr & reg_ier).any())

    return m
