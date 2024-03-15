import struct
import sys
import subprocess
import shutil
import tempfile
import os

core_top_sim = '/home/markus/work/repos/myc64-pocket/src/fpga/core_top-sim'

for prg in sys.argv[1:]:
  print(prg)
  cmd = [core_top_sim, '--dump-video', '--exit-frame', '250', '--prg', os.path.abspath(prg)]
  with tempfile.TemporaryDirectory() as tmpdirname:
    shutil.copy('bios.vh', tmpdirname)
    subprocess.run(cmd, cwd=tmpdirname)
    shutil.move('{}/vicii-0250.png'.format(tmpdirname), '{}.png'.format(os.path.basename(prg)))
