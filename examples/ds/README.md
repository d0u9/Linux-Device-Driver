# ds

The kernel's own data structures, each in a module that runs a demo on
load and prints the structure after each step.

Each `main.c` keeps its helper functions first, followed by a function
named after the data structure, such as `rbtree()`. This function runs
the operations, checks the result and releases its resources on both
success and failure. At the bottom, `mod_init()` calls it and returns
its status. `mod_exit()` is empty because no resources remain to release.

| example | structure | find by key | in order | typical use |
|---------|-----------|-------------|----------|-------------|
| `list` | doubly linked ring | O(n) | insertion order | queues, sets of objects |
| `hlist` | singly linked buckets | O(1) average | none | hash tables |
| `rbtree` | balanced binary tree | O(log n) | key order | ranges, ordered lookups |
| `kfifo` | ring buffer | no | first in, first out | producer and consumer |
| `xarray` | sparse array of pointers | O(log n) by index | index order | objects by ID, page cache |
| `ida` | ID allocator | - | - | numbering devices |
| `llist` | lock-less singly linked | O(n) | newest first | queuing from any context |
| `bitmap` | array of bits | O(1) by bit | bit order | free slots, CPU masks |

In list, hlist, rbtree and llist, the link is embedded in the user's
structure and the structure is reached from it with `container_of()`;
the kernel code only links, and never allocates the items.

These examples target the workspace's x86_64 kernel. In particular, the
xarray demo uses an index of 2^40, which needs a 64-bit `unsigned long`.
The sample output assumes this environment.

Build them all from the examples directory with `make ds`.
