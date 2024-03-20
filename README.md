# Analogue Pocket - MyC64 - 0.0.0

This is a go at wrapping up my very much work in progress C64 emulator for the
Analogue Pocket. The emulator core itself
[MyC64](https://github.com/markus-zzz/myc64) was something that I started
working on as a hobby project during summer of 2020. Sporadically over the
years I have put some effort into it. Finally it is approaching some level of
usefulness.

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

As of Analogue firmware version 2.2 normal C64 `.prg` files can be placed in
`/media/Assets/c64/common/` and loaded.

Suggestions are [Supermon+64](https://github.com/jblang/supermon64) and the
[High Voltage SID Collection (HVSC)](https://www.hvsc.c64.org/) converted to
[PSID64 format](https://boswme.home.xs4all.nl/HVSC/HVSC80_PSID64_packed.7z).

For games it is recommended to get hold of the *OneLoad64 collection* of 2100+
titles (just google it). From what I can tell the majority of the collection's
crunched `.prg` files load up properly. It is important to pick the crunched
ones as the uncrunched will attempt to replace the entire RAM which is not
supported while the system is running. Pocket appears to truncate directories
containing more than 1500 files so indexing as below is suggested.

```
$ p7zip -d OneLoad64-Games-Collection-v5.7z
$ cd AlternativeFormats/PRGs/Crunched/
$ for letter in {a..z}; do mkdir ${letter^}; mv ${letter}*.prg ${letter^}/; mv ${letter^}*.prg ${letter^}/; done
$ for digit in {0..9}; do mkdir ${digit}; mv ${digit}*.prg ${digit}/; done
```

## Usage

Pressing **left-of-analogue** brings up (toggles) the on-screen-display
containing a virtual keyboard and a small menu system for settings.

For the virtual keyboard the **face-x** button toggles sticky keys (useful for
modifiers such as shift) and the **face-y** button clears any sticky key.
Normal key press is accomplished with the **face-a** button.

Pressing **trig-l1** and **trig-r1** navigates through the different menu tabs.

Loading `.prg` files is accomplished by first selecting the file in the AP's
**Core Settings->Load PRG Slot #0** file browser and after that injecting it
from the **PRGS** menu tab.

Controllers can be mapped to C64 joystick ports in the **MISC** menu tab and
there you will also find a reset button for the emulator core.

A physical keyboard is supported while docked. The keyboard is expected to show
up on the third input (i.e. `cont3_joy`). For details on the key mapping see
table `hid2c64` in source file `src/bios/main.c`.

## Known issues

While being quite fun to play with the core it not entirely complete yet and
what follows is a list of things I have yet to address

- CIA implementation is lacking many features
- VIC-II is lacking sprite collision
- VIC-II sprite position seem to be offset by eight pixels wrt to background
- VIC-II is lacking soft scrolling (at least in the horizontal direction)
- SID is missing noise, ring modulation, hard sync and a few other things
