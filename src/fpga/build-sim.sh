#!/bin/bash

set -e
OBJ_DIR=obj_dir
rm -rf $OBJ_DIR

/home/markus/work/install/bin/verilator --trace-fst -cc +1364-2005ext+v --top-module core_top core/spram.v core/sprom.v core/core_top.v core/core_bridge_cmd.v apf/common.v core/myc64/rtl/myc64/*.v core/picorv32.v -Wno-fatal \
+define+__VERILATOR__=1 -CFLAGS -O3

VERILATOR_ROOT=/home/markus/work/install/share/verilator
cd $OBJ_DIR; make -f Vcore_top.mk; cd ..

g++ -std=c++14 core_top-sim.cpp $OBJ_DIR/Vcore_top__ALL.a -I$OBJ_DIR/ -I$VERILATOR_ROOT/include/ -I$VERILATOR_ROOT/include/vltstd $VERILATOR_ROOT/include/verilated.cpp $VERILATOR_ROOT/include/verilated_threads.cpp $VERILATOR_ROOT/include/verilated_fst_c.cpp -Werror -I. -o core_top-sim -O3 -g0 `pkg-config --cflags --libs gtk+-3.0` -lz
