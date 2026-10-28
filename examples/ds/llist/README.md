# llist

The kernel's lock-less list, `<linux/llist.h>`. On load the module shows
the order items come out in, then runs two producer threads adding 10000
items each while the module's own thread takes them, with no lock.

## no lock

The head is one pointer, and `llist_add()` pushes with `cmpxchg()`: any
number of producers can add at the same time, including from interrupt
context. NMI use additionally requires NMI-safe `cmpxchg`, indicated by
`CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG`. The consumer takes the whole list
at once with `llist_del_all()`, an `xchg()` that leaves the head empty,
and then walks what it took at leisure: nobody else can reach it any more.

What is given up:

- **Order.** Pushing at the front gives newest first;
  `llist_reverse_order()` turns a batch around.
- **Taking one.** `llist_del_first()` needs the consumers serialized,
  since another consumer could free the node it is reading.
- **Deleting from the middle**, and walking the list while it is shared.

`llist_add()` returns true if the list was empty: that producer is the one
to wake the consumer, and the others need not.

## output

```
added 1..5, taken:         5 4 3 2 1
reversed:                  1 2 3 4 5
20000 items from 2 producers in 13694 batches, list found empty 13694 times
```

The number of batches varies with the run and the number of CPUs; with
one CPU the producers finish before the consumer runs, and there are only
a couple. Each batch began with a producer finding the list empty.

## the threads

The producers are created with `kthread_create()`, their `task_struct`s
held with `get_task_struct()` before they are woken: a producer may be
done and gone before `kthread_stop()` is called for it, which then needs
the reference. Producer allocation failures are returned through
`kthread_stop()` as `-ENOMEM`, after all threads are joined and queued
items are freed.

## where the kernel uses it

- `irq_work` and `call_single_queue`: functions queued to run on a CPU
  from any context.
- Deferred freeing, such as `vfree()` from interrupt context.
- The scheduler's wake-up lists and many drivers' completion queues,
  filled from an interrupt and drained by a worker.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

## test the module

```bash
insmod llist.ko && dmesg | tail -n 10
rmmod llist
```

The log ends with `llist check passed`.

---

### ¶ The end
