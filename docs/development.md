# Development

## Setup

For a Ubuntu like environment the following packages are needed
```
$ sudo apt-get install clang lld llvm python3 pip
$ pip install amaranth
$ pip install amaranth-yosys
```

## Build instructions

The easiest way to get started is to run the entire flow with the release builder script
```
$ PATH=$PATH:/home/markus/intelFPGA_lite/22.1std/quartus/bin/ ./utils/build-release.sh "X.X.X"
```

## 1541 debugging

```
$ sigrok-cli -I csv:header=yes:samplerate=1mhz -i iec.csv -o iec.sr
$ pulseview iec.sr
```

```
$ ./core_top-sim --dump-video --keys "[150]LOAD<LSHIFT>2<LSHIFT>4<LSHIFT>2,8<RETURN>[400]LIST<RETURN>[450]LOAD<LSHIFT>2MANIAC<SPACE>MANSION<LSHIFT>2,8<RETURN>[2000]RUN<RETURN>[2700]<SPACE>" --trace dump.fst --trace-begin-frame 7950 --g64 ~/Downloads/mm.g64
```

## Misc

Encode a `.mp4` of simulation output
```
$ ffmpeg -r 50 -pattern_type glob -i 'vicii-*.png' -c:v libx264 -pix_fmt yuv420p  pigsquest.mp4
```

