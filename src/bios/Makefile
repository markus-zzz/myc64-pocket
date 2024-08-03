OBJS = bridge.o crts.o g64.o keyboard-ext.o keyboard-virt.o main.o misc.o osd.o prgs.o start.o

all: bios.vh

%.o: %.c
	clang --target=riscv32 -march=rv32imc -std=c99 -O3 -Os -fno-inline -mno-relax -Wall -Werror -c $< -o $@

%.o: %.S
	clang --target=riscv32 -march=rv32imc -mno-relax -Wall -Werror -c $< -o $@

bios.elf: $(OBJS) system.ld
	ld.lld -T system.ld $(OBJS) -o $@ -M

bios.bin: bios.elf
	llvm-objcopy --keep-section=.text --keep-section=.data --output-target=binary $< $@

bios.vh: bios.bin
	hexdump -v -e '4/4 "%08x " "\n"' $< > $@

clean:
	rm -f $(OBJS) bios.elf bios.bin bios.vh
