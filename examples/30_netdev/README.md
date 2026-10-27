# netdev

A network driver without hardware, after LDD3's snull: two interfaces,
`sn0` and `sn1`, wired to each other. A frame sent by one is received by
the other, so the whole path a real driver handles, transmit, the
transmit-done and receive interrupts, and receive, can be watched with
`ping` and TCP. No hardware is needed.

## the trick

Both interfaces are in the same machine, and the kernel would deliver a
packet for one of its own addresses locally instead of sending it out. So
snull invents a remote address on each side, and while "on the wire" flips
the lowest bit of the third byte of the source and destination addresses:

```
sn0  192.168.0.1/24                       sn1  192.168.1.2/24

ping 192.168.0.2 from sn0
  192.168.0.1 -> 192.168.0.2   --- wire --->   192.168.1.1 -> 192.168.1.2
                                                 for sn1: answer it
  192.168.0.1 <- 192.168.0.2   <--- wire ---   192.168.1.1 <- 192.168.1.2
```

`192.168.0.2` is sn1 as seen from sn0, and `192.168.1.1` is sn0 as seen
from sn1. There is no ARP (`IFF_NOARP`): the Ethernet header is built with
the destination set to the device's own address with the last bit flipped,
which is the twin's (`00:53:4e:55:4c:30` and `...:31`, "\0SNUL0" and
"\0SNUL1").

## what each part shows

- **Registering.** `alloc_netdev()` allocates the `net_device` with room for
  the driver's data (`netdev_priv()`) and calls `ldd_setup()`, which uses
  `ether_setup()` and sets the operations; `register_netdev()` makes the
  interface appear.
- **Opening and stopping.** `ndo_open` and `ndo_stop` run on
  `ip link set up` and `down`: they start and stop the transmit queue and
  NAPI.
- **Transmitting.** `ndo_start_xmit()` gets one skb at a time and returns
  `NETDEV_TX_OK` once it has taken it. Each device has a pool of
  `pool_size` buffers standing for the hardware's transmit memory: taking
  the last one stops the queue (`netif_stop_queue()`), and the receiver
  giving one back wakes it (`netif_wake_queue()`). The frame is copied
  into the buffer and only the copy is changed: an skb's data may be
  shared with clones, like the one TCP keeps for retransmission. Only IPv4
  is passed; anything else, such as IPv6's own traffic, is dropped.
- **Interrupts.** There are none, so the driver calls its handler where
  the hardware would interrupt: on the receiving device when a frame is
  queued, on the sending one when the transmission is done, which frees
  the skb with `dev_kfree_skb_any()`.
- **Receiving.** The frame is copied into a new skb, `eth_type_trans()`
  works out the protocol, and the skb goes up the stack. The skb gets
  `NET_IP_ALIGN` bytes of headroom first, so that the IP header behind the
  14-byte Ethernet header is 4-byte aligned.
- **NAPI** (`use_napi=1`). Instead of one interrupt per frame, the first
  frame's interrupt turns receive interrupts off and schedules
  `ldd_poll()`, which takes up to a budget of frames at a time from the
  softirq. When it finds no more, it calls `napi_complete_done()`, turns
  receive interrupts back on, and checks the queue once more for a frame
  that came in between. Under load this replaces an interrupt per frame
  with a poll per batch. The weight here is 2, so that budgets run out
  with only a few frames.
- **Statistics.** The counters in `dev->stats`, updated with
  `DEV_STATS_INC()` and `DEV_STATS_ADD()`, show in `ip -s link` and
  `/sys/class/net/*/statistics/`; no `ndo_get_stats` is needed.
- **Checksums.** The devices claim `NETIF_F_HW_CSUM`, so the stack leaves
  TCP and UDP checksums to the "hardware", and the receiver marks frames
  `CHECKSUM_UNNECESSARY`. The address rewrite would break those checksums
  anyway; the IP header checksum is recomputed with `ip_send_check()`.

## module parameters

| parameter | default | meaning |
|-----------|---------|---------|
| `pool_size` | 8 | transmit buffers per device |
| `use_napi` | 0 | receive with NAPI polling |

## shorter ways of writing the same

- `netdev_alloc_skb_ip_align()` is `dev_alloc_skb()` plus the
  `NET_IP_ALIGN` reserve.
- `netif_napi_add()` uses the default weight, 64.

## older ways of doing the same

- A private `struct net_device_stats` returned by `ndo_get_stats`; the core
  now reports `dev->stats` itself.
- `printk_ratelimit()` is `net_ratelimit()`, or `netdev_*_ratelimited()`.
- Interrupt handlers took a `struct pt_regs *` until 2.6.19.
- LDD3 queued received frames last in, first out, which reordered them.

The kernel's own pair of linked interfaces is `veth`, used for
containers: its two ends sit in different network namespaces, so no
address trick is needed.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

## Usage

Copy **load_module.sh**, **netdev.ko** and **test/netdev_test.sh** to the
target machine, then run:

```bash
sh load_module.sh                     # loads and sets up both addresses
sh load_module.sh load use_napi=1
sh load_module.sh load pool_size=1
```

## test the module

```bash
ping -I sn0 192.168.0.2
ip -s link show sn1
sh netdev_test.sh
```

```
ok   ping 192.168.0.2 from sn0, answered by sn1
ok   ping 192.168.1.1 from sn1, answered by sn0
ok   full-size frames (1514 bytes)
ok   rx_packets counted on both devices
ok   256 KB over TCP arrive intact
PASSED
```

With `pool_size=1` every frame stops the sender's queue until the receiver
gives the buffer back.

---

### ¶ The end
