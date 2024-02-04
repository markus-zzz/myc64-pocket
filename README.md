# Analogue Pocket - MyC64 - 0.0.0

This is a go at wrapping up my very much work in progress C64 emulator for the
Analogue Pocket. The emulator core itself
[MyC64](https://github.com/markus-zzz/myc64) was something that I started
working on as a hobby project during summer of 2020. Sporadically over the years I have
put some effort into it but overall **it is very far from complete**.

In its current state the emulator **will not support running any games** but it
does boot up into BASIC and it is possible to load `.prg` files such as
Supermon64 and some PSID64 SID players.

## Setup

Assuming that the Analogue Pocket's SD card is by some means mounted as `/media`

```
$ cp dist/platforms/c64.json /media/platforms/
$ cp -r dist/Cores/markus-zzz.MyC64 /media/Cores/
```

### System ROMs

For legal reasons all ROMs are kept outside the FPGA bitstream distributed
here. The three Commodore ROMs can be fetched with
```
wget http://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/basic.901226-01.bin
wget http://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/characters.901225-01.bin
wget http://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/kernal.901227-03.bin
```
they then need to be placed in `/media/Assets/c64/common/` as follows
```
/media/Assets/c64/common/basic.bin
/media/Assets/c64/common/characters.bin
/media/Assets/c64/common/kernal.bin
```

### User PRGs

Due to bug in the Analogue firmware it is not possible to load assets that are
not a multiple of four bytes. To deal with this the `.prg` files are converted
into the made up format `.prg2` containing an additional header indicating the
true size and then padding the payload to a multiple of four.

```
$ python3 ./utils/prg-to-prg2.py *.prg
```

Suggestions are [Supermon+64](https://github.com/jblang/supermon64) and the
[High Voltage SID Collection (HVSC)](https://www.hvsc.c64.org/) converted to
[PSID64 format](https://boswme.home.xs4all.nl/HVSC/HVSC80_PSID64_packed.7z).

## Usage

From **Core Settings->Load PRG Slot #0** one can select the `.prg2` that is to
be loaded into C64 memory when the **right-of-analogue** button is pressed.

- **left-of-analogue** - Toggle virtual keyboard on/off
- **right-of-analogue** - Load PRG slot #0 into C64 RAM
- **left-trig** - Shift modifier for keyboard

A physical keyboard is supported while docked. The keyboard is expected to show
up on the third input (i.e. `cont3_joy`). For details on the key mapping see
table `hid2c64` in source file `src/bios/main.c`.

## Build instructions

Build the BIOS for the housekeeping CPU
```
$ cd src/bios/
$ ./build-sw-clang.sh
$ cd -
```

*run the Quartus flow*

Do bit-reversal of the configuration bitstream
```
$ python3 utils/reverse-bits.py src/fpga/output_files/ap_core.rbf dist/Cores/markus-zzz.MyC64/bitstream.rbf_r
```

All non APF verilog is auto formatted with `verible-verilog-format`. Any C-code
is formatted with `clang-format`.
