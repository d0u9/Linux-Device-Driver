# list

The kernel's doubly linked list, `<linux/list.h>`. On load the module
builds a stack and a queue, joins, sorts, walks, deletes and rotates them,
prints the list after each step, checks it and frees it. `insmod` fails
if a check does not hold.

## the list lives inside the items

`struct list_head` is two pointers, `next` and `prev`, embedded in the
user's structure; `list_entry()` (`container_of()`) gets back from it.
So one list code serves every type, and an item needs no separate
allocation to be on a list.

The list is a ring. The head is a `list_head` of its own, not in an item:
an empty list is a head pointing to itself, and a walk stops when it
comes back to the head. With `prev` the head reaches the last item at
once, so adding at either end is O(1).

An item can be on as many lists as it has `list_head`s. Here every item
is on one list through `all`, and the odd ones on another through `odd`.

## what each part shows

- **Stack and queue.** `list_add()` adds after the head, at the front;
  `list_add_tail()` before it, at the back.
- **Walking.** `list_for_each_entry()` and `_reverse()`. The walk reads
  the next pointer after the body, so the body must not free the item.
- **Deleting while walking.** `list_for_each_entry_safe()` keeps the next
  item in a second cursor. `list_del()` unlinks; the item is then the
  user's to free, from every list it is on.
- **Joining.** `list_splice_tail_init()` moves a whole list in O(1),
  relinking only the ends, and leaves the source empty.
- **Sorting.** `list_sort()`, a stable merge sort that only relinks.
- **Rotating.** `list_rotate_left()` moves the first item to the back;
  `list_move()` and `list_move_tail()` move one item between lists.

## output

```
list_add 1..4:           4:D 3:C 2:B 1:A
list_add_tail 5..8:      5:E 6:F 7:G 8:H
stack spliced on:        5:E 6:F 7:G 8:H 4:D 3:C 2:B 1:A
sorted:                  1:A 2:B 3:C 4:D 5:E 6:F 7:G 8:H
odd list:                1:A 3:C 5:E 7:G
multiples of 3 gone:     1:A 2:B 4:D 5:E 7:G 8:H
odd list:                1:A 5:E 7:G
rotated left:            2:B 4:D 5:E 7:G 8:H 1:A
```

## where the kernel uses it

Nearly everywhere: a task's children, a device's resources, a driver's
devices, work queues, the pages on the LRU. A list is right when the
items are walked in order or taken from the ends; to find an item by key
it is O(n), and a hash table or a tree is used instead.

## shorter and older ways

- `list_for_each()` walks the bare `list_head`s, and the body calls
  `list_entry()`; the `_entry` walks do that for you.
- `list_count_nodes()` (v6.3) replaced counting by hand.
- `list_del()` poisons the pointers so a use after delete faults;
  `list_del_init()` leaves the node pointing to itself, so `list_empty()`
  on it says it is on no list.

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
insmod list.ko && dmesg | tail -n 20
rmmod list
```

The log ends with `list check passed`.

---

### ¶ The end
