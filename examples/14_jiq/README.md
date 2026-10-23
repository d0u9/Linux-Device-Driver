# jiq

**jiq** ("just in queue") shows the ways a driver can defer work to run later,
in another context, and how those contexts differ.

The module creates four read-only files in **/proc**. Reading one of them
runs `jiq_print()` again and again in a different context. Each call appends
one line: the jiffies, the difference to the previous line,
`preempt_count()`, the pid, the CPU and the command name of the current
process. When the buffer is full, or the timer has fired, the reader is woken
up and prints the buffer.

| file         | runs `jiq_print()` from                                   |
|--------------|-----------------------------------------------------------|
| `jiqwq`      | a work item on the shared workqueue, re-queued each time  |
| `jiqwqdelay` | a delayed work item, `delay` jiffies apart                |
| `jiqtimer`   | a kernel timer, once, one second later                    |
| `jiqtasklet` | a BH work item, re-queued each time                       |

Watch the `preempt` column. Work items run in a kworker thread in process
context: `preempt_count()` is 0 and they may sleep. Timers and BH work run in
softirq context: `preempt_count()` is non-zero, they must not sleep, and the
pid and command are those of whatever process happened to be interrupted.

Only one reader is served at a time. If a reader is interrupted by a signal,
the work item, timer or BH work it started is cancelled before the read
returns, and everything is cancelled again on unload.

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

Copy **jiq.ko** to the target machine, then run:

```bash
insmod jiq.ko
```

or, to set the interval of **jiqwqdelay** in jiffies:

```bash
insmod jiq.ko delay=10
```

## test the module

```bash
cat /proc/jiqwq
cat /proc/jiqwqdelay
cat /proc/jiqtimer
cat /proc/jiqtasklet
```

`jiqtimer` prints two lines, one from the reader and one from the timer a
second later. The other files print lines until the buffer is full.

## notes

- `jiqtasklet` used a tasklet (`tasklet_init()` and `tasklet_schedule()`),
  as in LDD3. Tasklets are deprecated; since Linux 6.9 BH workqueues replace
  them, and the file now queues its work on `system_bh_wq`. The name is kept.
- `schedule_work()` queues on `system_percpu_wq`, which used to be called
  `system_wq`; the old name is going away. `system_dfl_wq` is the unbound
  alternative.
- `jiqwq` sleeps by hand with `prepare_to_wait()`, `schedule()` and
  `finish_wait()`, as in 09_pipe_advanced_sleep. The loop tests both the
  completion flag and `signal_pending()` after every wakeup, because
  `schedule()` also returns for signals, for a `wake_up()` meant for another
  file sharing the wait queue, and spuriously. The other files use
  `wait_event_interruptible()`, which expands to the same loop.

---

### ¶ The end
