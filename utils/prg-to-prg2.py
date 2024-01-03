import struct
import sys

for arg in sys.argv[1:]:
  with open(arg, 'rb') as prg_file:
    prg_bytes = prg_file.read()
    with open(arg + '2', 'wb') as prg2_file:
      prg2_file.write(struct.pack('H', len(prg_bytes) - 2))
      prg2_file.write(prg_bytes)
      prg2_len = 2 + len(prg_bytes)
      while prg2_len % 4 != 0:
        prg2_file.write(struct.pack('B', 0))
        prg2_len += 1
