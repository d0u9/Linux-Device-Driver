#!/bin/bash

QSRC_DIR="$HOME/WorkDir/qemu"
mkdir -p build
cd build

if [ $# -gt 0 ]; then
../configure \
    --prefix=$HOME/Apps/cellar/qemu \
    --enable-virtfs \
    --sysconfdir=/home/doug/Apps/cellar/qemu/etc \
    --enable-debug \
    --extra-cflags="-g3" \
    --extra-ldflags="-g3" \
    --disable-strip \
    --disable-docs \
    --target-list=i386-softmmu,x86_64-softmmu
fi

make -j$(nproc)
