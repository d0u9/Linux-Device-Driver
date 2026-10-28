// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/xarray.h>
#include <linux/slab.h>

struct item {
	char val;
};

/* a mark is a tag bit kept per entry; here, items "in use" */
#define MARK_BUSY	XA_MARK_1

static void dump(struct xarray *xa, const char *title)
{
	struct item *it;
	unsigned long index;
	char line[128];
	int len = 0;

	/* only present entries are visited, however sparse the indices */
	xa_for_each(xa, index, it)
		len += scnprintf(line + len, sizeof(line) - len, " [%lu]=%c%s",
				 index, it->val,
				 xa_get_mark(xa, index, MARK_BUSY) ? "*" : "");
	pr_info("%-18s%s\n", title, len ? line : " (empty)");
}

static int store(struct xarray *xa, unsigned long index, char val)
{
	struct item *it = kzalloc_obj(*it);
	void *old;

	if (!it)
		return -ENOMEM;
	it->val = val;
	/* xa_store() returns the old entry, or an error entry */
	old = xa_store(xa, index, it, GFP_KERNEL);
	if (xa_is_err(old)) {
		kfree(it);
		return xa_err(old);
	}
	/* No concurrent readers here; otherwise protect object lifetime too. */
	kfree(old);
	return 0;
}

static void destroy(struct xarray *xa)
{
	struct item *it;
	unsigned long index;

	/* xa_destroy() frees internal nodes, not the objects we stored. */
	xa_for_each(xa, index, it)
		kfree(it);
	xa_destroy(xa);
}

/*
 * An array of pointers indexed by unsigned long, as sparse as it likes:
 * only the parts in use are allocated. Every entry starts as NULL, and
 * storing NULL is erasing. It has its own spinlock, taken by the
 * modifying xa_*() calls; readers take only the RCU read lock.
 */
static int __init xarray_demo(void)
{
	DEFINE_XARRAY(map);
	/* The allocating variant also tracks free indices, like IDR. */
	DEFINE_XARRAY_ALLOC(objs);
	static const unsigned long idx[] = { 0, 7, 1000, 1UL << 40 };
	struct item *it, *taken;
	unsigned long index;
	u32 id;
	int i, n, err;

	/* indices far apart cost no memory in between */
	for (i = 0; i < ARRAY_SIZE(idx); i++) {
		err = store(&map, idx[i], 'A' + i);
		if (err)
			goto out;
	}
	dump(&map, "stored:");

	it = xa_load(&map, 1000);
	pr_info("load 1000: %c, load 5: %s\n", it ? it->val : '-',
		xa_load(&map, 5) ? "found" : "NULL");

	/* storing again replaces; xa_insert() would refuse with -EBUSY */
	err = store(&map, 7, 'Z');
	if (err)
		goto out;
	err = xa_insert(&map, 7, NULL, GFP_KERNEL);
	pr_info("insert at 7: %d\n", err);
	if (err != -EBUSY) {
		err = err ? err : -EINVAL;
		goto out;
	}
	dump(&map, "7 replaced:");

	/* marks: set on some entries, then walk only those */
	xa_set_mark(&map, 0, MARK_BUSY);
	xa_set_mark(&map, 1000, MARK_BUSY);
	dump(&map, "0, 1000 marked:");
	n = 0;
	xa_for_each_marked(&map, index, it, MARK_BUSY)
		n++;
	pr_info("marked entries: %d\n", n);

	/* erasing returns the entry; it is the caller's to free */
	kfree(xa_erase(&map, 0));
	dump(&map, "0 erased:");

	/* xa_alloc() stores at the lowest free index and returns it in @id */
	for (i = 0; i < 3; i++) {
		taken = kzalloc_obj(*taken);
		if (!taken) {
			err = -ENOMEM;
			goto out;
		}
		taken->val = 'a' + i;
		err = xa_alloc(&objs, &id, taken, xa_limit_32b, GFP_KERNEL);
		if (err) {
			kfree(taken);
			goto out;
		}
		pr_info("xa_alloc %c: id %u\n", taken->val, id);
	}
	kfree(xa_erase(&objs, 1));
	taken = kzalloc_obj(*taken);
	if (!taken) {
		err = -ENOMEM;
		goto out;
	}
	taken->val = 'd';
	err = xa_alloc(&objs, &id, taken, xa_limit_32b, GFP_KERNEL);
	if (err) {
		kfree(taken);
		goto out;
	}
	pr_info("1 erased, xa_alloc %c: id %u\n", taken->val, id);
	dump(&objs, "allocated:");

	it = xa_load(&map, 7);
	if (xa_load(&map, 0) || !it || it->val != 'Z' || n != 2 ||
	    !xa_load(&map, 1UL << 40) || id != 1) {
		pr_err("xarray check failed\n");
		err = -EINVAL;
		goto out;
	}
	pr_info("xarray check passed\n");

out:
	destroy(&map);
	destroy(&objs);
	return err;
}

static int __init mod_init(void)
{
	return xarray_demo();
}

/* All resources have been released before xarray_demo() returns. */
static void __exit mod_exit(void)
{
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Demonstrating the kernel's xarray");
