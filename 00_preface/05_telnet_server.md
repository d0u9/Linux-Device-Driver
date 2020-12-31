# telnet server

> Telnet is a protocol used on the Internet or local area networks to provide
> a bidirectional interactive text-oriented communication facility using a
> virtual terminal connection. User data is interspersed in-band with Telnet
> control information in an 8-bit byte oriented data connection over the
> Transmission Control Protocol (TCP).

Sometimes, the ability to login into guest system for debuging is essential.
This ability helps to run multiple processes in different terminal. For
example, when testing FIFO device driver, two or more terminals are needed
in which processes fetching and feeding data to and from FIFO device 
simultaneously.

Using **Telenet** is a simple solution to this problem. Benefit from the
integration of telnet server in BusyBox binary, setting up telnet for remote
login is fairly easy.

busybox contains a telnet server, i.e `telnetd`. We cannot directly use
`telnetd` command to start telnet server if we have not set pty properly.

The telenet server in Busybox is command `telentd`; it is not a fully functional
service of telnet, which only provides fundamental functionalities. To use
Busybox's `telentd` command, a **pts** device node must exist.

# Setup pts device node

```bash
cd $LDD_ROOT/initramfs

mkdir dev/pts
mknod -m 666 dev/ptmx c 5 2
```

Automatically mount devpts device during boot time. Append the line below to
`init` script.


```bash
mount -t devpts devpts  /dev/pts
```

Rebuild the initramfs.

```bash
cd $LDD_ROOT/initramfs

find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
```

# Before initiating telnet server

Don't , netw
Normally, telnet uses port 23 as its default port. However, QEMU uses User 
Networking as its default networking backend. This type of networking backend
is simple and easy to use, no root privilege is required. But, the drawback is
obvious:

- Performance punishment.
- The guest is not directly accessible from the host.

The inability of accessing guest from host is crucial to the problem of how to
connect the telnet server from host.

To overcome this restriction, users may use other networking backend or the
handy option `hostfwd` which is specific of solving this problem.

To map port 23 of telnet to 7023 on the host, append the option below to your
QEMU command:

```bash
-net nic,model=e1000 -net user,hostfwd=tcp::7023-:23 \
```

These options instruct QEMU to use e1000 NIC, and map tcp port 7023 on host to
port 23 of guest.


# Test telnetd in QEMU guest

Boot your QEMU guest, and run the command in it to start telnet server:

```bash
telnetd -F -l /bin/sh
```

The option `-F` makes `telnetd` run in foreground. Just for testing the network
connection.


On host side, connect to QEMU guest by running:

```
telent localhost 7023
```

Successfully, the guest's shell promotion will be seen in your terminal. If
everything goes correctly, it is time to make it auto start.

# Automatic start for telnetd service.

Add the line below in your init script:

```bash
telnetd -l /bin/sh
```

---

# ¶ The end

1. [QEMU Networking][1]

[1]: https://wiki.qemu.org/Documentation/Networking




