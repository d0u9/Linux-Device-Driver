# Share files with 9p over virtio

Use QEMU's VirtFS support to share a host directory with the guest. This requires
no host file-sharing service or guest networking. The shared host directory is
`$LDD_ROOT/shared`.

Use the guest kernel configured and built in
[Compile Linux Kernel](02_compile_Linux_Kernel.md#configure-linux-kernel),
which includes the virtio PCI transport and 9p filesystem support.

# Export the host directory through QEMU

The QEMU build must have VirtFS enabled (`--enable-virtfs` in the earlier
build instructions). Run QEMU as your normal user with access to the shared
directory:

```bash
mkdir -p "$LDD_ROOT/shared"
qemu -enable-kvm \
    -kernel "$LDD_ROOT/src/linux-stable/arch/x86/boot/bzImage" \
    -initrd "$LDD_ROOT/initramfs.cpio.gz" \
    -append 'console=ttyS0' \
    -nographic \
    -virtfs local,path="$LDD_ROOT/shared",mount_tag=ldd,security_model=mapped-xattr
```

When using another launch command, add the same `-virtfs` argument to it.
`ldd` is the mount tag used by the guest, not a hostname or path.
`mapped-xattr` stores guest ownership and permissions in host extended
attributes; the shared host filesystem must support them. Guest writes are
limited by the host user's access rights.

# Mount the share in the guest

Run inside the guest:

```sh
mkdir -p /mnt
mount -t 9p -o trans=virtio,version=9p2000.L,cache=none ldd /mnt
ls /mnt
```

The host directory `$LDD_ROOT/shared/Linux-Device-Driver` appears as
`/mnt/Linux-Device-Driver` in the guest. Files are shared in both directions.
`cache=none` avoids caching file data and metadata while the host rebuilds
modules. The guest does not need to know the host's absolute directory path.

# Mount automatically at boot

Add this line to the initramfs `/init` script after mounting `/proc`, `/sys`,
and `/dev`, and before the shell restart loop:

```sh
mount -t 9p -o trans=virtio,version=9p2000.L,cache=none ldd /mnt
```

Then rebuild the archive on the host and restart QEMU with the `-virtfs`
argument:

```bash
cd "$LDD_ROOT/initramfs"
find . -print0 | cpio --null -ov --format=newc --owner=0:0 | gzip -9 > ../initramfs.cpio.gz
```

# Reference

- [QEMU invocation: VirtFS](https://www.qemu.org/docs/master/system/invocation.html)
- [Linux 9p filesystem](https://docs.kernel.org/filesystems/9p.html)

# ¶ The end
