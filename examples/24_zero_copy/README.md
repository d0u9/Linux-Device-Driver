# zero_copy

The kernel reading and writing a user buffer in place, without
`copy_to_user()` or `copy_from_user()`. A process hands its buffer to
`/dev/zero_copy0`; the driver pins the pages behind it and then reaches
them through its own mapping. Direct I/O, RDMA, V4L2's `USERPTR` buffers and
GPU userptr objects all start this way, usually to let a device DMA into the
buffer. No hardware is needed.

The ioctls, in `ioc_cmd.h`:

| ioctl | argument | does |
|-------|----------|------|
| `ZC_PIN` | `struct zc_buf { addr, len }` | pin the buffer; page-aligned, 1 to 16 pages |
| `ZC_FILL` | `__u32` | the kernel writes the value all over the buffer |
| `ZC_SUM` | `__u32`, returned | the kernel adds up the first word of every page |
| `ZC_UNPIN` | none | release the buffer; `close()` does it too |

## what each part shows

- **Pinning.** `pin_user_pages_fast()` looks up the pages behind the user
  addresses, faulting them in if needed, and pins them. A pinned page stays
  in memory and stays the page the process sees: it is not migrated, and
  on `fork()` the child gets a copy right away instead of sharing it.
  `FOLL_WRITE` says the kernel will write to it, `FOLL_LONGTERM` that it is
  kept across system calls. If only part of the buffer can be pinned, that
  part is unpinned again and the ioctl fails.
- **Reaching a page.** `kmap_local_page()` gives a kernel address for a
  page until `kunmap_local()`. After writing, `flush_dcache_page()` keeps
  the user's view up to date on CPUs whose caches can alias; on x86 it does
  nothing.
- **Unpinning.** The kernel's writes do not go through the user's page
  tables, so nothing marks the pages dirty.
  `unpin_user_pages_dirty_lock(..., true)` does that and drops the pins;
  without it, a write to a file-backed page could be lost on writeback or
  reclaim.
- **The ioctl interface.** `_IOW` and `_IOR` encode the direction and size
  of the argument; `ioc_cmd.h` uses only fixed-size types, since it is
  shared with user space. A mutex keeps two threads using one file from
  changing the pages under each other.

## older ways of doing the same

- `get_user_pages_fast()` and `put_page()`, with `set_page_dirty_lock()`
  before the put. They take ordinary references, which memory management
  cannot tell apart from any other, so it does not know the page is in
  direct use. Pages from `pin_user_pages*()` must be released with
  `unpin_user_page*()`, never `put_page()`.
- `page_address()` or `page_to_virt()` instead of `kmap_local_page()`.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

Build the test program, statically linked so that it runs in the guest:

```bash
make -C test
```

## Usage

Copy **load_module.sh**, **zero_copy.ko** and **test/zero.out** to the
target machine, then run:

```bash
sh load_module.sh
```

## test the module

```bash
./zero.out
dmesg
```

```
ok   unaligned buffer refused
ok   4 pages pinned
ok   second pin refused
ok   kernel filled the buffer
ok   user space sees 0x5a5a5a5a everywhere
ok   kernel read 0, 1, 2, 3 from the pages
ok   unpinned
ok   fill without a buffer refused
ok   pinned again, for close()
PASSED
```

`dmesg` shows the pinned buffer and the first word of each page as the
kernel read it.

---

### ¶ The end
