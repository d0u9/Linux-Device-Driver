# telnet server

> Telnet is a protocol used on the Internet or local area networks to provide
> a bidirectional interactive text-oriented communication facility using a
> virtual terminal connection. User data is interspersed in-band with Telnet
> control information in an 8-bit byte oriented data connection over the
> Transmission Control Protocol (TCP).

busybox contains a telnet server, i.e `telnetd`. We cannot directly use
`telnetd` command to start telnet server if we have not set pty properly.

# Setup pty

   First, create **pts** directory in dev:

   ```bash
   # Change the current directory to intramfs dir.
   cd /path/to/initramfs

   mkdir dev/pts
   ```

   Modify `initramfs/init` to auto mount devpts, add the command below to your
   `init` file:

   ```bash
   mount -t devpts devpts  /dev/pts
   ```

# Launch `telnetd` service in guest

   Map the telnet port 23 to host as 7023 by appending the below options to
   `qemu-system-X86_64`:

   ```
   -net nic,model=e1000 -net user,hostfwd=tcp::7023-:23
   ```

   So far, the complete qemu command is :

   ```bash
   qemu-system-x86_64 \
       -enable-kvm \
       -net nic,model=e1000 \
       -net user,hostfwd=tcp::7023-:23 \
       -kernel $KERNELDIR/arch/x86_64/boot/bzImage \
       -initrd initramfs.cpio.gz \
       -append 'console=ttyS0' \
       -nographic
   ```

   In the QEMU guest, for testing purpose, run `telnetd` with option `-F` to
   make the service run in foreground.

   ```bash
   telnetd -F -l /bin/bash
   ```

   Then test if everthing is working, run the command on yout host machine:

   ```bash
   telnet localhost 7023
   ```

   If nothing wried happen, add `telnetd -l /bin/sh` to your `init` file for
   auto launching.

# `initramfs/init` file

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

   # wait for NIC ready
   sleep 0.5

   # mount our working directory
   mount -t nfs -o nolock host_machine:/home/doug/Linux_Kernel/Linux-Device-Driver /mnt

   # Start telnetd
   telnetd -l /bin/sh

   # make the new shell as a login shell with -l option
   # only login shell read /etc/profile
   setsid sh -c 'exec sh -l </dev/ttyS0 >/dev/ttyS0 2>&1'
   ```
---

### ¶ The end






