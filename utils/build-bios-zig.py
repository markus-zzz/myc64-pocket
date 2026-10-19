#!/usr/bin/env python3
"""Build the unchanged housekeeping BIOS with Zig's bundled Clang and LLD."""

import os
import struct
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BIOS = ROOT / 'src/bios'
NAMES = ['bridge', 'crts', 'g64', 'keyboard-ext', 'keyboard-virt', 'main', 'misc', 'osd', 'prgs', 'start']


def main():
  env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(ROOT / '.zig-cache'))
  cc = [sys.executable, '-m', 'ziglang', 'cc', '-target', 'riscv32-freestanding',
        '-mcpu=generic_rv32+m+c', '-mabi=ilp32', '-mno-relax']
  objects = []
  for name in NAMES:
    source = name + ('.S' if name == 'start' else '.c')
    flags = ['-Wall', '-Werror']
    if source.endswith('.c'):
      flags += ['-std=c99', '-Os', '-fno-inline', '-ffreestanding', '-fno-stack-protector']
    subprocess.run(cc + flags + ['-c', source, '-o', name + '.o'], cwd=BIOS, env=env, check=True)
    objects.append(name + '.o')
  subprocess.run(cc + ['-nostdlib', '-Wl,-T,system.ld', '-Wl,--build-id=none'] + objects + ['-o', 'bios.elf'],
                 cwd=BIOS, env=env, check=True)

  # Extract ROM load segments, including .data's ROM initialization image.
  # Zig objcopy can require host SDK libraries; ELF32 extraction avoids that.
  elf = (BIOS / 'bios.elf').read_bytes()
  if elf[:7] != b'\x7fELF\x01\x01\x01':
    raise RuntimeError('Expected little-endian ELF32')
  header = struct.unpack_from('<HHIIIIIHHHHHH', elf, 16)
  if header[1] != 243 or header[3] != 0:
    raise RuntimeError('Expected a RISC-V BIOS with entry point zero')
  image = bytearray(8192)
  size = 0
  for index in range(header[9]):
    segment = struct.unpack_from('<IIIIIIII', elf, header[4] + index * header[8])
    kind, offset, _, physical, length, _, _, _ = segment
    if kind != 1 or length == 0:
      continue
    if physical + length > len(image):
      raise RuntimeError('BIOS exceeds the 8 KiB ROM')
    image[physical:physical + length] = elf[offset:offset + length]
    size = max(size, physical + length)
  if size == 0:
    raise RuntimeError('No ROM load segments found')
  size = (size + 15) & ~15
  image = image[:size]
  (BIOS / 'bios.bin').write_bytes(image)
  lines = []
  for offset in range(0, size, 16):
    lines.append(' '.join(f'{word:08x}' for word in struct.unpack_from('<IIII', image, offset)))
  (BIOS / 'bios.vh').write_text('\n'.join(lines) + '\n')
  print(f'Housekeeping BIOS: {size} / 8192 bytes')


if __name__ == '__main__':
  main()
