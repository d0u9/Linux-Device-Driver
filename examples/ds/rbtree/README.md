# rbtree

The kernel's red-black tree, `<linux/rbtree.h>`, used as a map from int
keys to letters (1 is `A`, 2 is `B`, ...). On load the module inserts,
searches, replaces and erases, prints the tree after each step, checks it
and frees it. `insmod` fails if a check does not hold.

## the tree knows nothing of keys

`struct rb_node` is embedded in the user's structure, like `list_head`,
and `rb_entry()` (`container_of()`) gets back from it. The tree code only
links nodes and keeps the tree balanced by recoloring and rotating; the
order comes from the user, as comparison functions:

- `rb_data_cmp()` compares two nodes, for inserting;
- `rb_data_key_cmp()` compares a bare key with a node, for searching.

Both return less than, equal to or greater than 0, like `strcmp()`.

## what each part shows

- **Inserting.** `rb_find_add()` walks down to where the key belongs and
  links the node there, or returns the node that already has the key.
  `rb_add()` inserts without that check, for trees that keep duplicate
  keys.
- **Searching.** `rb_find()` walks down with the key compare.
- **Walking in order.** `rb_first()` is the smallest key, `rb_next()` the
  next larger one; `rb_last()` and `rb_prev()` go the other way. Each step
  is O(log n) at worst, O(1) on average.
- **Replacing.** `rb_replace_node()` puts a new node in the place of an
  old one with the same key, without rebalancing.
- **Erasing.** `rb_erase()` unlinks a node and rebalances; the node is
  then the user's to free.
- **Freeing everything.** `rbtree_postorder_for_each_entry_safe()` visits
  children before parents, so each node can be freed as it is visited.
  It must not be mixed with `rb_erase()`, which moves nodes about.

## the tree as printed

```
== keys 7..11 added ==
  in order: 1:A 2:B 3:C 4:D 5:E 6:F 7:G 8:H 9:I 10:J 11:K
                          11:K (red)
                    10:J
                          9:I (red)
              8:H (red)
                    7:G
        6:F
              5:E
  4:D
              3:C
        2:B
              1:A
```

The tree is drawn on its side: the root on the left, larger keys above.
Inserting keys in order still gives a tree five levels deep, not a list
of 11: the rotations keep every path from the root within twice the
shortest.
After key 2 is erased, the root moves to 6. Reading a node's color needs
`<linux/rbtree_augmented.h>`; it is used here only for drawing.

## where the kernel uses it

- The scheduler's run queue, ordered by virtual deadline.
- hrtimers, ordered by expiry, through `timerqueue`.
- epoll's set of watched files, ordered by file and fd.
- ext4's extent status tree, and many file systems' extent maps.

The process address space (VMAs) used an rbtree until v6.1; it is a
maple tree now. Maps from integer indices often use an xarray instead.

## older ways of doing the same

- Before v5.12 there were no `rb_add()`, `rb_find_add()` or `rb_find()`:
  every user wrote the walk down the tree by hand and finished an insert
  with `rb_link_node()` and `rb_insert_color()`. Both hand-written
  versions are kept, commented out, in `main.c`.
- `struct rb_root_cached` with `rb_first_cached()` also remembers the
  leftmost node, so the smallest key is O(1); the scheduler and hrtimers
  use it, with `rb_add_cached()` and `rb_erase_cached()`.

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
insmod rbtree.ko && dmesg | tail -n 50
rmmod rbtree
```

The log ends with `tree check passed`.

---

### ¶ The end
