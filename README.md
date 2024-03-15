# Analogue Pocket - MyC64 - 0.0.0

This is a go at wrapping up my very much work in progress C64 emulator for the
Analogue Pocket. The emulator core itself
[MyC64](https://github.com/markus-zzz/myc64) was something that I started
working on as a hobby project during summer of 2020. Sporadically over the years I have
put some effort into it but overall **it is quite far from complete**.

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

The following games seem to work somewhat okay but others might too (have not
really tested that many).
```
$ md5sum *.prg
e8b3602ed47f3fa0fa057446a073291d  Bubble Bobble (1987)(Firebird Software).prg
ffc9147f4e436e75d6bc316555ac1fbf  Dan Dare 2 - Mekon's Revenge (1988)(Virgin Games)[cr Stack].prg
```

## Usage

From **Core Settings->Load PRG Slot #0** one can select the `.prg` that is to
be loaded into C64 memory when the **right-of-analogue** button is pressed.

- **left-of-analogue** - Toggle virtual keyboard on/off
- **right-of-analogue** - Load PRG slot #0 into C64 RAM
- **left-trig** - Shift modifier for keyboard

A physical keyboard is supported while docked. The keyboard is expected to show
up on the third input (i.e. `cont3_joy`). For details on the key mapping see
table `hid2c64` in source file `src/bios/main.c`.

## Development

### Build instructions

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

### Testing / verification

The testing process during development has so far not been very systematic. As
this is a hobby project I am aiming for what is the most fun, and that is
unfortunately not to have an extensive test suite that would effectively smoke
out any problem in a controlled environment, but instead to try and run real
games and see where it breaks.

The following test programs have been used during development to compare
against VICE
```
$ md5sum *.prg
e8b3602ed47f3fa0fa057446a073291d  Bubble Bobble (1987)(Firebird Software).prg
4af2b50eb5ff1c5c42ed4d2ed514390b  Commando (1985)(Elite).prg
0260b16e26a4b8f1a94917f098ffa673  Ghosts 'n Goblins (1986)(Elite)[b].prg
3a64ddb63188960b67ee3bfe583ea2d6  supermon64.prg
0c23bbdff747ee112196874cc910ca55  Top_Duck.prg
```

Clone the official git repository of the VICE C64 emulator

```
git clone https://github.com/VICE-Team/svn-mirror.git
$ ./autogen.sh
$ ./configure --without-libcurl --without-alsa --enable-gtk3ui --prefix=/home/markus/work/scratch/vice-install
$ make install
```

Then make some adjustments to its source code to enable dumping for system RAM
into an uncluttered format

```
diff --git a/vice/src/c64/c64memsnapshot.c b/vice/src/c64/c64memsnapshot.c
index 99c282746b..0b778b008b 100644
--- a/vice/src/c64/c64memsnapshot.c
+++ b/vice/src/c64/c64memsnapshot.c
@@ -203,6 +203,11 @@ static const char snap_mem_module_name[] = "C64MEM";

 int c64_snapshot_write_module(snapshot_t *s, int save_roms)
 {
+  {
+     FILE *fp = fopen("ram.bin", "wb");
+     fwrite(mem_ram, C64_RAM_SIZE, 1, fp);
+     fclose(fp);
+  }
     snapshot_module_t *m;

     /* Main memory module.  */
diff --git a/vice/src/monitor/mon_memmap.c b/vice/src/monitor/mon_memmap.c
index 5b95264f19..07b9ca0954 100644
--- a/vice/src/monitor/mon_memmap.c
+++ b/vice/src/monitor/mon_memmap.c
@@ -138,6 +138,8 @@ void monitor_cpuhistory_store(CLOCK cycle, unsigned int addr, unsigned int op,
         return;
     }

+    //fprintf(stderr, "%04x\n", addr);
+
     ++cpuhistory_i;
     if (cpuhistory_i == cpuhistory_lines) {
         cpuhistory_i = 0;
```

At first startup make sure that the following settings are selected

- **Settings->Host->Autostart** - set *Autostart mode* to *Inject into RAM*.
- **Settings->Machine->RAM** - set all to zero to make sure that RAM is zero initialized after reset.

Then simply run as e.g.

```
$ x64 supermon64.prg
```

