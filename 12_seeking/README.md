# seeking

Seeking method is used to reposition read/write offset of the opened file.
Obviously, not all the char devices need to implement this method. For example,
the keyboard device is one of this sort which can not be repositioned. The
seeking method is easy to implement, driver designer is free to choose the
appropriate meaning of the position to their devices. For example, use the
device block as the basic unit to offset, instead of byte.

Here, in this example, we create a simple char device file named "seeking" in
`/dev` directory. It has an internal buffer containing a DEC-HEX table, which
can be used to convert decimal number to its hex counterpart. Read the
different position of this device file can get the hex format number
accordingly. For example, read one byte from the position of 10 from the device
file, a char of `A` is returned. Read after position of 15, for example 16,
will cause the position cursor wrapping around.

## build the module

To build this module, execute:

```bash
make KERNELDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KERNELDIR` environment variable, simply
execute `make` is enough.

If neither `KERNELDIR` environment variable nor `KERNELDIR` option of make
are set, the current running kernel will be built against.

## Usage

Copy **load_module.sh** and **seeking.ko** files to the target machine,
then run:

```bash
sh load_module.sh
```


---

### ¶ The end
