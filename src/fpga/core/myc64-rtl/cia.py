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

class Cia(Elaboratable):
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
    self.o_irq = Signal()

    self.ports = [
        self.clk_1mhz_ph_en, self.i_cs, self.i_addr, self.i_we, self.i_data,
        self.o_data, self.i_pa, self.o_pa, self.i_pb, self.o_pb, self.o_irq
    ]

  def elaborate(self, platform):
    m = Module()

    class Reg(Enum):
      PRA = 0
      PRB = 1
      DDRA = 2
      DDRB = 3
      TA_LO = 4
      TA_HI = 5
      TB_LO = 6
      TB_HI = 7
      TOD_10THS = 8
      TOD_SEC = 9
      TOD_MIN = 10
      TOD_HR = 11
      SDR = 12
      ICR = 13
      CRA = 14
      CRB = 15

    reg_pra = Signal(8)
    reg_prb = Signal(8)
    reg_ddra = Signal(8)
    reg_ddrb = Signal(8)

    reg_ta_lo = Signal(8)
    reg_ta_hi = Signal(8)

    reg_tb_lo = Signal(8)
    reg_tb_hi = Signal(8)

    reg_tod_10ths = Signal(8)
    reg_tod_sec = Signal(8)
    reg_tod_min = Signal(8)
    reg_tod_hr = Signal(8)

    reg_sdr = Signal(8)
    reg_icr = Signal(8)
    reg_cra = Signal(8)
    reg_crb = Signal(8)

    timer_a_cntr = Signal(16)
    timer_b_cntr = Signal(16)

    icr_status_ta = Signal()
    icr_status_tb = Signal()
    icr_status_alrm = Signal()
    icr_status_sp = Signal()
    icr_status_flg = Signal()

    icr_status = Cat(icr_status_ta, icr_status_tb, icr_status_alrm, icr_status_sp, icr_status_flg)

    icr_mask_ta = Signal()
    icr_mask_tb = Signal()
    icr_mask_alrm = Signal()
    icr_mask_sp = Signal()
    icr_mask_flg = Signal()

    icr_mask = Cat(icr_mask_ta, icr_mask_tb, icr_mask_alrm, icr_mask_sp, icr_mask_flg)
    irq = Signal()
    m.d.comb += irq.eq((icr_status & icr_mask).any())
    m.d.comb += self.o_irq.eq(irq)

    # Convenience references for CRA bits
    cra_start = reg_cra[0]
    cra_pbon = reg_cra[1]
    cra_outmode = reg_cra[2]
    cra_runmode = reg_cra[3]
    cra_load = reg_cra[4]
    cra_inmode = reg_cra[5]
    cra_spmode = reg_cra[6]
    cra_todin = reg_cra[7]

    # Convenience references for CRB bits
    crb_start = reg_crb[0]
    crb_pbon = reg_crb[1]
    crb_outmode = reg_crb[2]
    crb_runmode = reg_crb[3]
    crb_load = reg_crb[4]
    crb_inmode = reg_crb[5:7]
    crb_alarm = reg_crb[7]

    timer_a_force_load = Signal()
    timer_b_force_load = Signal()

    # bus writes
    with m.If(self.clk_1mhz_ph_en & self.i_cs & self.i_we):
      with m.Switch(self.i_addr):
        with m.Case(0x0):
          m.d.sync += reg_pra.eq(self.i_data)
        with m.Case(0x1):
          m.d.sync += reg_prb.eq(self.i_data)
        with m.Case(0x2):
          m.d.sync += reg_ddra.eq(self.i_data)
        with m.Case(0x3):
          m.d.sync += reg_ddrb.eq(self.i_data)
        with m.Case(Reg.TA_LO):
          m.d.sync += reg_ta_lo.eq(self.i_data)
        with m.Case(Reg.TA_HI):
          m.d.sync += reg_ta_hi.eq(self.i_data)
        with m.Case(Reg.TB_LO):
          m.d.sync += reg_tb_lo.eq(self.i_data)
        with m.Case(Reg.TB_HI):
          m.d.sync += reg_tb_hi.eq(self.i_data)
        # Skip a few for now
        with m.Case(Reg.ICR):
          with m.If(self.i_data[7]): # SET
            m.d.sync += icr_mask.eq(icr_mask | self.i_data[0:5])
          with m.Else(): # CLEAR
            m.d.sync += icr_mask.eq(icr_mask & ~self.i_data[0:5])
        with m.Case(Reg.CRA):
          m.d.comb += timer_a_force_load.eq(self.i_data[4])
          m.d.sync += reg_cra.eq(self.i_data)
        with m.Case(Reg.CRB):
          m.d.comb += timer_b_force_load.eq(self.i_data[4])
          m.d.sync += reg_crb.eq(self.i_data)

    # bus reads
    with m.If(self.clk_1mhz_ph_en & self.i_cs & ~self.i_we):
      with m.Switch(self.i_addr):
        with m.Case(Reg.TA_LO):
          m.d.comb += self.o_data.eq(timer_a_cntr[0:8])
        with m.Case(Reg.TA_HI):
          m.d.comb += self.o_data.eq(timer_a_cntr[8:16])
        with m.Case(Reg.TB_LO):
          m.d.comb += self.o_data.eq(timer_b_cntr[0:8])
        with m.Case(Reg.TB_HI):
          m.d.comb += self.o_data.eq(timer_b_cntr[8:16])
        # Skip a few for now
        with m.Case(Reg.ICR):
          m.d.comb += self.o_data.eq(Cat(icr_status, C(0, 2), irq))
          m.d.sync += icr_status.eq(0) # Reads clear interrupt status
        with m.Case(Reg.CRA):
          m.d.comb += self.o_data.eq(reg_cra)
        with m.Case(Reg.CRB):
          m.d.comb += self.o_data.eq(reg_crb)

    # Timer A
    timer_a_zero = Signal()
    timer_a_reload = Signal()
    m.d.comb += [timer_a_zero.eq(timer_a_cntr == 0), timer_a_reload.eq(timer_a_force_load | (cra_start & ~cra_runmode & timer_a_zero))]
    with m.If(self.clk_1mhz_ph_en):
      with m.If(cra_start):
        with m.If(timer_a_zero):
          m.d.sync += icr_status_ta.eq(1) # XXX: Should be moved down to have highest priority (priority over bus read at least)
          with m.If(cra_runmode): # ONE-SHOT
            m.d.sync += cra_start.eq(0)
        m.d.sync += timer_a_cntr.eq(timer_a_cntr - 1)
      with m.If(timer_a_reload):
        m.d.sync += timer_a_cntr.eq(Cat(reg_ta_lo, reg_ta_hi))




#
# Old cruft to be removed
#



    with m.If(self.clk_1mhz_ph_en & self.i_cs & self.i_we):
      with m.Switch(self.i_addr):
        with m.Case(0x0):
          m.d.sync += self.o_pa.eq(self.i_data)

    m.d.comb += self.o_data.eq(0)
    with m.Switch(self.i_addr):
      with m.Case(0x0):
        m.d.comb += self.o_data.eq(self.i_pa)
      with m.Case(0x1):
        m.d.comb += self.o_data.eq(self.i_pb)


    return m


if __name__ == "__main__":
  cia = Cia()
  main(cia, name="cia", ports=cia.ports)
