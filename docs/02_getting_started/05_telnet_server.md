# telnet server

> Telnet is a protocol used on the Internet or local area networks to provide
> a bidirectional interactive text-oriented communication facility using a
> virtual terminal connection. User data is interspersed in-band with Telnet
> control information in an 8-bit byte oriented data connection over the
> Transmission Control Protocol (TCP).

Sometimes, the ability to log in to the guest system for debugging is
essential. This ability helps run multiple processes in different terminals.
For example, when testing the FIFO device driver, two or more terminals are
needed to fetch and feed data from and to the FIFO device simultaneously.

Using **Telnet**, which implements the DARPA protocol, is a simple solution to
this problem. Benefiting from the integration of telnet server in BusyBox
binary, setting up telnet for remote login is pretty easy.

The `telnet` server in the BusyBox is named `telnetd`; it only provides
fundamental functionalities compared to a complete version of the telnet server
usually installed via the package manager on a regular Linux distribution.

# Mount devpts

BusyBox telnetd needs Unix98 pseudo-terminals. In the initramfs `/init` script,
add the following commands after mounting devtmpfs on `/dev` and before the
shell restart loop:

> **EDIT FILE: `$LDD_ROOT/initramfs/init` on the host → `/init` in the guest.**
> This block runs at guest boot, after `mount -t devtmpfs devtmpfs /dev`.

```sh
# Guest /init: after mounting devtmpfs, before starting telnetd.
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts
```

Create the directory at boot because devtmpfs hides the archive's original
`/dev` contents. It supplies `/dev/ptmx`, so no host-side `mknod` is needed.

**RUN ON HOST:** Rebuild the initramfs.

```bash
cd "$LDD_ROOT/initramfs"
find . -print0 | cpio --null -ov --format=newc --owner=0:0 | gzip -9 > ../initramfs.cpio.gz
```

# Configure QEMU port forwarding

QEMU's user-mode network backend forwards a host port to the guest without
requiring a host network service or bridge. The guest uses `10.0.2.15` on
`eth0`, as configured in the previous initramfs chapter.

Replace the existing network arguments (`-net nic` and `-net user`, or an
existing `-nic`/`-netdev` and network device pair) with this single argument;
do not append a second NIC:

**HOST QEMU ARGUMENT:**

```text
-nic user,model=e1000,hostfwd=tcp:127.0.0.1:7023-:23
```

`hostfwd` is an option of the user-mode network backend, not a standalone
QEMU argument. It forwards connections to host `127.0.0.1:7023` to guest
port `23`. Restart QEMU with this argument and the rebuilt initramfs.

The `telnetd -l /bin/sh` commands below provide a root shell without login
authentication. Keep the host forwarding address bound to `127.0.0.1` for
this local development setup.

# Test telnetd in QEMU guest

**RUN IN GUEST:** Start the server manually for this test.

```sh
telnetd -F -l /bin/sh
```

The option `-F` makes `telnetd` run in the foreground, suitable for
testing the network connection.

**RUN ON HOST (another terminal):** Connect to the guest server.

```
telnet 127.0.0.1 7023
```

On success, you will get a shell prompt in the new host terminal. After
testing, press Ctrl-C in the guest serial console running `telnetd -F` to
stop the foreground server before starting another instance.

# Automatic start for telnetd service.

Add the line below to `/init` after mounting devpts and before the
`while :; do` shell restart loop. Lines after that loop will never run:

> **EDIT FILE: `$LDD_ROOT/initramfs/init` on the host → `/init` in the guest.**
> This block runs at guest boot, after mounting devpts and before the shell loop.

```sh
# Guest /init: after mounting devpts, before the shell restart loop.
telnetd -l /bin/sh
```

The server runs in the background without `-F`. Rebuild the initramfs again
on the host, then restart QEMU with the same port forwarding argument:

**RUN ON HOST:**

```bash
cd "$LDD_ROOT/initramfs"
find . -print0 | cpio --null -ov --format=newc --owner=0:0 | gzip -9 > ../initramfs.cpio.gz
```

---

# ¶ The end

1. [QEMU Networking][1]

[1]: https://wiki.qemu.org/Documentation/Networking
