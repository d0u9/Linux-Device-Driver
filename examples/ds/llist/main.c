// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/llist.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>
#include <linux/slab.h>

#define PRODUCERS	2
#define PER_PRODUCER	10000

struct item {
	int key;
	struct llist_node node;
};

/*
 * A singly linked list with one pointer for a head, changed only with
 * cmpxchg(): any number of producers add with llist_add(), with no lock
 * including from interrupts (NMIs require
 * CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG), and a consumer
 * takes the whole list at once with llist_del_all(). These globals are
 * shared by the producer threads and the consumer.
 */
static LLIST_HEAD(inbox);

static atomic_t producers_left = ATOMIC_INIT(PRODUCERS);
static atomic_t was_empty = ATOMIC_INIT(0);

static void dump(struct llist_node *first, const char *title)
{
	struct item *it;
	char line[128];
	int len = 0;

	llist_for_each_entry(it, first, node)
		len += scnprintf(line + len, sizeof(line) - len, " %d", it->key);
	pr_info("%-26s%s\n", title, len ? line : " (empty)");
}

static int free_batch(struct llist_node *first)
{
	struct item *it, *tmp;
	int n = 0;

	llist_for_each_entry_safe(it, tmp, first, node) {
		kfree(it);
		n++;
	}
	return n;
}

static int producer(void *data)
{
	struct item *it;
	int i, err = 0;

	/* GFP_KERNEL may sleep: this producer runs in process context. */
	for (i = 0; i < PER_PRODUCER; i++) {
		it = kzalloc_obj(*it);
		if (!it) {
			err = -ENOMEM;
			break;
		}
		it->key = i;
		/* true if the list was empty: the moment to wake a consumer */
		if (llist_add(&it->node, &inbox))
			atomic_inc(&was_empty);
	}
	atomic_dec(&producers_left);
	return err;
}

static int __init llist_demo(void)
{
	struct task_struct *tasks[PRODUCERS];
	struct llist_node *batch;
	struct item *it;
	int i, ret, err = 0, got = 0, batches = 0;
	bool done;

	/* llist_add() pushes at the front: taking all gives newest first */
	for (i = 1; i <= 5; i++) {
		it = kzalloc_obj(*it);
		if (!it)
			goto nomem;
		it->key = i;
		llist_add(&it->node, &inbox);
	}
	batch = llist_del_all(&inbox);
	dump(batch, "added 1..5, taken:");
	batch = llist_reverse_order(batch);
	dump(batch, "reversed:");
	dump(inbox.first, "inbox after taking:");
	free_batch(batch);

	/* now for real: producers on two threads, the consumer here */
	for (i = 0; i < PRODUCERS; i++) {
		tasks[i] = kthread_create(producer, NULL, "llist-prod%d", i);
		if (IS_ERR(tasks[i])) {
			err = PTR_ERR(tasks[i]);
			atomic_dec(&producers_left);
			continue;
		}
		/*
		 * The thread may be done before kthread_stop() is called: hold
		 * its task_struct from before it starts.
		 */
		get_task_struct(tasks[i]);
		wake_up_process(tasks[i]);
	}

	/*
	 * Check completion before detaching the final batch: once all
	 * producers are done, no new items can appear after that drain.
	 * An empty inbox alone is not enough while producers still run.
	 */
	do {
		done = !atomic_read(&producers_left);
		batch = llist_del_all(&inbox);
		if (batch) {
			got += free_batch(batch);
			batches++;
		}
		cond_resched();
	} while (!done || !llist_empty(&inbox));

	for (i = 0; i < PRODUCERS; i++) {
		if (IS_ERR(tasks[i]))
			continue;
		ret = kthread_stop(tasks[i]);
		if (ret && !err)
			err = ret;
		put_task_struct(tasks[i]);
	}
	if (err)
		return err;

	pr_info("%d items from %d producers in %d batches, list found empty %d times\n",
		got, PRODUCERS, batches, atomic_read(&was_empty));

	if (got != PRODUCERS * PER_PRODUCER) {
		pr_err("llist check failed\n");
		return -EINVAL;
	}
	pr_info("llist check passed\n");
	return 0;

nomem:
	free_batch(llist_del_all(&inbox));
	return -ENOMEM;
}

static int __init mod_init(void)
{
	return llist_demo();
}

/* All resources have been released before llist_demo() returns. */
static void __exit mod_exit(void)
{
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Demonstrating the kernel's lock-less list");
