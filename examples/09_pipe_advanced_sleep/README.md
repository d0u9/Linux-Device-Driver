# pipe\_advanced\_sleep

This example looks very like the previous example -- 08_pipe_simple_sleep.
However, the implementation details of this example is different. In this
example, we dig into the process sleep mechanisim one step further, to handle
more details of putting current process into sleep. We have to create wait
queue, set current process state, test sleep condition, and wake up process
when condition is fulfiled.

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

Copy **load_module.sh** and **pipe_advanced_sleep.ko** files to the target
machine, then run:

```bash
sh load_module.sh
```

## test the module

Read the device files named as `/dev/pipe_advanced_sleepN` via any Linux tools
you favoured. For example, here we use **cat**:

```
cat /dev/pipe_advanced_sleep0
```

The process of **cat** command will be blocked until something is writen into
the same device file by other processes.

Now, open another terminal via telnet, and write something to this device file.
Again, use any tools you favoured. We use **echo** here to demonstrate.

```
echo "hello world!" > /dev/pipe_advanced_sleep0
```

After successfully executing the command, the previous **cat** process will be
waken up and fetch the contents we just write. After print out the content
to the terminal, the **cat** process sleeps again to wait for more datas.

---

### ¶ The end
