# Build initramfs

# what is initramfs

Except from [Linux kernel documentation]:

> All 2.6 Linux kernels contain a gzipped "cpio" format archive, which is
> extracted into rootfs when the kernel boots up.  After extracting, the
> kernel checks to see if rootfs contains a file "init", and if so it executes
> it as PID 1. If found, this init process is responsible for bringing the
> system the rest of the way up, including locating and mounting the real root
> device (if any).  If rootfs does not contain an init program after the
> embedded cpio archive is extracted into it, the kernel will fall through to
> the older code to locate and mount a root partition, then exec some variant
> of /sbin/init out of that.

# Create initramfs image

```bash
# Create initramfs directory
mkdir $LDD_ROOT/initramfs
cd $LDD_ROOT/initramfs

# Create necessary directories
mkdir -p {bin,dev,etc,lib,lib64,mnt,proc,root,sbin,sys,tmp}

# Set Permission
chmod 1777 tmp

# Copy necessary device files from host, root privilege maybe needed.
cp -a /dev/{null,console,tty,ttyS0} dev/
```

# Install busybox

> BusyBox combines tiny versions of many common UNIX utilities into a single
> small executable. It provides replacements for most of the utilities you
> usually find in GNU fileutils, shellutils, etc. The utilities in BusyBox
> generally have fewer options than their full-featured GNU cousins; however,
> the options that are included provide the expected functionality and behave
> very much like their GNU counterparts. BusyBox provides a fairly complete
> environment for any small or embedded system.

**busybox** is distributed as a static linked binary. User can find all history
and latest versions on [busybox's official web page].

```bash
cd $LDD_ROOT/initramfs

wget https://www.busybox.net/downloads/binaries/1.30.0-i686/busybox -O bin/busybox
chmod +x bin/busybox

# Install busybox
bin/busybox --install bin
bin/busybox --install sbin
```

The `--install` option instructs **busybox** to create utilities in `bin` and
`sbin` directories.

# Compose init script

Kernel executes init script as PID 1 process. This init process is responsible
for bringing the rest of the system up properly and setting up necessary
configurations. These setups include mounting filesystems, assigning IP
addresses for Ethernet devices, etc.

```bash
cd $LDD_ROOT/initramfs

cat << EOF > init
#!/bin/busybox sh

# Mount the /proc and /sys filesystems.
mount -t proc none /proc
mount -t sysfs none /sys

# Boot real things.

# NIC up
ip link set eth0 up
ip addr add 10.0.2.15/24 dev eth0
ip link set lo up

# Wait for NIC ready
sleep 0.5

# Make the new shell as a login shell with -l option
# Only login shell read /etc/profile
setsid sh -c 'exec sh -l </dev/ttyS0 >/dev/ttyS0 2>&1'

EOF

chmod +x init
```

The `-l` option of `setsid` command in **init** script is compulsory. That lack
of this option will cause an error of "sh: can't access tty; job control turned
off" during system booting. It is detailed in [Busybox's FAQ].

## More setups

```bash
cd $LDD_ROOT/initramfs

# name resolve
cat << EOF > etc/hosts
127.0.0.1    localhost
10.0.2.2     host_machine
EOF

# common alias
cat << EOF > etc/profile
alias ll='ls -l'
EOF

# busybox saves password in /etc/passwd directly, no /etc/shadow is needed.
cat << EOF > etc/passwd
root:x:0:0:root:/root:/bin/bash
EOF

# group file
cat << EOF > etc/group
root:x:0:
EOF
```

# Build initramfs image.

```bash
cd $LDD_ROOT/initramfs

find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
```

# Run our kernel in QEMU

```bash
qemu -enable-kvm \
     -kernel $LDD_ROOT/kernels/linux-5.10.4/arch/x86_64/boot/bzImage \
     -initrd $LDD_ROOT/initramfs.cpio.gz \
     -append 'console=ttyS0' \
     -nographic
```

Press `<C-A> x` to terminate QEMU.

# Reference

1. [https://landley.net/writing/rootfs-howto.html][1]
2. [http://jootamam.net/howto-initramfs-image.htm][2]
3. [https://wiki.gentoo.org/wiki/Custom_Initramfs][3]
4. [https://busybox.net/FAQ.html][4]


[1]: https://landley.net/writing/rootfs-howto.html
[2]: http://jootamam.net/howto-initramfs-image.htm
[3]: https://wiki.gentoo.org/wiki/Custom_Initramfs
[4]: https://busybox.net/FAQ.html

---

# ¶ The end

[Linux kernel documentation]: https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git/tree/Documentation/filesystems/ramfs-rootfs-initramfs.txt?h=v4.9.30
[busybox's official web page]: https://www.busybox.net/downloads/binaries
[Busybox's FAQ]: (https://www.busybox.net/FAQ.html#job_control)
