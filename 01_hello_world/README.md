# hello world

This example demostrates the baisc concepts and process needed in building a
kernel module.


## build the module

To build this module, execute:

```bash
make KERNELDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KERNELDIR` environment variable, simply
execute `make` is enough.

If neither `KERNELDIR` environment variable nor `KERNELDIR` option of make
are set, the current running kernel will be built against.

## run the module

Copy **hello_world.ko** file to the target machine, then run:

```bash
insmod hello_world.ko
```

Inspect the result by running:

```bash
dmesg | tail -10
```
At the very end of the output, a `Hello World` message will be seen.

## remove the module

When the module is not longger needed anymore, remove it from the kernel to
release all the resources it occupied.

```bash
rmmod hello_world
```

Run `dmesg | tail -10` again, message `Bye, world` can be seen. The exit
function is executed properly.

---

### ¶ The end
