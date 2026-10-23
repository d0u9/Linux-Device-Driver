# jit

How do you measure time in the kernel, and how do you delay for a given
period? The timer hardware interrupts the kernel periodically, and much of
the kernel is driven by those ticks. Drivers often need to read the current
time, measure an elapsed time, or delay some work.

This example creates eight read-only files in the root of **/proc**. Reading
each one either prints the current time or delays for a while using a
different method. The files are created with `proc_create_single_data()`, so
each one only needs a `show` function.

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

Copy **jit.ko** files to the target machine, then run:

```bash
insmod jit.ko
```

or

```bash
insmod jit.ko delay=TICK_NUMBER
```

`delay` (default `HZ`, one second) sets the delay of the `jit*` files in
jiffies.

## test the module

The driver creates eight files in **/proc**.

1. **currentime**

   ```
   0x1039ec6e9 0x00000001039ec6e9 1503534097.257556
                               1503534097.256700783
   ```

   - `jiffies`, an `unsigned long` tick counter.
   - `get_jiffies_64()`, the 64-bit counter. Read it through this function;
     on 32-bit machines a direct read is not atomic.
   - `ktime_get_real_ts64()`, the wall-clock time with high resolution,
     printed here in microseconds.
   - `ktime_get_coarse_real_ts64()`, the same clock at tick resolution, but
     cheaper to read.

   `tv_sec` is a 64-bit `time64_t`; never cast it to `int`, or it overflows in
   2038. Older kernels had `do_gettimeofday()` and `current_kernel_time()`,
   which were removed because of that problem.

2. **jitbusy**

   Busy-waits with `cpu_relax()` until `delay` jiffies have passed. The CPU is
   kept busy the whole time, so avoid this except for very short delays.

3. **jitsched**

   Calls `schedule()` in a loop until the time has passed. The process stays
   runnable and only lets other processes run first.

4. **jitqueue**

   Sleeps on a wait queue with `wait_event_interruptible_timeout()`. Nobody
   wakes the queue, so it always returns after the timeout.

5. **jitschedto**

   Sets the state to `TASK_INTERRUPTIBLE` and calls `schedule_timeout()`.
   `schedule_timeout_interruptible()` does both steps in one call.

6. **jitimer**

   Arms a kernel timer that fires five times, every 10 jiffies, without
   blocking the code that armed it. Each run records the jiffies, whether it
   runs in softirq context, the pid, the CPU and the command name of the
   interrupted process. The reader waits until all five runs are done.

7. **jitasklet** and **jitasklethi**

   The same as **jitimer**, but each run queues the next one on a BH
   workqueue: `system_bh_wq` for **jitasklet** and `system_bh_highpri_wq` for
   **jitasklethi**.

   These two files used to use tasklets (`tasklet_schedule()` and
   `tasklet_hi_schedule()`), as in LDD3. Tasklets are deprecated, and since
   Linux 6.9 BH workqueues are their replacement: work items queued there
   also run in softirq context, and the high priority queue plays the role
   of the high priority tasklet. The file names are kept from the tasklet
   days.

Both timers and BH work must be stopped before the memory they use is freed.
The example calls `timer_shutdown_sync()` and `disable_work_sync()` before
`kfree()`, also when the reader is interrupted by a signal while the timer
or work item is still queued.

## choosing a delay function

`Documentation/timers/delay_sleep_functions.rst` in the kernel source gives
the current advice:

- In atomic context (spinlock held, interrupt handler): `ndelay()`,
  `udelay()`, `mdelay()`; these busy-wait, keep them short.
- In process context: `fsleep(us)` picks the right sleeping function for the
  duration; `usleep_range()` and `msleep()` can be used directly, and
  `msleep_interruptible()` lets a signal end the sleep early.

---

### ¶ The end
