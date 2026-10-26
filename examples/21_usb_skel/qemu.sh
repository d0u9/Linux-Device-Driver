#!/bin/sh
# RUN ON HOST: boot the guest with the ldd-usb device on the xHCI controller
# that boot.sh already adds. Extra arguments are passed on to QEMU.
exec "${LDD_ROOT:?export LDD_ROOT first}/bin/boot.sh" \
    -device ldd-usb,id=usb0 \
    "$@"
