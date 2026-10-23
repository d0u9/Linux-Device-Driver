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

static dev_t ioctl_devno;
static struct ioctl_dev *ioctl_dev;

/*
 * No .compat_ioctl: IOCTL_HOWMANY passes an integer and struct ioctl_msg_arg
 * holds a pointer, so 32-bit callers on a 64-bit kernel would need a real
 * compat handler. See the README.
 */
static const struct file_operations fops = {
	.owner          = THIS_MODULE,
	.open           = ioctl_open,
	.read           = ioctl_read,
	.unlocked_ioctl = ioctl_ioctl,
};

static int __init m_init(void)
{
	int err;

	pr_info("loaded\n");

	ioctl_dev = kzalloc_obj(*ioctl_dev);
	if (!ioctl_dev)
		return -ENOMEM;

	mutex_init(&ioctl_dev->lock);
	ioctl_dev_set_default(ioctl_dev);

	err = alloc_chrdev_region(&ioctl_devno, 0, IOCTL_DEV_NR, MODULE_NAME);
	if (err < 0) {
		pr_err("can't get major number: %d\n", err);
		goto err_free;
	}

	cdev_init(&ioctl_dev->cdev, &fops);
	ioctl_dev->cdev.owner = THIS_MODULE;

	err = cdev_add(&ioctl_dev->cdev, ioctl_devno, IOCTL_DEV_NR);
	if (err) {
		pr_err("adding ioctl device failed: %d\n", err);
		goto err_unregister;
	}

	return 0;

err_unregister:
	unregister_chrdev_region(ioctl_devno, IOCTL_DEV_NR);
err_free:
	kfree(ioctl_dev);
	return err;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	cdev_del(&ioctl_dev->cdev);
	unregister_chrdev_region(ioctl_devno, IOCTL_DEV_NR);
	kfree(ioctl_dev);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("An ioctl() example");
