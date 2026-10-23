# seeking

The `llseek` method repositions the read/write offset of an open file. Not
every char device needs it; a keyboard, for example, has no position to move
to. When a device does have positions, the driver is free to choose what a
position means, for example a block instead of a byte.

In this example, we create a char device named `/dev/seeking`. Its content is
the 16-byte table `0123456789ABCDEF`, so the byte at position N is N written
in hex: reading one byte at position 10 returns `A`. Positions wrap around
the table: position 16 is the same as 0, and reading past the last entry
continues from the start.

`seeking_llseek()` computes the new position from `whence`:

- `SEEK_SET`: `off`
- `SEEK_CUR`: the current position plus `off`
- `SEEK_END`: the table size (16) plus `off`, so `-1` means the last entry

A negative result is rejected with `-EINVAL`; anything else is taken modulo
16. `read()` also checks its position itself, because `pread()` passes an
offset straight to it without going through `llseek`.

A device with a fixed size and no wrapping can use the kernel's helpers
instead of writing `llseek` by hand: `fixed_size_llseek()` handles all three
`whence` values and rejects positions outside the device, and
`no_seek_end_llseek()` does the same but refuses `SEEK_END`, and
`default_llseek()` covers a plain byte position.

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

Copy **load_module.sh** and **seeking.ko** files to the target machine,
then run:

```bash
sh load_module.sh
```

## test the module

In the **test** directory, we write a simple test program to test the
functionality of the kernel driver.

Build it with `make` and run `./seek.out` on the target machine.

The test program reads the device from offset 15 down to 0, one byte per
read, then checks `SEEK_END`, wrapping and a negative position:

```
15(dec) = F(hex)
14(dec) = E(hex)
13(dec) = D(hex)
12(dec) = C(hex)
11(dec) = B(hex)
10(dec) = A(hex)
9(dec) = 9(hex)
8(dec) = 8(hex)
7(dec) = 7(hex)
6(dec) = 6(hex)
5(dec) = 5(hex)
4(dec) = 4(hex)
3(dec) = 3(hex)
2(dec) = 2(hex)
1(dec) = 1(hex)
0(dec) = 0(hex)
SEEK_END -1 = F
SEEK_SET 26 = A
SEEK_SET -1 fails: Invalid argument
```

---

### ¶ The end
