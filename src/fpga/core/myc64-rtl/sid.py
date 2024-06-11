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


class WaveformGenerator(Elaboratable):

  def __init__(self):
    self.clk_1mhz_ph1_en = Signal()
    self.i_frequency = Signal(16)
    self.i_duty_cycle = Signal(12)
    self.i_triangle_en = Signal()
    self.i_sawtooth_en = Signal()
    self.i_pulse_en = Signal()
    self.i_noise_en = Signal()
    self.o_wave = Signal(12)

  def elaborate(self, platform):
    m = Module()

    phase_accum = Signal(24)
    wave_sawtooth = Signal(12)
    wave_triangle = Signal(12)
    wave_pulse = Signal(12)
    wave_noise = Signal(12)
    lfsr = Signal(23, reset=0b1)
    feedback = Signal()
    lfsr_clk_d = Signal()

    m.d.comb += feedback.eq(lfsr[15] ^ lfsr[17] ^ lfsr[18] ^ lfsr[22])

    lfsr_clk = phase_accum[19]

    with m.If(self.clk_1mhz_ph1_en):
      m.d.sync += phase_accum.eq(phase_accum + self.i_frequency)
      m.d.sync += lfsr_clk_d.eq(lfsr_clk)
      with m.If(~lfsr_clk_d & lfsr_clk):
        m.d.sync += lfsr.eq(Cat(feedback, lfsr[0:22]))

    m.d.comb += wave_sawtooth.eq(phase_accum[12:24])
    m.d.comb += wave_triangle.eq(Cat(C(0, 1), phase_accum[23].replicate(11) ^ phase_accum[12:23]))
    m.d.comb += wave_pulse.eq(Mux(phase_accum[12:24] <= self.i_duty_cycle, C(0xfff, 12), C(0, 12)))
    m.d.comb += wave_noise.eq(lfsr[11:23])

    m.d.comb += self.o_wave.eq(~(~Mux(self.i_sawtooth_en, wave_sawtooth, 0) & ~Mux(self.i_triangle_en, wave_triangle, 0)
                                 & ~Mux(self.i_pulse_en, wave_pulse, 0) & ~Mux(self.i_noise_en, wave_noise, 0)))

    return m


class EnvelopeGenerator(Elaboratable):

  def __init__(self):
    self.clk_1mhz_ph1_en = Signal()
    self.i_gate = Signal()
    self.i_attack = Signal(4)
    self.i_decay = Signal(4)
    self.i_sustain = Signal(4)
    self.i_release = Signal(4)
    self.o_envelope = Signal(8)

  def generate_map(self, m, freq, values, in_sig, out_sig):
    period = 1 / freq
    with m.Switch(in_sig):
      for idx, value in enumerate(values):
        with m.Case(idx):
          m.d.comb += out_sig.eq(int((value / period) / 256))

  def elaborate(self, platform):
    m = Module()

    attack_durations = [
        0.002, 0.008, 0.016, 0.024, 0.038, 0.056, 0.068, 0.080, 0.100, 0.250, 0.500, 0.800, 1.000, 3.000, 5.000, 8.000
    ]
    decay_release_durations = [
        0.006, 0.024, 0.048, 0.072, 0.114, 0.168, 0.204, 0.240, 0.300, 0.750, 1.500, 2.400, 3.000, 9.000, 15.000, 24.000
    ]

    map_attack = Signal(16)
    map_decay = Signal(16)
    map_release = Signal(16)

    self.generate_map(m, 1e6, attack_durations, self.i_attack, map_attack)
    self.generate_map(m, 1e6, decay_release_durations, self.i_decay, map_decay)
    self.generate_map(m, 1e6, decay_release_durations, self.i_release, map_release)

    cntr = Signal(8)
    freq_div = Signal(16)

    m.d.comb += self.o_envelope.eq(cntr)

    with m.If(self.clk_1mhz_ph1_en):
      m.d.sync += freq_div.eq(freq_div - 1)

      with m.FSM(reset='idle') as fsm:
        #
        # Idle
        #
        with m.State('idle'):
          with m.If(self.i_gate):
            m.d.sync += freq_div.eq(map_attack)
            m.next = 'attack'
        #
        # Attack
        #
        with m.State('attack'):
          with m.If(~self.i_gate):
            m.d.sync += freq_div.eq(map_release)
            m.next = 'release'
          with m.Elif(cntr == 0xff):
            m.d.sync += freq_div.eq(map_decay)
            m.next = 'decay'
          with m.Elif(freq_div == 0):
            m.d.sync += cntr.eq(cntr + 1)
            m.d.sync += freq_div.eq(map_attack)
        #
        # Decay
        #
        with m.State('decay'):
          with m.If(~self.i_gate):
            m.d.sync += freq_div.eq(map_release)
            m.next = 'release'
          with m.Elif(cntr == Cat(C(0, 4), self.i_sustain)):
            m.next = 'sustain'
          with m.Elif(freq_div == 0):
            m.d.sync += cntr.eq(cntr - 1)
            m.d.sync += freq_div.eq(map_decay)
        #
        # Sustain
        #
        with m.State('sustain'):
          # Track self.i_sustain (counting down only)
          with m.If(cntr > Cat(C(0, 4), self.i_sustain)):
            m.d.sync += cntr.eq(cntr - 1)
          with m.If(~self.i_gate):
            m.d.sync += freq_div.eq(map_release)
            m.next = 'release'
        #
        # Release
        #
        with m.State('release'):
          with m.If(self.i_gate):
            m.d.sync += freq_div.eq(map_attack)
            m.next = 'attack'
          with m.Elif(cntr == 0):
            m.next = 'idle'
          with m.Elif(freq_div == 0):
            m.d.sync += cntr.eq(cntr - 1)
            m.d.sync += freq_div.eq(map_release)

    return m


class Sid(Elaboratable):

  def __init__(self):

    self.clk_1mhz_ph1_en = Signal()
    self.i_cs = Signal()
    self.i_addr = Signal(5)
    self.i_we = Signal()
    self.i_data = Signal(8)
    self.o_data = Signal(8)
    self.o_wave = Signal(16)

  def elaborate(self, platform):
    m = Module()

    rf = RegisterFile(base=0xd400)

    r_d400 = rf.addRegRW(addr=0xd400, width=8)  # Voice 1 freq lo-byte.
    r_d401 = rf.addRegRW(addr=0xd401, width=8)  # Voice 1 freq hi-byte.
    r_d402 = rf.addRegRW(addr=0xd402, width=8)  # Voice 1 pulse-width lo-byte.
    r_d403 = rf.addRegRW(addr=0xd403, width=4)  # Voice 1 pulse-width hi-byte.
    r_d404 = rf.addRegRW(addr=0xd404, width=8)  # Voice 1 ctrl register.
    r_d405 = rf.addRegRW(addr=0xd405, width=8)  # Voice 1 Attack and Decay length.
    r_d406 = rf.addRegRW(addr=0xd406, width=8)  # Voice 1 Sustain volume and Release length.

    r_d407 = rf.addRegRW(addr=0xd407, width=8)  # Voice 2 freq lo-byte.
    r_d408 = rf.addRegRW(addr=0xd408, width=8)  # Voice 2 freq hi-byte.
    r_d409 = rf.addRegRW(addr=0xd409, width=8)  # Voice 2 pulse-width lo-byte.
    r_d40a = rf.addRegRW(addr=0xd40a, width=4)  # Voice 2 pulse-width hi-byte.
    r_d40b = rf.addRegRW(addr=0xd40b, width=8)  # Voice 2 ctrl register.
    r_d40c = rf.addRegRW(addr=0xd40c, width=8)  # Voice 2 Attack and Decay length.
    r_d40d = rf.addRegRW(addr=0xd40d, width=8)  # Voice 2 Sustain volume and Release length.

    r_d40e = rf.addRegRW(addr=0xd40e, width=8)  # Voice 3 freq lo-byte.
    r_d40f = rf.addRegRW(addr=0xd40f, width=8)  # Voice 3 freq hi-byte.
    r_d410 = rf.addRegRW(addr=0xd410, width=8)  # Voice 3 pulse-width lo-byte.
    r_d411 = rf.addRegRW(addr=0xd411, width=4)  # Voice 3 pulse-width hi-byte.
    r_d412 = rf.addRegRW(addr=0xd412, width=8)  # Voice 3 ctrl register.
    r_d413 = rf.addRegRW(addr=0xd413, width=8)  # Voice 3 Attack and Decay length.
    r_d414 = rf.addRegRW(addr=0xd414, width=8)  # Voice 3 Sustain volume and Release length.

    tmp_bus_wen = (self.clk_1mhz_ph1_en & self.i_cs & self.i_we)
    dummy = Signal(8)
    rf.genInterface(module=m, bus_addr=self.i_addr, bus_wen=tmp_bus_wen, bus_rdata=dummy, bus_wdata=self.i_data)

    # Voice 1
    voice1 = Signal(21)
    m.submodules.u_wave1 = u_wave1 = WaveformGenerator()
    m.d.comb += [
        u_wave1.clk_1mhz_ph1_en.eq(self.clk_1mhz_ph1_en),
        u_wave1.i_frequency.eq(Cat(r_d400, r_d401)),
        u_wave1.i_duty_cycle.eq(Cat(r_d402, r_d403)),
        u_wave1.i_triangle_en.eq(r_d404[4]),
        u_wave1.i_sawtooth_en.eq(r_d404[5]),
        u_wave1.i_pulse_en.eq(r_d404[6]),
        u_wave1.i_noise_en.eq(r_d404[7])
    ]
    m.submodules.u_env1 = u_env1 = EnvelopeGenerator()
    m.d.comb += [
        u_env1.clk_1mhz_ph1_en.eq(self.clk_1mhz_ph1_en),
        u_env1.i_gate.eq(r_d404[0]),
        u_env1.i_attack.eq(r_d405[4:8]),
        u_env1.i_decay.eq(r_d405[0:4]),
        u_env1.i_sustain.eq(r_d406[4:8]),
        u_env1.i_release.eq(r_d406[0:4])
    ]
    m.d.comb += voice1.eq((u_wave1.o_wave - C(0x800, 12)).as_signed() * u_env1.o_envelope.as_unsigned())

    # Voice 2
    voice2 = Signal(21)
    m.submodules.u_wave2 = u_wave2 = WaveformGenerator()
    m.d.comb += [
        u_wave2.clk_1mhz_ph1_en.eq(self.clk_1mhz_ph1_en),
        u_wave2.i_frequency.eq(Cat(r_d407, r_d408)),
        u_wave2.i_duty_cycle.eq(Cat(r_d409, r_d40a)),
        u_wave2.i_triangle_en.eq(r_d40b[4]),
        u_wave2.i_sawtooth_en.eq(r_d40b[5]),
        u_wave2.i_pulse_en.eq(r_d40b[6]),
        u_wave2.i_noise_en.eq(r_d40b[7])
    ]
    m.submodules.u_env2 = u_env2 = EnvelopeGenerator()
    m.d.comb += [
        u_env2.clk_1mhz_ph1_en.eq(self.clk_1mhz_ph1_en),
        u_env2.i_gate.eq(r_d40b[0]),
        u_env2.i_attack.eq(r_d40c[4:8]),
        u_env2.i_decay.eq(r_d40c[0:4]),
        u_env2.i_sustain.eq(r_d40d[4:8]),
        u_env2.i_release.eq(r_d40d[0:4])
    ]
    m.d.comb += voice2.eq((u_wave2.o_wave - C(0x800, 12)).as_signed() * u_env2.o_envelope.as_unsigned())

    # Voice 3
    voice3 = Signal(21)
    m.submodules.u_wave3 = u_wave3 = WaveformGenerator()
    m.d.comb += [
        u_wave3.clk_1mhz_ph1_en.eq(self.clk_1mhz_ph1_en),
        u_wave3.i_frequency.eq(Cat(r_d40e, r_d40f)),
        u_wave3.i_duty_cycle.eq(Cat(r_d410, r_d411)),
        u_wave3.i_triangle_en.eq(r_d412[4]),
        u_wave3.i_sawtooth_en.eq(r_d412[5]),
        u_wave3.i_pulse_en.eq(r_d412[6]),
        u_wave3.i_noise_en.eq(r_d412[7])
    ]
    m.submodules.u_env3 = u_env3 = EnvelopeGenerator()
    m.d.comb += [
        u_env3.clk_1mhz_ph1_en.eq(self.clk_1mhz_ph1_en),
        u_env3.i_gate.eq(r_d412[0]),
        u_env3.i_attack.eq(r_d413[4:8]),
        u_env3.i_decay.eq(r_d413[0:4]),
        u_env3.i_sustain.eq(r_d414[4:8]),
        u_env3.i_release.eq(r_d414[0:4])
    ]
    m.d.comb += voice3.eq((u_wave3.o_wave - C(0x800, 12)).as_signed() * u_env3.o_envelope.as_unsigned())

    m.d.sync += self.o_wave.eq(voice1[7:21].as_signed() + voice2[7:21].as_signed() + voice3[7:21].as_signed())

    with m.Switch(self.i_addr):
      with m.Case(0x1b):
        m.d.comb += self.o_data.eq(u_wave3.o_wave[4:12])
      with m.Case(0x1c):
        m.d.comb += self.o_data.eq(u_env3.o_envelope)

    return m
