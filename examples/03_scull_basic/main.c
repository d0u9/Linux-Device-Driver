// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/list.h>

#include "main.h"
#include "fops.h"

static int scull_major;
static int scull_minor;

static struct scull_dev *scull_dev[SCULL_NR_DEVS];

static const struct file_operations scull_fops = {
	.owner   = THIS_MODULE,
	.open    = scull_open,
	.read    = scull_read,
	.write   = scull_write,
	.release = scull_release,
};

static void __init init_scull_dev(struct scull_dev *dev)
{
	dev->block_counter = 0;

	INIT_LIST_HEAD(&dev->block_list);
	mutex_init(&dev->mutex);

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
}

static void scull_destroy_devs(int count)
{
	/* Tear down in the reverse order of creation. */
	for (int i = count - 1; i >= 0; --i) {
		cdev_del(&scull_dev[i]->cdev);
		scull_trim(scull_dev[i]);
		kfree(scull_dev[i]);
		scull_dev[i] = NULL;
	}
}

static int __init m_init(void)
{
	int err;
	int i;
	dev_t devno;

	pr_info("loaded\n");

	err = alloc_chrdev_region(&devno, scull_minor, SCULL_NR_DEVS,
				  MODULE_NAME);
	if (err < 0) {
		pr_err("can't get major number: %d\n", err);
		return err;
	}
	scull_major = MAJOR(devno);

	for (i = 0; i < SCULL_NR_DEVS; ++i) {
		scull_dev[i] = kzalloc_obj(*scull_dev[i]);
		if (!scull_dev[i]) {
			err = -ENOMEM;
			goto err_destroy;
		}

		init_scull_dev(scull_dev[i]);

		/*
		 * cdev_add() makes this char device usable from userspace.
		 * Do not call it until the device is ready to be used.
		 */
		devno = MKDEV(scull_major, scull_minor + i);
		err = cdev_add(&scull_dev[i]->cdev, devno, 1);
		if (err) {
			pr_err("adding %s%d failed: %d\n", MODULE_NAME, i, err);
			kfree(scull_dev[i]);
			scull_dev[i] = NULL;
			goto err_destroy;
		}
	}

	return 0;

err_destroy:
	scull_destroy_devs(i);
	unregister_chrdev_region(MKDEV(scull_major, scull_minor),
				 SCULL_NR_DEVS);
	return err;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	scull_destroy_devs(SCULL_NR_DEVS);
	unregister_chrdev_region(MKDEV(scull_major, scull_minor),
				 SCULL_NR_DEVS);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Memory-backed char device demonstrating basic file operations");
