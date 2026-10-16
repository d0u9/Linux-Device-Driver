# scull

To best illustrate the concepts behind the basic file operations, device
**scull** is the only choice. **scull** is a character device driver backed by
memory. A user space program can write data to the device and read it back
without consuming the stored data. Opening the device with `O_WRONLY` calls
`scull_trim()` to clear the stored data and free the associated memory.

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

Copy **load_module.sh** and **scull.ko** files to the target machine, then run:

```bash
sh load_module.sh
```

## test the module

Write anything by any Linux tool you favoured to any device file named as
`/dev/scullN`. Then execute `dmesg | tail -10`, you will find some messages
like:

```
[ 3850.344538] scull: scull_open() is invoked
[ 3850.344541] scull: scull_trim() is invoked
[ 3850.344552] scull: scull_write() is invoked
[ 3850.344556] scull: WR pos = 12, block = 0, offset = 0, write 12 bytes
[ 3850.344559] scull: scull_release() is invoked
```

Next, read the device file which you've written to before, if success, contents
previously wrote into are extracted. `dmesg | tail -10` will look like:

```
[ 4000.317270] scull: scull_open() is invoked
[ 4000.317279] scull: scull_read() is invoked
[ 4000.317286] scull: RD pos = 12, block = 0, offset = 0, read 12 bytes
[ 4000.317622] scull: scull_read() is invoked
[ 4000.317624] scull: RD pos = 12, block = 0, offset = 12, read 65536 bytes
[ 4000.317634] scull: scull_release() is invoked
```

---

### ¶ The end
