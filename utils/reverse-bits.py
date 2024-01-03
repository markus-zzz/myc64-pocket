import struct
import sys

def rev_byte_bits(in_byte):
  out_byte = 0
  for i in range(8):
    if in_byte & (1 << i):
      out_byte |=  (1 << (7 - i))
  return out_byte

in_path = sys.argv[1]
out_path = sys.argv[2]
with open(in_path, 'rb') as in_file:
  with open(out_path, 'wb') as out_file:
    for in_byte in in_file.read():
      out_file.write(struct.pack('B', rev_byte_bits(in_byte)))
