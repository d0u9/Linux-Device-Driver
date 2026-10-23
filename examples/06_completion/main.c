// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/completion.h>

#include "main.h"

static dev_t completion_devno;
static struct completion_dev completion_dev;

static int completion_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __func__);

	filp->private_data = container_of(inode->i_cdev,
					  struct completion_dev, cdev);

	return 0;
}

static ssize_t completion_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *pos)
{
	struct completion_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __func__);

	pr_debug("process %d(%s) is going to sleep\n",
		 current->pid, current->comm);
	/* Sleep interruptibly so the reader can be stopped with Ctrl-C. */
	if (wait_for_completion_interruptible(&dev->completion))
		return -ERESTARTSYS;
	pr_debug("awoken %d(%s)\n", current->pid, current->comm);

	return 0;
}

static ssize_t completion_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *pos)
{
	struct completion_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __func__);

	pr_debug("process %d(%s) awakening the readers...\n",
		 current->pid, current->comm);
	complete(&dev->completion);

	return count;
}

static const struct file_operations completion_fops = {
	.owner = THIS_MODULE,
	.open  = completion_open,
	.read  = completion_read,
	.write = completion_write,
};

static int __init m_init(void)
{
	int err;

	pr_info("loaded\n");

	init_completion(&completion_dev.completion);

	err = alloc_chrdev_region(&completion_devno, 0, 1, MODULE_NAME);
	if (err < 0) {
		pr_err("can't get major number: %d\n", err);
		return err;
	}

	cdev_init(&completion_dev.cdev, &completion_fops);
	completion_dev.cdev.owner = THIS_MODULE;

	err = cdev_add(&completion_dev.cdev, completion_devno, 1);
	if (err) {
		pr_err("adding completion device failed: %d\n", err);
		unregister_chrdev_region(completion_devno, 1);
		return err;
	}

	return 0;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	cdev_del(&completion_dev.cdev);
	unregister_chrdev_region(completion_devno, 1);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Example of Kernel's completion mechanism");
