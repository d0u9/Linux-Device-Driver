// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>

#include "main.h"
#include "fops.h"

static dev_t poll_devno;
static struct poll_dev *poll_dev;

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open  = poll_open,
	.read  = poll_read,
	.write = poll_write,
	.poll  = poll_poll,
};

static void timer_fn(struct timer_list *t)
{
	struct poll_dev *dev = timer_container_of(dev, t, timer);

	pr_debug("%s() is invoked\n", __func__);

	++dev->timer_counter;

	/* make readable every timer interval */
	atomic_set(&dev->can_rd, 1);
	wake_up_interruptible(&dev->inq);

	/* make writable every two timer intervals */
	if (dev->timer_counter % 2) {
		atomic_set(&dev->can_wr, 1);
		wake_up_interruptible(&dev->outq);
	}

	mod_timer(&dev->timer, jiffies + TIMER_INTERVAL);
}

static void init_dev(struct poll_dev *dev)
{
	mutex_init(&dev->mutex);
	timer_setup(&dev->timer, timer_fn, 0);

	atomic_set(&dev->can_wr, 0);
	atomic_set(&dev->can_rd, 0);

	init_waitqueue_head(&dev->inq);
	init_waitqueue_head(&dev->outq);

	cdev_init(&dev->cdev, &fops);
	dev->cdev.owner = THIS_MODULE;

	dev->buf_len = strlen(DFT_MSG);
	memcpy(dev->buff, DFT_MSG, dev->buf_len);
}

static int __init m_init(void)
{
	int err;

	pr_info("loaded\n");

	poll_dev = kzalloc_obj(*poll_dev);
	if (!poll_dev)
		return -ENOMEM;

	init_dev(poll_dev);

	err = alloc_chrdev_region(&poll_devno, 0, POLL_DEV_NR, MODULE_NAME);
	if (err < 0) {
		pr_err("can't get major number: %d\n", err);
		goto err_free;
	}

	err = cdev_add(&poll_dev->cdev, poll_devno, POLL_DEV_NR);
	if (err) {
		pr_err("adding poll device failed: %d\n", err);
		goto err_unregister;
	}

	/* start the timer last, once everything it touches is ready */
	mod_timer(&poll_dev->timer, jiffies + TIMER_INTERVAL);

	return 0;

err_unregister:
	unregister_chrdev_region(poll_devno, POLL_DEV_NR);
err_free:
	kfree(poll_dev);
	return err;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	cdev_del(&poll_dev->cdev);
	/* unlike timer_delete_sync(), also stops timer_fn() from re-arming */
	timer_shutdown_sync(&poll_dev->timer);
	unregister_chrdev_region(poll_devno, POLL_DEV_NR);
	kfree(poll_dev);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple poll example");
