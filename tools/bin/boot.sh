#!/bin/bash

: "${LDD_ROOT:?Enter the configured LDD workspace or export LDD_ROOT first.}"

INITRAMFS="$LDD_ROOT/initramfs.cpio.gz"
KERNELPATH="$LDD_ROOT/src/linux-stable/arch/x86/boot/bzImage"
SHARED="$LDD_ROOT/shared"

if [ ! -e "$INITRAMFS" ]; then
  echo "Could not locate initramfs.cpio.gz."
  exit 1
fi

if [ ! -e "$KERNELPATH" ]; then
  echo "Could not locate bzImage file at $KERNELPATH."
  exit 1
fi

if [ ! -d "$SHARED" ]; then
  echo "Could not locate shared directory at $SHARED."
  exit 1
fi

# TODO: Add the ldd devices from tools/qemu/ once they are written.
"${LDD_ROOT}/bin/qemu" \
    -enable-kvm \
    -kernel "$KERNELPATH" \
    -initrd "$INITRAMFS" \
    -append 'console=ttyS0' \
    -nographic \
    -nic user,model=e1000,hostfwd=tcp:127.0.0.1:7023-:23 \
    -virtfs local,path="$SHARED",mount_tag=ldd,security_model=mapped-xattr \
    -m 512M \
    -device qemu-xhci \
    -device edu

