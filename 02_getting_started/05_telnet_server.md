# telnet server

> Telnet is a protocol used on the Internet or local area networks to provide
> a bidirectional interactive text-oriented communication facility using a
> virtual terminal connection. User data is interspersed in-band with Telnet
> control information in an 8-bit byte oriented data connection over the
> Transmission Control Protocol (TCP).

Sometimes, the ability to login into the guest system for debugging is
essential. This ability helps run multiple processes in different terminals.
For example, when testing the FIFO device driver, two or more terminals are
needed to fetch and feed data from and to the FIFO device simultaneously.

Using **Telnet**, which implements the DARPA protocol, is a simple solution to
this problem. Benefiting from the integration of telnet server in BusyBox
binary, setting up telnet for remote login is pretty easy.

The `telnet` server in the Busybox is named `telnetd`; it only provides
fundamental functionalities compared to a complete version of the telnet server
usually installed via the package manager on a regular Linux distribution.

# Setup pts device node

It must have a **pts** device node exist to make Busybox function correctly.
Create the device node by invoking:

```bash
cd $LDD_ROOT/initramfs

mkdir dev/pts
mknod -m 666 dev/ptmx c 5 2
```

Automatically mount the `devpts` device during the boot time by appending the
line below to the `init` script after the nfs mount line.


```bash
mount -t devpts devpts  /dev/pts
```

Then, rebuild the initramfs.

```bash
cd $LDD_ROOT/initramfs

find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
```

# Before initiating telnet server

Typically, `telnet` uses port 23 as its default port. However, due to the
default networking backend opted by QEMU, the "User Networking",  accessing
port 23 of a guest machine is not straightforward. In contrast to the simple
and easy using and no privilege is required of this backend, its drawback is
evident as well:

    - Performance punishment.
    - The guest is not directly accessible from the host.

The inability to access the guest imposed by QEMU's networking backend is
crucial to the problem of connecting the telnet server from the host. To
overcome this restriction, users may use other networking backend or append the
handy option `-hostfwd`. The `-hostfwd` option tells QEMU the rule that forwards
a port from the guest to the host. In this telnet scenario, to map port 23 in
the guest to port 7023 on the host, append the option below to your QEMU
command:

```bash
-netdev user,id=host_net,hostfwd=tcp::7023-:23 \
-device e1000,mac=52:54:00:12:34:50,netdev=host_net \
```

The options instruct QEMU to use an e1000 NIC and forward the TCP port 23 from
the guest to 7023 on the host.

# Test telnetd in QEMU guest

Boot your QEMU guest, and run the command in it to start the telnet server:

```bash
telnetd -F -l /bin/sh
```

The option `-F` makes `telnetd` run in the foreground, suitable for
testing the network connection.

On the host side, connect to the telnet server in the QEMU guest by running:

```
telnet localhost 7023
```

On successful, you will get the shell promoter in the terminal. Next, make this
telnetd server start automatically again by writing the line in the `init`
script.

# Automatic start for telnetd service.

Add the line below in your init script:

```bash
telnetd -l /bin/sh
```
---

# ¶ The end

1. [QEMU Networking][1]

[1]: https://wiki.qemu.org/Documentation/Networking




