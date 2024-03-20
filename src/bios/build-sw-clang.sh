#!/bin/bash

# exit when any command fails
set -e

clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror main.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror keyboard-virt.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror keyboard-ext.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror osd.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror prgs.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror misc.c -c
llvm-mc --arch=riscv32 -mcpu=generic-rv32 -mattr=+c -assemble start.S --filetype=obj -o start.o
ld.lld -T system.ld start.o main.o keyboard-virt.o keyboard-ext.o prgs.o misc.o osd.o -o bios.elf -M
llvm-objcopy --only-section=.text --output-target=binary bios.elf bios.bin
hexdump -v -e '4/4 "%08x " "\n"' bios.bin > bios.vh
