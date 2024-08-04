#!/bin/bash

set -x -e

curl -o basic.bin      http://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/basic.901226-01.bin
curl -o characters.bin http://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/characters.901225-01.bin
curl -o kernal.bin     http://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/kernal.901227-03.bin
curl -o 1540-c000.bin  http://www.zimmers.net/anonftp/pub/cbm/firmware/drives/new/1541/1540-c000.325302-01.bin
curl -o 1541-e000.bin  http://www.zimmers.net/anonftp/pub/cbm/firmware/drives/new/1541/1541-e000.901229-01.bin
