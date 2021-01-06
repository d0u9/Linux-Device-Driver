#!/bin/zsh

QBIN = "$HOME/WorkDir/qemu/build/x86_64-softmmu/qemu-system-x86_64"

gdb -q --args \
$QBIN \
                   -enable-kvm \
                   -vnc none \
                   -m 6G \
                   -kernel linux-5.7.8/arch/x86_64/boot/bzImage \
                   -initrd initramfs.cpio.gz \
                   -append 'console=ttyS0' \
                   -nographic \
                   -device isa-kmod-edu,chardev=pr1 \
                   -chardev file,id=pr1,path=/tmp/isa_edu \
                   -device pci-kmod-edu \
                   -device qemu-xhci \
                   -device usb-kmod-edu \
                   -device edu \
                   ;
