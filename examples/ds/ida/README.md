# ida

The kernel's ID allocator, `<linux/idr.h>`. On load the module numbers
four devices, frees one, allocates in a range until it runs out, and
destroys the allocator.

## numbers, nothing else

An IDA hands out the smallest unused non-negative integer and takes it
back. It keeps one bit per ID, so it is small, and stores nothing with
the ID. A driver uses one to name its devices, `foo0`, `foo1`, ..., and
to reuse the number of a device that went away. It locks itself.

When an object has to be found by its ID, an xarray with `xa_alloc()`
both allocates the ID and keeps the pointer; an IDA only numbers.

## what each part shows

- **Allocating.** `ida_alloc()` returns an ID from 0 to `INT_MAX`,
  `-ENOMEM` on allocation failure, or `-ENOSPC` when no ID is available.
- **Freeing.** `ida_free()`; the freed ID is the first to be reused.
- **Ranges.** `ida_alloc_range()`, `ida_alloc_min()`, `ida_alloc_max()`,
  with inclusive bounds and `-ENOSPC` when the range is full.
- **Destroying.** `ida_destroy()` frees every ID at once.
- `ida_find_first_range()` and `ida_exists()` look at the IDs in use;
  the module uses them to print the list.

## output

```
4 devices:               foo0 foo1 foo2 foo3
foo1 removed:            foo0 foo2 foo3
one more:                foo0 foo1 foo2 foo3
alloc in 10..11: 10
alloc in 10..11: 11
alloc in 10..11: -28
```

## older ways of doing the same

- `ida_simple_get()` and `ida_simple_remove()` were removed in 2024; they
  are `ida_alloc_range()` and `ida_free()`.
- Before that, `ida_pre_get()` and `ida_get_new()`, retried by hand.
- The IDA is built on the xarray now; once it was on the IDR.

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
insmod ida.ko && dmesg | tail -n 15
rmmod ida
```

The log ends with `destroyed, empty: yes`.

---

### ¶ The end
