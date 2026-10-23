# hello world

This example demonstrates the basic concepts and process needed in building a
kernel module.


## build the module

To build this module for the QEMU guest, run on the host from this directory:

```bash
make KDIR="$LDD_ROOT/src/linux-stable"
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither the `KDIR` environment variable nor the make option is set,
the module is built against the running host kernel, which may differ from
the guest kernel. Workspace activation sets `LDD_ROOT`, not `KDIR`.

## run the module

Copy **hello_world.ko** file to the target machine, then run:

```bash
insmod hello_world.ko
```

Inspect the result by running:

```bash
dmesg | tail -10
```
At the very end of the output, a `Hello, world!` message will be seen.

## remove the module

When the module is no longer needed, remove it from the kernel to
release all the resources it occupied.

```bash
rmmod hello_world
```

Run `dmesg | tail -10` again, message `Bye, world!` can be seen. The exit
function is executed properly.

---

### ¶ The end
