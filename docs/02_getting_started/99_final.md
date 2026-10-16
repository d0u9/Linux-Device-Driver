# Summary

To this point, we have properly set up our QEMU development environment for
Linux driver development. The previous chapters covered:

- Patch and build QEMU.
- Configure and build the Linux guest kernel image.
- Set up an initramfs image to boot the guest and provide its basic configuration.
- Mount a shared host directory in the guest using 9p over virtio.
- Set up a Telnet service to allow users to create multiple shell sessions.

As a summary, we paste the complete configurations of QEMU command and initramfs'
init script below for a quick reference.

# initramfs' init script

> **EDIT FILE: `$LDD_ROOT/initramfs/init` on the host → `/init` in the guest.**
> This script runs at guest boot.

```sh
#!/bin/busybox sh

export PATH=/bin:/sbin
export HOME=/root

# Mount guest filesystems.
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs devtmpfs /dev
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts

# Boot real things.

# NIC up
ip link set eth0 up
ip addr add 10.0.2.15/24 dev eth0
ip link set lo up

# Wait for NIC ready
sleep 0.5

# Mount the shared directory
mount -t 9p -o trans=virtio,version=9p2000.L,cache=none ldd /mnt

# Start telnet service
telnetd -l /bin/sh

# Start a login shell and restart it on exit so PID 1 stays alive.
while :; do
    setsid sh -c 'exec sh -l </dev/ttyS0 >/dev/ttyS0 2>&1'
    sleep 1
done
```

Rebuild the initramfs after editing this file, as described in the
[initramfs chapter](03_build_initramfs.md#build-initramfs-image).

# QEMU command options

For samples in this repo, some specific hardware is essential to test and debug
our drivers. Some of these hardware devices are integrated in QEMU, and the others are
implemented by myself for the pure purpose of education.

It is very convenient to write the long QEMU command in an auxiliary shell script
to prevent typing it each time.

The custom devices below require the QEMU patch, whose migration to QEMU
11.1.2 is still pending. Until then, omit the `isa-kmod-edu`, `pci-kmod-edu`,
and `usb-kmod-edu` device arguments when testing the basic guest setup.

**RUN ON HOST:**

```bash
#!/bin/bash

"$LDD_ROOT/bin/qemu" \
   -nic user,model=e1000,hostfwd=tcp:127.0.0.1:7023-:23 \
   -enable-kvm \
   -m 512M \
   -kernel "$LDD_ROOT/src/linux-stable/arch/x86/boot/bzImage" \
   -initrd "$LDD_ROOT/initramfs.cpio.gz" \
   -append 'console=ttyS0' \
   -nographic \
   -virtfs local,path="$LDD_ROOT/shared",mount_tag=ldd,security_model=mapped-xattr \
   -device isa-kmod-edu,chardev=pr1 \
   -chardev file,id=pr1,path=/tmp/isa_edu \
   -device pci-kmod-edu \
   -device qemu-xhci \
   -device usb-kmod-edu \
   -device edu
```

# ¶ The end

