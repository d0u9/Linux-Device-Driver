# pipe\_simple\_sleep

In this example, we implement a simple pipe liked device, in which one process
read it will be blocked until another process writes something to the same
device. We call this example as _simple\_sleep_ because the kernel APIs we used
in this example is simple. We have not to deep down into the internal
mechanisim of how wait queue works.

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

Copy **load_module.sh** and **pipe_simple_sleep.ko** files to the target
machine, then run:

```bash
sh load_module.sh
```

## test the module

Read the device files named as `/dev/pipe_simple_sleepN` via any Linux tools
you favoured. For example, here we use **cat**:

```
cat /dev/pipe_simple_sleep0
```

The process of **cat** command will be blocked until something is writen into
the same device file by other processes.

Now, open another terminal via telnet, and write something to this device file.
Again, use any tools you favoured. We use **echo** here to demonstrate.

```
echo "hello world!" > /dev/pipe_simple_sleep0
```

After successfully executing the command, the previous **cat** process will be
waken up and fetch the contents we just write. After print out the content
to the terminal, the **cat** process sleeps again to wait for more datas.

---

### ¶ The end
