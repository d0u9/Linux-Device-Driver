# nfs support

As a developer, move files between the target machine and local host is a common
operation. For QEMU, there is no ssh server running inside (of course, you may
install one and configure it properly); Moving files both side is an urgent
requirement for development process.

To solve this problem, using NFS is a simple and grace solution. Running the
NFS server on your development host, and mount it in QEMU virtual machine is
fairly simple. That no any tool need to be installed in QEMU, makes life
fast and easy.

# Setup NFS server on host

Install NFS server on Ubuntu/Debian host by running apt command:

```bash
sudo apt-get install nfs-kernel-server
```

For other Linux distributions, consult the manual for help.

Setup NFS server to export working directory:

```bash
sudo bash -c "echo \
    '$LDD_ROOT/nfs_dir        127.0.0.1(insecure,rw,sync,no_root_squash)' \
    >> /etc/exports"
```

The `insecure` option is compulsory, or an error of "refused mount request from
127.0.0.1 for /xxxx (/xxx): illegal port xxxx" will report.

# Test NFS mounting in QEMU guest

Mount host's NFS filesystem in qemu:

```bash
mount -t nfs -o nolock host_machine:/path/to/working/directory /mnt
```

The actual value of `/path/to/working/directory` is the result of evaluation of
`$LDD_ROOT/nfs_dir`. Due to the fact that executing the mount command in QEMU
guest, reference to this environment variable is not working. Type it manually
in QEMU's terminal is needed.

# Auto mount NFS in guest

For a fast development, manually mounting this NFS at each time the QEMU guest
booting is time consuming. Setup an automatically mounting is good for speeding
up development circle.


Adding mounting command in **init** script, which we mentioned in previous
chapter, right after the `sleep 0.5` command to enable auto mounting ability.

```bash
# Add this line in init script. Put it just after the line of sleep 0.5.
mount -t nfs -o nolock host_machine:/path/to/working/directory /mnt
```

Rebuild the initramfs.

```bash
cd $LDD_ROOT/initramfs

find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
```

# ¶ The end

---

# Reference

1. [http://www.nathanfriend.co.uk/nfs-refused-mount-illegal-port/][1]


[1]: http://www.nathanfriend.co.uk/nfs-refused-mount-illegal-port/

---
