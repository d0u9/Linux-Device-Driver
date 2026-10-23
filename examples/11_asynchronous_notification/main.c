// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "main.h"
#include "fops.h"

static dev_t async_notify_devno;
static struct async_notify_dev *async_notify_dev;

static const struct file_operations fops = {
	.owner   = THIS_MODULE,
	.open    = async_notify_open,
	.read    = async_notify_read,
	.write   = async_notify_write,
	.fasync  = async_notify_fasync,
	.release = async_notify_release,
};

static int __init m_init(void)
{
	int err;

	pr_info("loaded\n");

	async_notify_dev = kzalloc_obj(*async_notify_dev);
	if (!async_notify_dev)
		return -ENOMEM;

	mutex_init(&async_notify_dev->mutex);
	cdev_init(&async_notify_dev->cdev, &fops);
	async_notify_dev->cdev.owner = THIS_MODULE;

	err = alloc_chrdev_region(&async_notify_devno, 0, ASYNC_NOTIFY_DEV_NR,
				  MODULE_NAME);
	if (err < 0) {
		pr_err("can't get major number: %d\n", err);
		goto err_free;
	}

	err = cdev_add(&async_notify_dev->cdev, async_notify_devno,
		       ASYNC_NOTIFY_DEV_NR);
	if (err) {
		pr_err("adding async_notify device failed: %d\n", err);
		goto err_unregister;
	}

	return 0;

err_unregister:
	unregister_chrdev_region(async_notify_devno, ASYNC_NOTIFY_DEV_NR);
err_free:
	kfree(async_notify_dev);
	return err;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	cdev_del(&async_notify_dev->cdev);
	unregister_chrdev_region(async_notify_devno, ASYNC_NOTIFY_DEV_NR);
	kfree(async_notify_dev);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Asynchronous notification for non-block IO");
