# pipe\_advanced\_sleep

This example behaves exactly like the previous one, 08_pipe_simple_sleep,
including `stream_open()` and the read position kept in the device. The
difference is how a process is put to sleep: instead of the
`wait_event_interruptible()` macro, it spells out the steps the macro hides.

## sleeping by hand

```c
DEFINE_WAIT(wait);                                       /* 1 */
prepare_to_wait(&dev->rd_queue, &wait, TASK_INTERRUPTIBLE); /* 2 */
if (!READ_ONCE(dev->buff_len))                           /* 3 */
	schedule();
finish_wait(&dev->rd_queue, &wait);                      /* 4 */
if (signal_pending(current))
	return -ERESTARTSYS;
```

1. `DEFINE_WAIT()` declares a wait queue entry for the current process.
2. `prepare_to_wait()` adds the entry to the queue and sets the process state
   to `TASK_INTERRUPTIBLE`.
3. Only now is the condition tested again; `schedule()` gives up the CPU if
   there is still nothing to read.
4. `finish_wait()` sets the state back to `TASK_RUNNING` and removes the
   entry from the queue. A signal ends the wait with `-ERESTARTSYS`.

The order of steps 2 and 3 is what makes this safe. The mutex has to be
dropped before sleeping, so a writer can slip in at any time:

- If the writer runs before step 2, it cannot find the reader on the queue
  and its wakeup goes nowhere. Step 3 sees the new data, so the reader does
  not sleep.
- If the writer runs after step 2, even between the test and `schedule()`,
  it finds the reader on the queue and sets it back to `TASK_RUNNING`, so
  `schedule()` returns right away.

Testing first and joining the queue afterwards would lose the wakeup in the
first case and leave the reader asleep for good.

`buff_len` is read without the mutex in step 3, so it is read with
`READ_ONCE()` and written with `WRITE_ONCE()`. These keep the compiler from
splitting or caching the access and mark the race as intended; they do not
add CPU memory ordering, which the wait queue lock already provides.

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

The process of **cat** command will be blocked until something is written into
the same device file by other processes.

Now, open another terminal via telnet, and write something to this device file.
Again, use any tools you favoured. We use **echo** here to demonstrate.

```
echo "hello world!" > /dev/pipe_advanced_sleep0
```

After the command succeeds, the previous **cat** process is woken up and
fetches the contents we just wrote. After printing them to the terminal, the
**cat** process sleeps again to wait for more data.

---

### ¶ The end
