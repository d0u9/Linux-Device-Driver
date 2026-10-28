#!/bin/sh
# RUN ON HOST: boot the guest. boot.sh already adds the ldd-pcie device
# behind a PCI Express root port. Extra arguments are passed on to QEMU.
exec "${LDD_ROOT:?export LDD_ROOT first}/bin/boot.sh" "$@"
