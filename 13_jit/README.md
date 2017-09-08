# jit

How to measure time in the Kernel? How to delay a specific time period in the
Kernel? Time is very essential to softwares. The entire Kernel and related
function a propelled by time, the timer device hardware interrupts the Kernel
periodically to spur the Kernel running.

To driver developers, how to get the current time, how to measure the elapsed
time, how to delay the execution of a specific task is common cases.

In this example, we implement a simple deriver which exports files in the root
of **/proc** filesystem. Reads to each of these files will get current time of
kernel, or delay a short period which may delay the read process of the whole
kernel.

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

Copy **jit.ko** files to the target machine, then run:

```bash
insmod jit.ko
```

or

```bash
insmod jit.ko delay=TICK_NUMBER
```


---

### ¶ The end
