// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/idr.h>

static void dump(struct ida *ids, const char *title)
{
	char line[128];
	int len = 0;
	int id = 0;

	/* ida_find_first_range(): the first ID in use from @id on */
	while ((id = ida_find_first_range(ids, id, ~0)) >= 0)
		len += scnprintf(line + len, sizeof(line) - len, " foo%d", id++);
	pr_info("%-24s%s\n", title, len ? line : " (none)");
}

/*
 * An ID allocator: hands out the smallest unused integer, and takes it
 * back. It stores nothing with the ID; a driver uses one to number its
 * devices, foo0, foo1, ... IDs range from 0 to INT_MAX. Allocation
 * returns -ENOMEM on allocation failure or -ENOSPC when no ID fits.
 * Individual calls lock internally; a sequence of calls is not atomic.
 */
static int __init ida_demo(void)
{
	DEFINE_IDA(dev_ida);
	int ids[4], i, id, err = 0;

	for (i = 0; i < 4; i++) {
		ids[i] = ida_alloc(&dev_ida, GFP_KERNEL);
		if (ids[i] < 0) {
			err = ids[i];
			goto out;
		}
	}
	dump(&dev_ida, "4 devices:");

	/* a freed ID is the first to be handed out again */
	ida_free(&dev_ida, ids[1]);
	dump(&dev_ida, "foo1 removed:");
	id = ida_alloc(&dev_ida, GFP_KERNEL);
	dump(&dev_ida, "one more:");
	if (id != 1) {
		err = id < 0 ? id : -EINVAL;
		goto out;
	}

	/* Both range bounds are inclusive: 10 and 11, then -ENOSPC. */
	for (i = 0; i < 3; i++) {
		id = ida_alloc_range(&dev_ida, 10, 11, GFP_KERNEL);
		pr_info("alloc in 10..11: %d\n", id);
	}
	dump(&dev_ida, "with the range:");
	if (id != -ENOSPC || !ida_exists(&dev_ida, 11)) {
		err = -EINVAL;
		goto out;
	}
	pr_info("ida check passed\n");

out:
	if (err)
		pr_err("ida check failed\n");
	/* frees every ID at once; ida_free() each one also works */
	ida_destroy(&dev_ida);
	pr_info("destroyed, empty: %s\n", ida_is_empty(&dev_ida) ? "yes" : "no");
	return err;
}

static int __init mod_init(void)
{
	return ida_demo();
}

/* All resources have been released before ida_demo() returns. */
static void __exit mod_exit(void)
{
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Demonstrating the kernel's IDA");
