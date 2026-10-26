#!/bin/sh
# RUN ON HOST: boot the guest with the ldd-io device in I/O port space at 0x200.
# Watch the panel and type into the keyboard from two more terminals:
#   nc 127.0.0.1 7100     # panel
#   nc 127.0.0.1 7101     # keyboard
# Extra arguments are passed on to QEMU.
exec "${LDD_ROOT:?export LDD_ROOT first}/bin/boot.sh" \
    -chardev socket,id=panel0,host=127.0.0.1,port=7100,server=on,wait=off \
    -chardev socket,id=kbd0,host=127.0.0.1,port=7101,server=on,wait=off \
    -device ldd-io,panel=panel0,keyboard=kbd0 \
    "$@"
