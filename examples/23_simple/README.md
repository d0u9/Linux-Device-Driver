# simple

`mmap()` in a driver, after LDD3's simple: two char devices that map a
buffer of eight pages into user space in the two usual ways. No hardware is
needed.

| device | how pages get mapped | buffer |
|--------|----------------------|--------|
| `/dev/simple0` | all at once, in `mmap()`, with `remap_pfn_range()` | `alloc_pages()`, physically contiguous |
| `/dev/simple1` | one by one, on first access, from `.fault` | `vmalloc()`, pages anywhere |

Page *i* of each buffer is filled with `0xdead0000 + i`, so a mapping shows
which pages it got.

## what each part shows

- **The offset.** The last argument of `mmap()` is a byte offset into the
  device; the kernel passes it in pages as `vma->vm_pgoff`. Both devices
  map from that page on and refuse, with `-EINVAL`, a range that runs past
  the buffer.
- **`remap_pfn_range()`.** It fills in the page tables for the whole range
  at once, from a page frame number (PFN). One call covers the range only
  if the memory is physically contiguous, hence `alloc_pages()`.
- **`.fault`.** `mmap()` only sets `vm_ops`. The first access to each page
  faults; `simple_vma_fault()` finds the page for `vmf->pgoff` with
  `vmalloc_to_page()`, takes a reference with `get_page()` and hands it
  back in `vmf->page`. The kernel maps it and drops the reference when the
  page is unmapped. `dmesg` shows one fault per page touched.
- **`vm_operations_struct`.** `.open` and `.close` run when a mapping is
  copied (`fork()`, or split by a partial `munmap()` or `mprotect()`) and
  when one goes away. `mmap()` does not call `.open` for the mapping it
  creates, so the driver does.
- **Lifetime.** A mapping holds a reference to the file, and the file one
  to the module, so the module cannot be unloaded while anything is still
  mapped.

## newer ways of doing the same

The comments in `main.c` name them:

- For RAM, `vm_map_pages()` or `vm_insert_page()` map pages as
  `remap_pfn_range()` does, but keep their reference counts.
  `remap_pfn_range()` is meant for memory the kernel does not manage as
  pages; device registers are mapped with `io_remap_pfn_range()` and
  `pgprot_noncached()`.

## older ways of doing the same

- `.fault` was `.nopage` in LDD3, returning the page instead of a
  `vm_fault_t`.
- `vma->vm_flags` was changed directly; now `vm_flags_set()` and
  `vm_flags_clear()` do it.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

Build the test program, statically linked so that it runs in the guest:

```bash
make -C test
```

## Usage

Copy **load_module.sh**, **simple.ko** and **test/mmap_test.out** to the
target machine, then run:

```bash
sh load_module.sh
```

## test the module

```bash
./mmap_test.out /dev/simple0
./mmap_test.out /dev/simple1
dmesg
```

It maps pages 2 to 5, maps them a second time and writes through one
mapping, and tries pages 6 to 9:

```
page 2: 0xdead0002
page 3: 0xdead0003
page 4: 0xdead0004
page 5: 0xdead0005
shared: wrote 0x12345678, other mapping reads 0x12345678
past the end: Invalid argument
```

For `simple1`, `dmesg` shows a fault for each of pages 2 to 5 of the first
mapping but only for page 2 of the second, the only one the test touches:

```
vma open:  0x7f2a325c3000-0x7f2a325c7000, page offset 2
fault at 0x7f2a325c3000: page 2
fault at 0x7f2a325c4000: page 3
fault at 0x7f2a325c5000: page 4
fault at 0x7f2a325c6000: page 5
vma open:  0x7f2a325bf000-0x7f2a325c3000, page offset 2
fault at 0x7f2a325bf000: page 2
vma close: 0x7f2a325c3000-0x7f2a325c7000, page offset 2
vma close: 0x7f2a325bf000-0x7f2a325c3000, page offset 2
```

---

### ¶ The end
