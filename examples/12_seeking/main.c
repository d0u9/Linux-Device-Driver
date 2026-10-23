// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include "main.h"
#include "fops.h"

static dev_t seeking_devno;
static struct seeking_dev *seeking_dev;

static const struct file_operations fops = {
	.owner  = THIS_MODULE,
	.open   = seeking_open,
	.read   = seeking_read,
	.llseek = seeking_llseek,
};

static int __init m_init(void)
{
	int err;

	pr_info("loaded\n");

	seeking_dev = kzalloc_obj(*seeking_dev);
	if (!seeking_dev)
		return -ENOMEM;

	/* initialize the internal buffer with the DEC-HEX table */
	memcpy(seeking_dev->buff, HEX_DICT, HEX_DICT_LEN);
	mutex_init(&seeking_dev->mutex);
	cdev_init(&seeking_dev->cdev, &fops);
	seeking_dev->cdev.owner = THIS_MODULE;

	err = alloc_chrdev_region(&seeking_devno, 0, SEEKING_DEV_NR,
				  MODULE_NAME);
	if (err < 0) {
		pr_err("can't get major number: %d\n", err);
		goto err_free;
	}

	err = cdev_add(&seeking_dev->cdev, seeking_devno, SEEKING_DEV_NR);
	if (err) {
		pr_err("adding %s failed: %d\n", MODULE_NAME, err);
		goto err_unregister;
	}

	return 0;

err_unregister:
	unregister_chrdev_region(seeking_devno, SEEKING_DEV_NR);
err_free:
	kfree(seeking_dev);
	return err;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	cdev_del(&seeking_dev->cdev);
	unregister_chrdev_region(seeking_devno, SEEKING_DEV_NR);
	kfree(seeking_dev);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Illustrate how to make a file seekable.");
