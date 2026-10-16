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

## test the module

This driver export 8 files in the **/proc** filesystem after it is loaded.

1. **currentime**

   Read to this file will get four values:

   ```
   0x1039ec6e9 0x00000001039ec6e9 1503534097.257556
                               1503534097.256700783
   ```

   The first value is the value of `jiffies` variable, which is a 32bit
   unsigned number. The second value the 64bit version of jiffies, which can
   be obtained by `get_jiffies_64()` function. Any directly use of 64bit
   jiffies is not recommended.

   The third value obtained via `do_gettimeofday()`, which is similar to the
   `gettimeofday()` function in userspace. This function has a near microsecond
   resolution, because it asks the timing hardware what fraction of the current
   jiffy has already elapsed.

   The fourth value is returned by `current_kernel_time()`, almost the same as
   `do_gettimeofday()` but in different of data type.

2. **jitbusy**

   Any read to this file will cause the system to pause for a period which is
   designated by `delay` variable. The under layer implementation uses
   `cpu_relax()` function in a busy loop to fulfil this delay purpose. However,
   the `cpu_relax()` function actually does nothing but insert `nop`
   instructions, so the this approach should definitely be avoided whenever
   possible.

3. **jitsched**

   Read to this file will block the current read process for a period. Also the
   length of delay can be designated by `delay` variable. The under layer
   implementation in this example is that the process voluntarily yield the CPU.
   The process is still in the running queue, which can be scheduled again.

4. **jitqueue**

   In this delay implementation, we resort to wait queue. We first create and
   initialize a wait queue header. Then we add the current process to this
   queue by invoking `wait_event_timeout()` or `wait_event_interruptible_timeout()`
   functions. These two functions all have a parameter which indicates how
   long to wait before timeout.

5. **jitschedto**

   This is almost identical to the **jitqueue**, the only difference is that in
   this implementation we use `schedule_timeout()` function to avoid manually
   setting up wait queue.

6. **jitimer**

   The previous methods of delay all block the current process until the time
   has elapsed. Sometimes we need some mechanism which won't block the current
   process but can notify us the timeout. Kernel timer is best candidate.
   In this implementation, the we set the timeout value to `delay` variable. In
   the timeout handler, we print some kernel information about the current 
   execution context.

7. **jitasklet** and **jitasklethi**

   Beside kernel timer, the tasklet mechanism in kernel can also fulfil the
   same task. The different between **jitasklet** and **jitasklethi** is that
   they have different priority.

---

### ¶ The end
