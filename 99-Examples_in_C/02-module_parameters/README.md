# module parameters

Several parameters that a driver needs to know can change from system to system.
These can vary from the device number to use to numerous aspects of how the
driver should operate.

This example shows how our kernel module get parameters from user at load time
by insmod or modprobe.

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

Copy **hello_world.ko** file to the target machine, then run:

```bash
insmod module_parameters.ko
```

Running `dmesg | tail -10`, you can find something like this:

```
[ 1141.505799] parameters test module is loaded
[ 1141.508953] #0 Hello, Mom
```

It is right that we defautly set to print **hello, Mom** only one time. Next we
unload this module and pass parameters to it when loading.

```bash
# unload the module
rmmod module_parameters

# re-load the module and passing parameters to it
insmod module_parameters.ko whom=dady howmany=3
```

This time, the message is changed:

```
[ 1322.364784] parameters test module is loaded
[ 1322.366768] #0 Hello, dady
[ 1322.367999] #1 Hello, dady
[ 1322.369154] #2 Hello, dady
```

---

### ¶ The end

