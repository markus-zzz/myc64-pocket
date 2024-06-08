#!/bin/bash

# exit when any command fails
set -e

clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror main.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror keyboard-virt.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror keyboard-ext.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror osd.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror prgs.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror g64.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror crts.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror misc.c -c
clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror start.S -c
ld.lld -T system.ld start.o main.o keyboard-virt.o keyboard-ext.o prgs.o crts.o g64.o misc.o osd.o -o bios.elf -M
llvm-objcopy --only-section=.text --output-target=binary bios.elf bios.bin
hexdump -v -e '4/4 "%08x " "\n"' bios.bin > bios.vh
