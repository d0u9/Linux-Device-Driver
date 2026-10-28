# hlist

The kernel's list for hash buckets, `hlist` in `<linux/list.h>`, used
through the hash table of `<linux/hashtable.h>`. On load the module puts
16 items in an 8-bucket table, prints every bucket, searches, deletes,
checks the table and frees it.

## hlist and list

| | `list_head` | `hlist_head` / `hlist_node` |
|---|---|---|
| head | `next`, `prev`: 16 bytes | `first`: 8 bytes |
| node | `next`, `prev` | `next`, `pprev` |
| shape | a ring through the head | a line ending in NULL |
| add | front or back, O(1) | front, O(1); no way to the back |
| delete | O(1) | O(1) |
| use | lists walked in order | hash buckets |

A hash table is an array of heads, most of them empty or short. Halving
the head halves the table: 1024 buckets are 8 KB instead of 16 KB, and
the kernel has tables of millions of buckets. What is given up is the
pointer to the tail, which a bucket does not need.

## pprev

A node's `pprev` is not the previous node but the pointer that points to
this one: the head's `first` for the first node, the previous node's
`next` otherwise. Deleting is then `*pprev = next` in every case, O(1),
without knowing the bucket and without testing whether the node is
first. A deleted node with `pprev` NULL is on no list
(`hlist_unhashed()`, `hash_hashed()`).

## what each part shows

- **The table.** `DEFINE_HASHTABLE(table, 3)` is 2^3 empty heads.
- **Adding.** `hash_add()` hashes the key with `hash_min()` and calls
  `hlist_add_head()`, so the newest item of a bucket comes first.
- **Searching.** `hash_for_each_possible()` walks only the bucket of the
  key, and the key must still be compared: other keys share the bucket.
- **Deleting.** `hash_del()` is `hlist_del_init()`.
- **Freeing.** `hash_for_each_safe()` visits every bucket.

## output

```
sizeof list_head 16, hlist_head 8: a 1024-bucket table is 16384 or 8192 bytes
== keys 1..16 ==
  [0] -> 16:P -> 8:H -> NULL
  [1] -> 11:K -> 3:C -> NULL
  ...
  [7] -> 13:M -> 5:E -> NULL
5's pprev is the previous node's next
5 deleted, hashed: no
```

## where the kernel uses it

- PIDs, inodes and dentries are found through hlist hash tables.
- The network stack's socket lookup tables.
- `hlist_nulls` ends each bucket with a marker instead of NULL, for
  lookups under RCU while items move between buckets;
  `hlist_bl` keeps a bit lock in the head's lowest bit.
- Tables that must grow use `rhashtable`, which resizes under RCU.

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
insmod hlist.ko && dmesg | tail -n 25
rmmod hlist
```

The log ends with `hash table check passed`.

---

### ¶ The end
