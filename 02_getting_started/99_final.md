# Summary

To this point, we have properly setup up our QEMU development environment for
Linux Driver. During the chapters before, a few works have stepped including:

- Patch and build QEMU.
- Configure and build Linux kernel image, on which all examples will run.
- Setup an initramfs image to help booting system up and give some basic
  configurations to system.
- Mount NFS filesystem in QEMU guest for fast sharing files between host and guest.
- Setup telnet service to allow users create multiple shell environment.

As a summary, we paste the complete configurations of QEMU command and initramfs'
init script below for a quick reference.

# initramfs' init script

```bash
#!/bin/busybox sh

# Mount the /proc and /sys filesystems.
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devpts devpts  /dev/pts

# Boot real things.

# NIC up
ip link set eth0 up
ip addr add 10.0.2.15/24 dev eth0
ip link set lo up

# Wait for NIC ready
sleep 0.5

# Mount NFS
mount -t nfs -o nolock host_machine:/home/doug/LDD_ROOT /mnt

# Start telnet service
telnetd -l /bin/sh

# Make the new shell as a login shell with -l option
# Only login shell read /etc/profile
setsid sh -c 'exec sh -l </dev/ttyS0 >/dev/ttyS0 2>&1'
```

# QEMU command options

For samples in this repo, some specific hardwares is essential to test and debug
our drivers. Some of these hardwares are integrated in QEMU, and the others are
implemented by myself for the pure purpose of education.

It is very convenient to write the long QEMU command in a auxiliary shell script
to prevent typing it each time.

```bash
#!/bin/bash

$LDD_ROOT/bin/qemu \
   -net nic,model=e1000
   -net user,hostfwd=tcp::7023-:23 \
   -enable-kvm \
   -vnc none \
   -m 512M \
   -kernel $LDD_ROOT/kernels/linux-5.10.4/arch/x86_64/boot/bzImage \
   -initrd $LDD_ROOT/initramfs.cpio.gz \
   -append 'console=ttyS0' \
   -nographic \
   -device isa-kmod-edu,chardev=pr1 \
   -chardev file,id=pr1,path=/tmp/isa_edu \
   -device pci-kmod-edu \
   -device qemu-xhci \
   -device usb-kmod-edu \
   -device edu
```

# ¶ The end

