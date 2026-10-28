// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/slab.h>

/*
 * Key 1 is 'A', key 2 is 'B', ... Every item is on the list of all items
 * through @all, and the odd ones are on a second list through @odd: one
 * list_head per list the item can be on.
 *
 * These ordinary list operations do not lock. Concurrent readers and
 * writers need external synchronization; _safe iteration only permits
 * deleting the current item in this walk, not concurrent modification.
 */
struct item {
	int key;
	char val;
	struct list_head all;
	struct list_head odd;
};

static struct item *new_item(int key)
{
	struct item *it = kzalloc_obj(*it);

	if (!it)
		return NULL;
	it->key = key;
	it->val = 'A' + key - 1;
	/* not on the odd list yet: list_empty(&it->odd) says so */
	INIT_LIST_HEAD(&it->odd);
	return it;
}

/*
 * The list is a ring: the head is a list_head like the others, but not in
 * an item, and the walk stops when it comes back to it.
 * list_for_each_entry() is list_for_each() with list_entry() (that is,
 * container_of()) on each node.
 */
static void dump_all(struct list_head *head, const char *title)
{
	struct item *it;
	char line[128];
	int len = 0;

	list_for_each_entry(it, head, all)
		len += scnprintf(line + len, sizeof(line) - len, " %d:%c",
				 it->key, it->val);
	pr_info("%-24s%s\n", title, len ? line : " (empty)");
}

/* the same items, walked through their other list_head */
static void dump_odd(struct list_head *head, const char *title)
{
	struct item *it;
	char line[128];
	int len = 0;

	list_for_each_entry(it, head, odd)
		len += scnprintf(line + len, sizeof(line) - len, " %d:%c",
				 it->key, it->val);
	pr_info("%-24s%s\n", title, len ? line : " (empty)");
}

/* backwards: the ring is doubly linked, so prev is as cheap as next */
static void dump_all_reverse(struct list_head *head, const char *title)
{
	struct item *it;
	char line[128];
	int len = 0;

	list_for_each_entry_reverse(it, head, all)
		len += scnprintf(line + len, sizeof(line) - len, " %d:%c",
				 it->key, it->val);
	pr_info("%-24s%s\n", title, len ? line : " (empty)");
}

static int item_cmp(void *priv, const struct list_head *a,
		    const struct list_head *b)
{
	int ka = list_entry(a, struct item, all)->key;
	int kb = list_entry(b, struct item, all)->key;

	return (ka > kb) - (ka < kb);
}

/* items are freed with list_del() first, so @odd is never left dangling */
static void free_all(struct list_head *head)
{
	struct item *it, *tmp;

	list_for_each_entry_safe(it, tmp, head, all) {
		list_del(&it->all);
		list_del(&it->odd);
		kfree(it);
	}
}

static int __init list_demo(void)
{
	LIST_HEAD(stack);
	LIST_HEAD(queue);
	LIST_HEAD(odds);
	struct item *it, *tmp;
	int key, err = 0;

	pr_info("empty head points to itself: %s\n",
		stack.next == &stack && stack.prev == &stack ? "yes" : "no");

	/* list_add() puts at the front: the list is a stack */
	for (key = 1; key <= 4; key++) {
		it = new_item(key);
		if (!it) {
			err = -ENOMEM;
			goto out;
		}
		list_add(&it->all, &stack);
	}
	dump_all(&stack, "list_add 1..4:");

	/* list_add_tail() puts at the back, before the head: a queue */
	for (key = 5; key <= 8; key++) {
		it = new_item(key);
		if (!it) {
			err = -ENOMEM;
			goto out;
		}
		list_add_tail(&it->all, &queue);
	}
	dump_all(&queue, "list_add_tail 5..8:");

	/* Both lists are nonempty after the successful allocation loops. */
	pr_info("first of the stack: %d, of the queue: %d\n",
		list_first_entry(&stack, struct item, all)->key,
		list_first_entry(&queue, struct item, all)->key);

	/*
	 * Joining two lists is O(1): only the ends are relinked. The _init
	 * variant leaves @stack empty rather than pointing into @queue.
	 */
	list_splice_tail_init(&stack, &queue);
	dump_all(&queue, "stack spliced on:");
	dump_all(&stack, "stack after:");

	/* a merge sort, stable, that only relinks */
	list_sort(NULL, &queue, item_cmp);
	dump_all(&queue, "sorted:");

	/* the odd items go on a second list too, still in one piece each */
	list_for_each_entry(it, &queue, all)
		if (it->key % 2)
			list_add_tail(&it->odd, &odds);
	dump_odd(&odds, "odd list:");

	/*
	 * Deleting while walking needs the _safe walk, which reads the next
	 * node before the body frees this one. An item leaves every list it
	 * is on before it is freed.
	 */
	list_for_each_entry_safe(it, tmp, &queue, all) {
		if (it->key % 3)
			continue;
		list_del(&it->all);
		if (!list_empty(&it->odd))
			list_del(&it->odd);
		kfree(it);
	}
	dump_all(&queue, "multiples of 3 gone:");
	dump_odd(&odds, "odd list:");

	/* moving the first to the back is just relinking, no free and alloc */
	list_rotate_left(&queue);
	dump_all(&queue, "rotated left:");
	dump_all_reverse(&queue, "backwards:");

	/* 1..8 less 3 and 6, rotated once; odd ones 1, 5 and 7 */
	if (list_count_nodes(&queue) != 6 || list_count_nodes(&odds) != 3 ||
	    list_first_entry(&queue, struct item, all)->key != 2 ||
	    list_last_entry(&queue, struct item, all)->key != 1) {
		pr_err("list check failed\n");
		err = -EINVAL;
		goto out;
	}
	pr_info("list check passed\n");

out:
	free_all(&stack);
	free_all(&queue);
	return err;
}

static int __init mod_init(void)
{
	return list_demo();
}

/* All resources have been released before list_demo() returns. */
static void __exit mod_exit(void)
{
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Demonstrating the kernel's doubly linked list");
