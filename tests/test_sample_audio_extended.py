import unittest

from amaranth.sim import Settle
from test_sample_audio import Cia, Sid, read, run_sim, signed16, tick, write
from sid import VolumeDac


class TimerRegressionTests(unittest.TestCase):

  def test_zero_latch_waits_for_a_tick(self):
    dut = Cia()

    def process():
      yield dut.clk_1mhz_ph_en.eq(1)
      yield from write(dut, 13, 0x82)
      yield from write(dut, 15, 0x59)
      yield from tick(8)
      self.assertEqual((yield dut.o_irq), 0)
      yield from write(dut, 4, 2)
      yield from write(dut, 5, 0)
      yield from write(dut, 14, 0x11)
      yield from tick(2)
      self.assertEqual((yield dut.o_irq), 0)
      yield from tick()
      self.assertEqual((yield dut.o_irq), 1)

    run_sim(dut, process)

  def test_two_a_underflows_then_stop_and_rearm(self):
    dut = Cia()

    def process():
      yield dut.clk_1mhz_ph_en.eq(1)
      yield from write(dut, 4, 2)
      yield from write(dut, 5, 0)
      yield from write(dut, 6, 1)
      yield from write(dut, 7, 0)
      yield from write(dut, 13, 0x82)
      yield from write(dut, 15, 0x59)
      yield from write(dut, 14, 0x11)
      yield from tick(5)
      self.assertEqual((yield dut.o_irq), 0)
      yield from tick()
      self.assertEqual((yield dut.o_irq), 1)
      self.assertEqual((yield from read(dut, 15)) & 0x11, 0)
      status = yield from read(dut, 13)
      self.assertEqual(status & 0x82, 0x82)
      yield from tick(20)
      self.assertEqual((yield dut.o_irq), 0)
      # The sample ISR writes a new interval and force-loads the one-shot.
      yield from write(dut, 7, 0)
      yield from write(dut, 6, 2)
      yield from write(dut, 15, 0x59)
      yield from tick(5)
      self.assertEqual((yield dut.o_irq), 0)
      yield from tick(4)
      self.assertEqual((yield dut.o_irq), 1)

    run_sim(dut, process)

  def test_phi2_mode_is_unchanged(self):
    dut = Cia()

    def process():
      yield dut.clk_1mhz_ph_en.eq(1)
      yield from write(dut, 6, 2)
      yield from write(dut, 7, 0)
      yield from write(dut, 13, 0x82)
      yield from write(dut, 15, 0x19)
      yield from tick(2)
      self.assertEqual((yield dut.o_irq), 0)
      yield from tick()
      self.assertEqual((yield dut.o_irq), 1)

    run_sim(dut, process)

  def test_continuous_timer_b_reloads(self):
    dut = Cia()

    def process():
      yield dut.clk_1mhz_ph_en.eq(1)
      yield from write(dut, 6, 2)
      yield from write(dut, 7, 0)
      yield from write(dut, 13, 0x82)
      yield from write(dut, 15, 0x11)
      yield from tick(3)
      self.assertEqual((yield dut.o_irq), 1)
      yield from read(dut, 13)
      self.assertEqual((yield dut.o_irq), 0)
      yield from tick()
      self.assertEqual((yield dut.o_irq), 0)
      yield from tick()
      self.assertEqual((yield dut.o_irq), 1)

    run_sim(dut, process)

  def test_clock_enable_pauses_counting(self):
    dut = Cia()

    def process():
      yield dut.clk_1mhz_ph_en.eq(1)
      yield from write(dut, 6, 2)
      yield from write(dut, 7, 0)
      yield from write(dut, 13, 0x82)
      yield from write(dut, 15, 0x19)
      yield dut.clk_1mhz_ph_en.eq(0)
      yield from tick(20)
      self.assertEqual((yield dut.o_irq), 0)
      yield dut.clk_1mhz_ph_en.eq(1)
      yield from tick(2)
      self.assertEqual((yield dut.o_irq), 0)
      yield from tick()
      self.assertEqual((yield dut.o_irq), 1)

    run_sim(dut, process)

  def test_timer_a_cnt_mode_counts_edges_not_phi2(self):
    dut = Cia()

    def process():
      yield dut.clk_1mhz_ph_en.eq(1)
      yield from write(dut, 4, 1)
      yield from write(dut, 5, 0)
      yield from write(dut, 13, 0x81)
      yield from write(dut, 14, 0x39)
      yield from tick(8)
      self.assertEqual((yield dut.o_irq), 0)
      for index in range(2):
        yield dut.i_cnt.eq(0)
        yield from tick(2)
        yield dut.i_cnt.eq(1)
        yield from tick()
        self.assertEqual((yield dut.o_irq), index)

    run_sim(dut, process)

  def test_timer_b_cnt_mode_counts_edges(self):
    dut = Cia()

    def process():
      yield dut.clk_1mhz_ph_en.eq(1)
      yield from write(dut, 6, 0)
      yield from write(dut, 7, 0)
      yield from write(dut, 13, 0x82)
      yield from write(dut, 15, 0x39)
      yield from tick(8)
      self.assertEqual((yield dut.o_irq), 0)
      yield dut.i_cnt.eq(0)
      yield from tick()
      yield dut.i_cnt.eq(1)
      yield from tick()
      self.assertEqual((yield dut.o_irq), 1)

    run_sim(dut, process)

  def test_gated_a_mode_requires_cnt_high(self):
    dut = Cia()

    def process():
      yield dut.clk_1mhz_ph_en.eq(1)
      yield dut.i_cnt.eq(0)
      yield from write(dut, 4, 2)
      yield from write(dut, 5, 0)
      yield from write(dut, 6, 0)
      yield from write(dut, 7, 0)
      yield from write(dut, 13, 0x82)
      yield from write(dut, 15, 0x79)
      yield from write(dut, 14, 0x11)
      yield from tick(12)
      self.assertEqual((yield dut.o_irq), 0)
      yield dut.i_cnt.eq(1)
      yield from tick(3)
      self.assertEqual((yield dut.o_irq), 1)

    run_sim(dut, process)

  def test_force_load_does_not_create_an_a_underflow(self):
    dut = Cia()

    def process():
      yield dut.clk_1mhz_ph_en.eq(1)
      yield from write(dut, 13, 0x83)
      yield from write(dut, 15, 0x59)
      yield from write(dut, 14, 0x11)
      yield from write(dut, 14, 0x11)
      self.assertEqual((yield dut.o_irq), 0)
      yield from tick()
      self.assertEqual((yield dut.o_irq), 1)

    run_sim(dut, process)


class SidRegressionTests(unittest.TestCase):

  def test_bus_writes_need_select_and_clock_enable(self):
    dut = Sid()

    def process():
      yield dut.clk_1mhz_ph1_en.eq(1)
      yield dut.i_we.eq(1)
      yield dut.i_addr.eq(0x18)
      yield dut.i_data.eq(0x0f)
      yield from tick(5)
      self.assertEqual((yield dut.o_wave), 0)
      yield dut.i_cs.eq(1)
      yield dut.clk_1mhz_ph1_en.eq(0)
      yield from tick(5)
      self.assertEqual((yield dut.o_wave), 0)

    run_sim(dut, process)

  def test_voice_three_disable_does_not_mute_samples(self):
    dut = Sid()

    def process():
      yield dut.clk_1mhz_ph1_en.eq(1)
      yield from write(dut, 0x18, 0x8f)
      yield from tick(3)
      self.assertGreater(signed16((yield dut.o_wave)), 3000)

    run_sim(dut, process)

  def test_three_voices_plus_sample_do_not_wrap(self):
    dut = Sid()

    def process():
      yield dut.clk_1mhz_ph1_en.eq(1)
      for base in [0, 7, 14]:
        yield from write(dut, base + 2, 0xff)
        yield from write(dut, base + 3, 0x0f)
        yield from write(dut, base + 6, 0xf0)
        yield from write(dut, base + 4, 0x41)
      yield from write(dut, 0x18, 0x0f)
      yield from tick(2500)
      output = signed16((yield dut.o_wave))
      self.assertGreater(output, 10000)
      self.assertLess(output, 16000)

    run_sim(dut, process)


class VolumeDacTests(unittest.TestCase):

  def test_constant_volume_decays_to_silence(self):
    dut = VolumeDac()

    def process():
      yield dut.clk_1mhz_ph1_en.eq(1)
      yield dut.i_volume.eq(15)
      yield Settle()
      self.assertEqual((yield dut.o_wave), 3840)
      yield from tick(50000)
      self.assertLessEqual(abs((yield dut.o_wave)), 1)
      yield dut.i_volume.eq(0)
      yield Settle()
      self.assertLess((yield dut.o_wave), -3800)
      yield from tick(50000)
      self.assertLessEqual(abs((yield dut.o_wave)), 1)

    run_sim(dut, process)

  def test_one_bit_stream_has_both_polarities(self):
    dut = VolumeDac()
    samples = []

    def process():
      yield dut.clk_1mhz_ph1_en.eq(1)
      for _ in range(16):
        for volume, duration in [(15, 585), (0, 900)]:
          yield dut.i_volume.eq(volume)
          yield from tick(duration)
          samples.append((yield dut.o_wave))

    run_sim(dut, process)
    self.assertGreater(max(samples), 1000)
    self.assertLess(min(samples), -1000)
    self.assertTrue(all(-3840 <= sample <= 3840 for sample in samples))

  def test_dc_filter_uses_sid_clock_enable(self):
    dut = VolumeDac()

    def process():
      yield dut.i_volume.eq(15)
      yield from tick(100)
      self.assertEqual((yield dut.o_wave), 3840)
      yield dut.clk_1mhz_ph1_en.eq(1)
      yield from tick(100)
      self.assertLess((yield dut.o_wave), 3840)

    run_sim(dut, process)


if __name__ == '__main__':
  unittest.main()
