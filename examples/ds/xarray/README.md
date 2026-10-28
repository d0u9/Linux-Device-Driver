# xarray

The kernel's array of pointers indexed by `unsigned long`,
`<linux/xarray.h>`. On load the module stores at indices far apart,
loads, replaces, marks and erases entries, lets a second xarray choose
the indices, checks the result and frees everything.

## a sparse array

On a 64-bit kernel, an xarray looks like an array of `2^64` pointers,
all NULL, but only the parts in use are allocated: indices 0, 7, 1000
and 2^40 cost a few small nodes. Inside it is a radix tree of 64-way nodes, and lookups take the
RCU read lock only. Writers are serialized by a spinlock the xarray
holds itself, taken by modifying `xa_*()` calls. Callers can use
`xa_lock()` with the advanced `xas_*()` API or suitable `__xa_*()`
helpers for several updates under one lock. The lock protects the
xarray structure, not the lifetime of the pointed-to objects; concurrent
readers need their own lifetime protection, such as RCU or references.
This demo has no concurrent readers, so it frees replaced entries directly.

## what each part shows

- **Storing.** `xa_store()` returns the old entry, which is the caller's
  to free, or an error entry to test with `xa_is_err()`.
  `xa_insert()` stores only where there is nothing, else `-EBUSY`.
- **Loading.** `xa_load()` returns the entry or NULL.
- **Walking.** `xa_for_each()` visits present entries in index order and
  skips the empty ranges.
- **Marks.** Three bits per entry, `XA_MARK_0` to `_2`, kept up the tree
  so `xa_for_each_marked()` finds the marked entries without visiting
  the rest. The page cache marks dirty and writeback pages this way.
- **Erasing.** `xa_erase()` is storing NULL, and returns the old entry.
- **Allocating indices.** An xarray defined with `DEFINE_XARRAY_ALLOC()`
  has `xa_alloc()`: store at the lowest free index within a limit and
  return the index. This is the "ID to object" map drivers used IDR for.

## output

```
stored:            [0]=A [7]=B [1000]=C [1099511627776]=D
insert at 7: -16
0, 1000 marked:    [0]=A* [7]=Z [1000]=C* [1099511627776]=D
0 erased:          [7]=Z [1000]=C* [1099511627776]=D
1 erased, xa_alloc d: id 1
allocated:         [0]=a [1]=d [2]=c
```

## where the kernel uses it

- The page cache: each file's pages by page index.
- IDR and IDA are built on it.
- Driver tables of objects by ID, such as DRM handles and InfiniBand
  objects.

## older ways of doing the same

- The radix tree, `radix_tree_insert()` and `radix_tree_lookup()`, is the
  same structure under an older API; it is now a wrapper on the xarray.
- IDR, `idr_alloc()` and `idr_find()`, is `xa_alloc()` and `xa_load()`.
  New code uses the xarray directly.

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
insmod xarray.ko && dmesg | tail -n 20
rmmod xarray
```

The log ends with `xarray check passed`.

---

### ¶ The end
