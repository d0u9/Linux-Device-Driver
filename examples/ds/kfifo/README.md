# kfifo

The kernel's ring buffer, `<linux/kfifo.h>`. On load the module writes
and reads a byte fifo until it wraps and fills, then shows a fifo of
ints, a fifo of variable-length records and an allocated fifo, and checks
what comes out.

## two counters that never go back

A kfifo is a buffer of a power-of-2 size and two unsigned counters: `in`
counts every element ever put, `out` every one taken. Neither is reset
or wrapped by hand:

- the slot is the counter masked with `size - 1`;
- the length is `in - out`, right even after `in` passes `UINT_MAX`,
  because unsigned subtraction wraps too.

So there is no full-or-empty ambiguity and no wasted slot, which is why
the size must be a power of 2. The writer changes only `in` and the
reader only `out`: one writer and one reader need no lock. More than one
of either take a lock, with `kfifo_in_spinlocked()` and
`kfifo_out_spinlocked()`.

```
empty:                 [........] out=0 in=0 len=0 avail=8
in "abcde":            [abcde...] out=0 in=5 len=5 avail=3
after out:             [...de...] out=3 in=5 len=2 avail=6
in "fghijk", wraps:    [ijkdefgh] out=3 in=11 len=8 avail=0
in "xyz" when full: 0 copied
out all: "defghijk"
drained:               [........] out=11 in=11 len=0 avail=8
```

## what each part shows

- **Bytes.** `kfifo_in()` and `kfifo_out()` copy as much as fits or as
  much as there is, and return the count; they never block or fail.
  `kfifo_peek()` looks without taking.
- **Typed elements.** The type is part of the fifo: `kfifo_put()` and
  `kfifo_get()` move one element and return 0 when full or empty.
- **Records.** `STRUCT_KFIFO_REC_1` stores each record behind a 1-byte
  length (`_REC_2`: 2 bytes). Each `kfifo_out()` consumes one record,
  copying at most the destination size; any remaining bytes of that
  record are discarded. Use `kfifo_peek_len()` to size the destination
  before reading when truncation is unwanted. The demo reads `hello`
  into a 3-byte destination: it gets `hel`, and the next record is `hi`.
  Three records containing 13 bytes of data take 16 bytes in the fifo.
- **Allocated.** `DECLARE_KFIFO_PTR()` with `kfifo_alloc()`, which rounds
  the size up to a power of 2, and `kfifo_free()`. `DEFINE_KFIFO()` has
  the buffer inside the structure and needs neither.

## where the kernel uses it

- Data between an interrupt handler and a reader, in serial, input and
  USB drivers.
- `kfifo_to_user()` and `kfifo_from_user()` copy straight to and from
  user space in `read()` and `write()`.
- `kfifo_dma_in_prepare()` gives the free space as a scatterlist for DMA.

The kernel's samples are in `samples/kfifo/`.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

## test the module

```bash
insmod kfifo.ko && dmesg | tail -n 30
rmmod kfifo
```

The log ends with `kfifo check passed`.

---

### ¶ The end
