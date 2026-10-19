import unittest

from amaranth import Elaboratable, Module
from test_sample_audio import Cia, Sid, read, run_sim, signed16, tick, write


class SamplePath(Elaboratable):

  def __init__(self):
    self.cia = Cia()
    self.sid = Sid()

  def elaborate(self, platform):
    m = Module()
    m.submodules.cia = self.cia
    m.submodules.sid = self.sid
    m.d.comb += [
        self.cia.clk_1mhz_ph_en.eq(1),
        self.sid.clk_1mhz_ph1_en.eq(1)
    ]
    return m


class SamplePathTests(unittest.TestCase):

  def test_game_timer_setup_drives_silent_voice_volume_samples(self):
    dut = SamplePath()

    def process():
      cia, sid = dut.cia, dut.sid
      for addr in range(0x19):
        yield from write(sid, addr, 0)
      yield from write(cia, 4, 8)
      yield from write(cia, 5, 0)
      yield from write(cia, 6, 0x40)
      yield from write(cia, 7, 0)
      yield from write(cia, 13, 0x82)
      yield from write(cia, 15, 0x59)
      yield from write(cia, 14, 0x51)
      yield from tick(584)
      self.assertEqual((yield cia.o_irq), 0)
      yield from tick()
      self.assertEqual((yield cia.o_irq), 1)
      yield from read(cia, 13)
      yield from write(sid, 0x18, 15)
      yield from tick(3)
      self.assertGreater(signed16((yield sid.o_wave)), 3000)
      yield from write(cia, 6, 0x50)
      yield from write(cia, 7, 0)
      yield from write(cia, 15, 0x59)
      for elapsed in range(740):
        if (yield cia.o_irq):
          break
        yield from tick()
      else:
        self.fail('Rearmed sample timer never interrupted')
      self.assertGreaterEqual(elapsed, 720)
      self.assertLessEqual(elapsed, 729)
      yield from write(sid, 0x18, 0)
      yield from tick(3)
      self.assertLess(signed16((yield sid.o_wave)), 0)

    run_sim(dut, process)


if __name__ == '__main__':
  unittest.main()
