# SPDX-License-Identifier: GPL-3.0-or-later
"""Small linear/8580-like SID filter; not a nonlinear/measured 6581 model.

Independently implemented Chamberlin (two-integrator state-variable) topology:
  hp = input - lp - damping * bp
  bp = bp + f * hp
  lp = lp + f * bp                 (uses the NEW bp)
f = 2*sin(pi*fc/fs), fs = 1 MHz.  An affine interpolation of its endpoints
maps all 2048 cutoff codes onto 30..12500 Hz. The affine approximation error
is <0.027%; including fixed-point rounding the relative error is <0.3%.
Damping = 2**((4-resonance)/8), approximately 1.414..0.386 (Q=0.707..2.594).
The transition determinant is 1-f*damping and trace 2-f*damping-f*f;
f*f+2*f*damping < 4 and 0 < f*damping < 2 throughout this range, so the
unclipped constant-control recurrence is strictly stable.

States are signed Q19.14 (34 bits), with over 21x full-three-voice headroom.
Products round to nearest (ties toward +infinity), rather than biasing every
integration downward. Explicit saturation protects against arbitrary rapid
control changes; tails below one output unit are cleared on silent input.
Mode outputs add, not average; the SID mixer saturates the final voice mix.
This intentionally does not model 6581 distortion, cutoff discontinuities,
external-input audio, or analog output offsets (VolumeDac handles digis).
"""

import math

from amaranth import *


FRACTION_BITS = 14
STATE_BITS = 34
COEFFICIENT_BITS = 20
F_MIN = round(2 * math.sin(math.pi * 30 / 1e6) * (1 << COEFFICIENT_BITS))
F_MAX = round(2 * math.sin(math.pi * 12500 / 1e6) * (1 << COEFFICIENT_BITS))
F_SLOPE = round((F_MAX - F_MIN) * (1 << 16) / 2047)
DAMPING = tuple(round(2 ** ((4 - resonance) / 8) * (1 << 14)) for resonance in range(16))


def saturate(m, target, value):
  """Assign a signed target without allowing truncation to wrap its sign."""
  maximum = (1 << (len(target) - 1)) - 1
  minimum = -(1 << (len(target) - 1))
  m.d.comb += target.eq(Mux(value > maximum, maximum, Mux(value < minimum, minimum, value)))


class SidFilter(Elaboratable):

  def __init__(self):
    self.clk_1mhz_ph1_en = Signal(name='filter_enable')
    self.i_wave = Signal(signed(16), name='filter_input')
    self.i_cutoff = Signal(11, name='filter_cutoff')
    self.i_resonance = Signal(4, name='filter_resonance')
    self.i_modes = Signal(3, name='filter_modes')  # LP, BP, HP
    self.o_wave = Signal(signed(22), name='filter_output')
    self.low = Signal(signed(STATE_BITS), name='filter_low')
    self.band = Signal(signed(STATE_BITS), name='filter_band')
    self.high = Signal(signed(STATE_BITS), name='filter_high')

  def elaborate(self, platform):
    m = Module()
    frequency = Signal(17, name='filter_frequency')
    damping = Signal(15, name='filter_damping')
    m.d.comb += [
        frequency.eq(F_MIN + ((self.i_cutoff * F_SLOPE + (1 << 15)) >> 16)),
        damping.eq(Array(Const(value, 15) for value in DAMPING)[self.i_resonance])
    ]

    high_next = Signal(signed(STATE_BITS), name='filter_high_next')
    band_next = Signal(signed(STATE_BITS), name='filter_band_next')
    low_next = Signal(signed(STATE_BITS), name='filter_low_next')
    # Unsigned coefficients get an explicit zero sign bit before multiplication.
    damped = (self.band * Cat(damping, Const(0, 1)).as_signed() + (1 << 13)) >> 14
    saturate(m, high_next, (self.i_wave << FRACTION_BITS) - self.low - damped)
    band_step = (high_next * Cat(frequency, Const(0, 1)).as_signed() + (1 << 19)) >> COEFFICIENT_BITS
    saturate(m, band_next, self.band + band_step)
    low_step = (band_next * Cat(frequency, Const(0, 1)).as_signed() + (1 << 19)) >> COEFFICIENT_BITS
    saturate(m, low_next, self.low + low_step)

    with m.If(self.clk_1mhz_ph1_en):
      # Remove sub-audio quantization limit cycles; at most one output LSB.
      with m.If((self.i_wave == 0) &
                (self.low > -(1 << FRACTION_BITS)) & (self.low < (1 << FRACTION_BITS)) &
                (self.band > -(1 << FRACTION_BITS)) & (self.band < (1 << FRACTION_BITS))):
        m.d.sync += [self.low.eq(0), self.band.eq(0), self.high.eq(0)]
      with m.Else():
        m.d.sync += [self.low.eq(low_next), self.band.eq(band_next), self.high.eq(high_next)]

    m.d.comb += self.o_wave.eq(
        Mux(self.i_modes[0], self.low >> FRACTION_BITS, 0) +
        Mux(self.i_modes[1], self.band >> FRACTION_BITS, 0) +
        Mux(self.i_modes[2], self.high >> FRACTION_BITS, 0))
    return m
