#!/bin/bash

KERNEL="5.16.19"
MACHINE="x86_64"
SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
export LDD_ROOT="$(cd "${SCRIPTDIR}"/.. && pwd)"

QEMUPATH="${LDD_ROOT}/qemu/build/qemu-system-${MACHINE}"
if [ -e "${QEMUPATH}" ]; then
  if [ ! -e "${LDD_ROOT}/bin/qemu" ]; then
    echo "Soft linking qemu to qemu-system-${MACHINE}"
    ln -s "${QEMUPATH}" "${LDD_ROOT}/bin/qemu"
  fi
else  
  echo "Failed to find QEMU executable at ${QEMUPATH}."
  exit 1
fi

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


${LDD_ROOT}/bin/qemu \
    -enable-kvm \
    -kernel "$KERNELPATH" \
    -initrd "$INITRAMFS" \
    -append 'console=ttyS0' \
    -nographic \
    -net nic,model=e1000 \
    -net user,hostfwd=tcp::7023-:23 \
    -vnc none \
    -m 512M \
    -device isa-kmod-edu,chardev=pr1 \
    -chardev file,id=pr1,path=/tmp/isa_edu \
    -device pci-kmod-edu \
    -device qemu-xhci \
    -device usb-kmod-edu \
    -device edu

