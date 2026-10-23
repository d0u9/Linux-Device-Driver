# pipe\_simple\_sleep

In this example, we implement a simple pipe-like device. A process reading it
is blocked until another process writes something to the same device, and a
writer is blocked until the previous data has been read. We call this example
_simple\_sleep_ because it only uses the high-level wait queue helpers,
`wait_event_interruptible()` and `wake_up_interruptible()`, without diving
into how wait queues work internally. The buffer holds only 8 bytes, so
longer messages take several rounds.

Like a real pipe, the device has no file position. The buffer and the read
position are shared by all openers and kept in the device structure. `open()`
calls `stream_open()`, so `lseek()`, `pread()` and `pwrite()` fail with
`-ESPIPE`, and the driver never uses the `f_pos` argument.

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

The process of **cat** command will be blocked until something is written into
the same device file by other processes.

Now, open another terminal via telnet, and write something to this device file.
Again, use any tools you favoured. We use **echo** here to demonstrate.

```
echo "hello world!" > /dev/pipe_simple_sleep0
```

After the command succeeds, the previous **cat** process is woken up and
fetches the contents we just wrote. After printing them to the terminal, the
**cat** process sleeps again to wait for more data.

---

### ¶ The end
