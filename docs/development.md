# Development

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

## Testing / verification

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

## 1541

```
$ { echo 'atn,clk,dat'; cat iec.csv; } | sigrok-cli -I csv:header=yes:samplerate=1mhz -i - -o iec.sr
$ pulseview iec.sr
```

```
$ ./core_top-sim --dump-video --keys "[150]LOAD<LSHIFT>2<LSHIFT>4<LSHIFT>2,8<RETURN>[400]LIST<RETURN>[450]LOAD<LSHIFT>2MANIAC<SPACE>MANSION<LSHIFT>2,8<RETURN>[2000]RUN<RETURN>[2700]<SPACE>" --trace dump.fst --trace-begin-frame 7600 --g64 ~/Downloads/mm.g64
```
