# nfs support

In last post, we have been successfully built and run a basic ram based
initramfs in QEMU to test our kernel modules. However, the first question we
maybe ask is how to tansfer our binary kernel module file (the .ko file) into
this test machine. Of course, we have many different means to fulfil this task,
for example the tftp tool or even by nc command. Consider the inconvenient use
(have to manually invoke the tansfer each time after we rebuid the module), NFS
is a better choice to share common file accross the host and the guest.

# Setup NFS server on host

1. Install NFS server.

   On Ubuntu/Debian Host:

   ```bash
   sudo apt-get install nfs-kernel-server
   ```
   Consult the manual if you are using different host OS.

2. Export our working directory.

   Appending the following contents to `/etc/exports`:

   ```bash
   cat << EOF >> /etc/exports
   /path/to/working/directory   127.0.0.1(insecure,rw,sync,no_root_squash)
   EOF
   ```

   Note that the `insecure` option is compulsory or `refused mount request from
   127.0.0.1 for /xxxx (/xxx): illegal port xxxx` error will be reported.

# Auto mount NFS in guest

   In last post, we configured the kernel by running `make defconfig`, which
   defaultly enables the NFS support in kernel (tested on kernel 4.9). Make
   sure NFS support is enabled in your kernel before continue.

   Add the following line in `initramfs/init` right after NIC ready:

   ```bash
   mount -t nfs -o nolock host_machine:/path/to/working/directory /mnt
   ```

   Rebuild the initramfs. Note that we already have the **host_machine** name
   solved in `initramfs/etc/hosts` file.


# `initramfs/init` file

   ```bash
   #!/bin/busybox sh

   # Mount the /proc and /sys filesystems.
   mount -t proc none /proc
   mount -t sysfs none /sys

   # Boot real things.

   # NIC up
   ip link set eth0 up
   ip addr add 10.0.2.15/24 dev eth0
   ip link set lo up

   # wait for NIC ready
   sleep 0.5

   # mount our working directory
   mount -t nfs -o nolock host_machine:/nfs/mapped_dir /mnt

   # make the new shell as a login shell with -l option
   # only login shell read /etc/profile
   setsid sh -c 'exec sh -l </dev/ttyS0 >/dev/ttyS0 2>&1'
   ```

---

# Reference

1. [http://www.nathanfriend.co.uk/nfs-refused-mount-illegal-port/][1]


[1]: http://www.nathanfriend.co.uk/nfs-refused-mount-illegal-port/

---

### ¶ The end
