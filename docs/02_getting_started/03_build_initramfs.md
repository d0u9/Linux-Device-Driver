# Build initramfs

# what is initramfs

Excerpt from [Linux kernel documentation]:

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
mkdir -p "$LDD_ROOT/initramfs"
cd "$LDD_ROOT/initramfs"

# Create necessary directories
mkdir -p {bin,dev,etc,lib,lib64,mnt,proc,root,sbin,sys,tmp}

# Set Permission
chmod 1777 tmp

# Create guest device nodes without depending on the host's serial devices.
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1
sudo mknod -m 666 dev/tty c 5 0
sudo mknod -m 600 dev/ttyS0 c 4 64
```

# Install busybox

> BusyBox combines tiny versions of many common UNIX utilities into a single
> small executable. It provides replacements for most of the utilities you
> usually find in GNU fileutils, shellutils, etc. The utilities in BusyBox
> generally have fewer options than their full-featured GNU cousins; however,
> the options that are included provide the expected functionality and behave
> very much like their GNU counterparts. BusyBox provides a fairly complete
> environment for any small or embedded system.

Use the official BusyBox **1.35.0 x86_64 musl** static binary from
[BusyBox's binary downloads]. This matches the 64-bit guest without requiring
`CONFIG_IA32_EMULATION`.

```bash
cd "$LDD_ROOT/initramfs"

wget https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox -O bin/busybox
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

Enable `CONFIG_DEVTMPFS=y` in the guest kernel for the `/dev` mount below.
It must be mounted explicitly when booting from initramfs.

```bash
cd "$LDD_ROOT/initramfs"

cat << 'EOF' > init
#!/bin/busybox sh

export PATH=/bin:/sbin
export HOME=/root

# Mount guest filesystems.
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs devtmpfs /dev

# Boot real things.

# NIC up
ip link set eth0 up
ip addr add 10.0.2.15/24 dev eth0
ip link set lo up

# Wait for NIC ready
sleep 0.5

# Start a login shell and restart it on exit so PID 1 stays alive.
while :; do
    setsid sh -c 'exec sh -l </dev/ttyS0 >/dev/ttyS0 2>&1'
    sleep 1
done

EOF

chmod +x init
```

`sh -l` starts a login shell that reads `/etc/profile`. `setsid` creates a
new session, and opening `/dev/ttyS0` gives the shell a controlling terminal
for job control. See [BusyBox's FAQ]. Exiting the shell starts a new one;
use QEMU's exit shortcut to stop the guest.

The devtmpfs mount hides the device nodes stored in the archive and supplies
nodes for devices registered with the driver core. Examples that only register
a device number still need `mknod`. For the later Telnet setup, create
`/dev/pts` after this mount, then mount devpts there.

## More setups

```bash
cd "$LDD_ROOT/initramfs"

# name resolve
cat << EOF > etc/hosts
127.0.0.1    localhost
10.0.2.2     host_machine
EOF

# common alias
cat << EOF > etc/profile
alias ll='ls -l'
EOF

# Root account metadata; the init script starts the shell directly.
cat << EOF > etc/passwd
root:x:0:0:root:/root:/bin/sh
EOF

# group file
cat << EOF > etc/group
root:x:0:
EOF
```

# Build initramfs image.

```bash
cd "$LDD_ROOT/initramfs"

find . -print0 | cpio --null -ov --format=newc --owner=0:0 | gzip -9 > ../initramfs.cpio.gz
```

# Run our kernel in QEMU

```bash
qemu -enable-kvm \
     -kernel "$LDD_ROOT/src/linux-stable/arch/x86/boot/bzImage" \
     -initrd "$LDD_ROOT/initramfs.cpio.gz" \
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
[BusyBox's binary downloads]: https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/
[BusyBox's FAQ]: https://busybox.net/FAQ.html#job_control
