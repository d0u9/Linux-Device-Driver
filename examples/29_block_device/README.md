# memdisk

A block device kept in memory, after LDD3's sbull: `/dev/memdisk`, 16 MB by
default, which can be partitioned, formatted and mounted like a disk. The
module shows the ways a block driver receives I/O. No hardware is
needed.

## the three request modes

File systems and the page cache describe I/O as **bios**: a start sector
and a list of segments (`struct bio_vec`: a page, an offset, a length). How
they reach the driver depends on `request_mode`:

| `request_mode` | driver entry | walks |
|----------------|--------------|-------|
| `0` simple (default) | `blk_mq_ops.queue_rq()` | the request's segments, in one loop: `rq_for_each_segment()` |
| `1` full | `blk_mq_ops.queue_rq()` | the request's bios, then each bio's segments: `__rq_for_each_bio()` and `bio_for_each_segment()` |
| `2` noqueue | `block_device_operations.submit_bio()` | each bio's segments, as the bio arrives |

In modes 0 and 1 the block layer first merges adjacent bios into a
**request**, may reorder requests with an I/O scheduler, and hands them out
with a tag from the tag set, one per request in flight. A request is thus a
list of bios, each a list of segments:

```
request: sectors 100..131
  bio 1: sectors 100..115    segment: page A, 0..4095
                             segment: page B, 0..4095
  bio 2: sectors 116..131    segment: page C, 512..4095
                             segment: page D, 0..511
```

Simple and full move the same data in the same order: full only spells out
the two levels that `rq_for_each_segment()` hides. In LDD3 they differed.
The simple request function did only the request's current chunk,
`blk_rq_cur_sectors()` of it, and completed the request piece by piece,
coming back for the rest; the full one did the whole request at once.
blk-mq hands a driver the whole request and expects it completed as a
whole, so only the difference in how the loop is written is left.

In mode 2 there is no request queue: no merging, no scheduler. Drivers for
real hardware (NVMe, SCSI, virtio-blk) use blk-mq: merging makes larger
transfers, and tags map well onto hardware queues. Stacking drivers such as
dm and md, which pass bios on to other devices, and RAM disks like `brd`
and `zram`, which gain nothing from merging, are bio-based.

## what each part shows

- **A gendisk.** `blk_mq_alloc_disk()` or `blk_alloc_disk()` creates the
  disk with its queue limits (here the logical block size); the driver
  sets its name, numbers, operations and capacity, and `add_disk()` makes
  it live: the partition table is read and `/dev/memdisk` appears.
- **Sectors.** The block layer always counts in 512-byte sectors:
  `set_capacity()`, `blk_rq_pos()` and `bi_iter.bi_sector` all use them,
  whatever `hardsect_size` is. The logical block size only sets the
  smallest unit I/O may come in.
- **Segments.** `rq_for_each_segment()` and `bio_for_each_segment()` walk
  the segments; `memcpy_from_bvec()` and `memcpy_to_bvec()` copy one
  without the driver mapping the page itself.
- **Completing.** A request ends with `blk_mq_end_request()`, a bio with
  `bio_endio()`, or `bio_io_error()` on failure. Once `queue_rq()` has
  ended the request, it returns `BLK_STS_OK` whatever the request's status;
  returning an error would have blk-mq end it again.
- **Major numbers.** `register_blkdev()` reserves a major, listed in
  `/proc/devices`. It is optional now: with `gd->major` left 0,
  `add_disk()` takes numbers from a shared dynamic major.

A driver for a disk that does DMA would turn a request into a scatterlist
with `blk_rq_map_sg()`, map it with `dma_map_sg()` and hand the segments
to the device, completing the request from its interrupt handler.

## module parameters

| parameter | default | meaning |
|-----------|---------|---------|
| `nsectors` | 32768 | size, in blocks of `hardsect_size` |
| `hardsect_size` | 512 | logical block size, a power of two from 512 to the page size |
| `request_mode` | 0 | 0 simple, 1 full, 2 noqueue |

## older ways of doing the same

- LDD3's request functions took requests off a single queue under a lock
  and could complete them piece by piece, and its "no queue" mode used
  `make_request_fn`; these are blk-mq's `queue_rq()` and `submit_bio()`
  now.
- `blk_queue_logical_block_size()` and the other `blk_queue_*()` setters
  became fields of `struct queue_limits`, given when the disk is
  allocated.
- `kmap_atomic()` on `bv_page`, or `page_address()`, are
  `memcpy_from_bvec()`, `memcpy_to_bvec()` or `bvec_kmap_local()` now.
- sbull emulated a removable medium that was "changed" 30 seconds after
  the last close. Drivers for removable media report it through
  `block_device_operations.check_events`; it is left out here.

The kernel's own RAM disk is `drivers/block/brd.c`.

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

Copy **load_module.sh**, **memdisk.ko** and **test/memdisk_test.sh** to the
target machine, then run:

```bash
sh load_module.sh
sh load_module.sh load request_mode=1    # full
sh load_module.sh load request_mode=2    # noqueue
sh load_module.sh load hardsect_size=4096 nsectors=4096
```

## test the module

```bash
sh memdisk_test.sh
```

```
ok   size is 16777216 bytes
ok   block size is 512 bytes
ok   data written and read back
ok   write past the end refused
ok   partition /dev/memdisk1 created
ok   ext2 made on /dev/memdisk1
ok   file survives umount and mount
PASSED
```

busybox `fdisk` only handles 512-byte sectors, so with a larger
`hardsect_size` the test puts the file system on the whole disk.

---

### ¶ The end
