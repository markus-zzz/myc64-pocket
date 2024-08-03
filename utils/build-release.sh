#!/bin/bash

set -x -e

DIST=dist
STAGING=_staging_
DATE=$(date +'%Y-%m-%d')
VERSION=$1

rm -f MyC64-Pocket.zip
rm -rf ${STAGING}

pushd src/bios
make clean
make
popd

pushd src/fpga/core/myc64-rtl
python3 myc64.py
popd

pushd src/fpga/core/my1541-rtl
python3 my1541.py
popd

quartus_sh --flow compile ./src/fpga/ap_core.qpf

cp -r ${DIST} ${STAGING}

python3 utils/reverse-bits.py src/fpga/output_files/ap_core.rbf ${STAGING}/Cores/markus-zzz.MyC64/bitstream.rbf_r

sed -i "s/VERSION/${VERSION}/" ${STAGING}/Cores/markus-zzz.MyC64/core.json
sed -i "s/DATE_RELEASE/${DATE}/" ${STAGING}/Cores/markus-zzz.MyC64/core.json

pushd ${STAGING}
zip -r ../MyC64-Pocket.zip .
popd

cat src/fpga/output_files/ap_core.fit.summary
