import sys
import unittest
from pathlib import Path

from amaranth.sim import Simulator, Settle, Tick

RTL = Path(__file__).resolve().parents[1] / 'src/fpga/core/myc64-rtl'
sys.path.insert(0, str(RTL))

from cia import Cia
from sid import Sid


def run_sim(dut, process):
  sim = Simulator(dut)
  sim.add_clock(1e-6)
  sim.add_process(process)
  sim.run()


def tick(count=1):
  for _ in range(count):
    yield Tick()
    yield Settle()


def write(dut, addr, data):
  yield dut.i_cs.eq(1)
  yield dut.i_we.eq(1)
  yield dut.i_addr.eq(addr)
  yield dut.i_data.eq(data)
  yield from tick()
  yield dut.i_cs.eq(0)
  yield dut.i_we.eq(0)
  yield Settle()


def read(dut, addr):
  yield dut.i_cs.eq(1)
  yield dut.i_we.eq(0)
  yield dut.i_addr.eq(addr)
  yield Settle()
  value = yield dut.o_data
  yield from tick()
  yield dut.i_cs.eq(0)
  yield Settle()
  return value


def signed16(value):
  return value - 0x10000 if value & 0x8000 else value


class SampleAudioRegressionTests(unittest.TestCase):

  def test_volume_samples_are_audible_with_all_voices_silent(self):
    dut = Sid()

    def process():
      yield dut.clk_1mhz_ph1_en.eq(1)
      # The game's sample setup clears all SID registers, then toggles D418.
      for addr in range(0x19):
        yield from write(dut, addr, 0)
      yield from write(dut, 0x18, 0x0f)
      yield from tick(3)
      self.assertGreater(signed16((yield dut.o_wave)), 0)

    run_sim(dut, process)

  def test_chained_timer_b_waits_for_timer_a(self):
    dut = Cia()

    def process():
      yield dut.clk_1mhz_ph_en.eq(1)
      yield from write(dut, 6, 1)
      yield from write(dut, 7, 0)
      yield from write(dut, 13, 0x82)
      # As in the game: one-shot B counts A underflows, not PHI2.
      yield from write(dut, 15, 0x59)
      yield from tick(16)
      self.assertEqual((yield dut.o_irq), 0)
      self.assertEqual((yield from read(dut, 6)), 1)

    run_sim(dut, process)


if __name__ == '__main__':
  unittest.main()
