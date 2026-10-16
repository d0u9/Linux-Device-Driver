# nfs support

As a developer, moving files between the target machine and the local host is a
common operation, especially during the process of development and debugging.
However, for QEMU guests, there is no such ssh server running inside that is
used for syncing files to and from guests.

To this problem, NFS is a simple and grace solution. Running an NFS server on
your development host and mounting the NFS filesystem in QEMU virtual machine
is fairly simple. That no tool needs to be installed in QEMU guests makes life
fast and easy.

# Setup NFS server on host

Install NFS server on an Ubuntu/Debian host by running the `apt-get` command:

For Ubuntu:

```bash
sudo apt-get install nfs-kernel-server
```

For Fedora:

```bash
sudo dnf -y install nfs-utils
sudo systemctl enable --now rpcbind nfs-server
```
For other Linux distributions, consult the manual for help.

Setup NFS server to export a working directory:

```bash
sudo bash -c "echo \
    '$LDD_ROOT        127.0.0.1(insecure,rw,sync,no_root_squash)' \
    >> /etc/exports"
```

The `insecure` option is compulsory, without it an error of "refused mount
request from 127.0.0.1 for /xxxx (/xxx): illegal port xxxx" will report.

# Test NFS mounting in QEMU guest

Mount host's NFS filesystem in QEMU guest:

```bash
mount -t nfs -o nolock host_machine:/path/to/working/directory /mnt
```

The actual value of `/path/to/working/directory` is the result of evaluating
the `$LDD_ROOT` variable. Due to the fact that the final `mount` command is
executed in QEMU guest in which the `$LDD_ROOT` variable is not accessible, you
have to expand this variable manually to its real absolute path.

# Auto mount NFS in guest

For agile development, manually mounting this NFS filesystem each time the QEMU
guest booting is time-consuming. It is extremely unfriendly for the development
circle of sync-build-test. To ease this redundant work, set up automatic
mounting in the `init` script by appending the command below right after the
`sleep 0.5` command.

```bash
# Add this line in init script. Put it just after the line of sleep 0.5.
mount -t nfs -o nolock host_machine:/path/to/working/directory /mnt
```

Then, rebuild the initramfs.

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
