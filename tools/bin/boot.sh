#!/bin/bash

KERNEL="5.16.19"
: "${LDD_ROOT:?Enter the configured LDD workspace or export LDD_ROOT first.}"

INITRAMFS="$LDD_ROOT/initramfs.cpio.gz"

if [ ! -e "$INITRAMFS" ]; then
  echo "Could not locate initramfs.cpio.gz."
  exit 1
fi

KERNELPATHA="${LDD_ROOT}/kernels/linux-${KERNEL}/arch/x86_64/boot/bzImage"
if [ ! -e "${KERNELPATHA}" ]; then
  KERNELPATHB="${LDD_ROOT}/kernel/linux-${KERNEL}/arch/x86_64/boot/bzImage"
  if [ ! -e "${KERNELPATHB}" ]; then
    echo "Could not locate bzImage file in linux kernel in ${KERNELPATHA} and ${KERNELPATHB}."
    exit 1
  else
    KERNELPATH="${KERNELPATHB}"
  fi
else
  KERNELPATH="${KERNELPATHA}"
fi


"${LDD_ROOT}/bin/qemu" \
    -enable-kvm \
    -kernel "$KERNELPATH" \
    -initrd "$INITRAMFS" \
    -append 'console=ttyS0' \
    -nographic \
    -net nic,model=e1000 \
    -net user,hostfwd=tcp::7023-:23 \
    -m 512M \
    -device isa-kmod-edu,chardev=pr1 \
    -chardev file,id=pr1,path=/tmp/isa_edu \
    -device pci-kmod-edu \
    -device qemu-xhci \
    -device usb-kmod-edu \
    -device edu

