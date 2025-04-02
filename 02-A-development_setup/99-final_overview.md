# Final Overview

To make life eaiser, I post the full content of `init` and a script for lunching QEMU guest here:

## init

```bash
cat << EOF > $LDD_ROOT/initramfs/init
#!/bin/busybox sh

# Mount the /proc and /sys filesystems.
mount -t proc none /proc
mount -t sysfs none /sys

# Boot real things.

# NIC up
ip link set eth0 up
ip addr add 10.0.2.15/24 dev eth0
ip link set lo up

# Mount devpts
mount -t devpts devpts  /dev/pts

# Wait for NIC ready
sleep 0.5

# Mount nfs
mount -t nfs -o nolock host_machine:/home/doug/projects/ldd /mnt

# Start telnetd service
telnetd -l /bin/sh

# Our examples
export EXPDIR=/mnt/Linux-Device-Driver/99-Examples

# Make the new shell as a login shell with -l option
# Only login shell read /etc/profile
setsid sh -c 'exec sh -l </dev/ttyS0 >/dev/ttyS0 2>&1'
EOF
```

## QEMU script

The auxilaury script for lunching QEMU guest. Paste below content to `$LDD_ROOT/bin/ldd-launch.sh`;

```bash
#!/bin/bash

# Detect host architecture
hardware=$(uname -m)
case "$hardware" in
  x86_64)
    host_arch="x86_64"
    ;;
  aarch64)
    host_arch="arm64"
    ;;
  *)
    echo "Error: Unsupported architecture $host_arch"
    exit 1
    ;;
esac


if [ -z "${LDD_ROOT}" ]; then
  echo "Error: LDD_ROOT environment variable is not set"
  exit 1
fi

if ! command -v ldd-qemu &> /dev/null; then
  echo "Error: ldd-qemu command not found"
  exit 1
fi

initramfs_img="$LDD_ROOT/initramfs.cpio.gz"
if [ ! -f "$initramfs_img" ]; then
  echo "Error: initramfs.cpio.gz not found"
  exit 1
fi

kernel_img="$LDD_ROOT/kernel/linux-current/arch/${host_arch}/boot/bzImage"
if [ ! -f "$kernel_img" ]; then
  echo "Error: kernel image not found"
  exit 1
fi

ldd-qemu \
  -enable-kvm \
  -kernel "$kernel_img" \
  -initrd "$initramfs_img" \
  -append 'console=ttyS0' \
  -nographic \
  -net nic,model=e1000 \
  -net user,hostfwd=tcp::7023-:23 \
  -vnc none \
  -m 512M \
  -device qemu-xhci \
  ;
```

Grant execution permission to the script.

```bash
chmod +x $LDD_ROOT/bin/ldd-launch.sh
```
