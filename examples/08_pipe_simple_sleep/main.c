// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/wait.h>

#include "main.h"
#include "fops.h"

static dev_t pipe_devno;
static struct pipe_dev *pipe_dev[PIPE_DEV_NR];

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open  = pipe_open,
	.read  = pipe_read,
	.write = pipe_write,
};

static void __init init_pipe_dev(struct pipe_dev *dev)
{
	mutex_init(&dev->mutex);

	cdev_init(&dev->cdev, &fops);
	dev->cdev.owner = THIS_MODULE;

	init_waitqueue_head(&dev->rd_queue);
	init_waitqueue_head(&dev->wr_queue);
}

static void pipe_destroy_devs(int count)
{
	/* Tear down in the reverse order of creation. */
	for (int i = count - 1; i >= 0; --i) {
		cdev_del(&pipe_dev[i]->cdev);
		kfree(pipe_dev[i]);
		pipe_dev[i] = NULL;
	}
}

static int __init m_init(void)
{
	int err;
	int i;

	pr_info("loaded\n");

	err = alloc_chrdev_region(&pipe_devno, 0, PIPE_DEV_NR, MODULE_NAME);
	if (err < 0) {
		pr_err("can't get major number: %d\n", err);
		return err;
	}

	for (i = 0; i < PIPE_DEV_NR; ++i) {
		pipe_dev[i] = kzalloc_obj(*pipe_dev[i]);
		if (!pipe_dev[i]) {
			err = -ENOMEM;
			goto err_destroy;
		}

		init_pipe_dev(pipe_dev[i]);

		err = cdev_add(&pipe_dev[i]->cdev,
			       MKDEV(MAJOR(pipe_devno), MINOR(pipe_devno) + i), 1);
		if (err) {
			pr_err("adding pipe%d failed: %d\n", i, err);
			kfree(pipe_dev[i]);
			pipe_dev[i] = NULL;
			goto err_destroy;
		}
	}

	return 0;

err_destroy:
	pipe_destroy_devs(i);
	unregister_chrdev_region(pipe_devno, PIPE_DEV_NR);
	return err;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	pipe_destroy_devs(PIPE_DEV_NR);
	unregister_chrdev_region(pipe_devno, PIPE_DEV_NR);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("A pipe like device showing how to put readers and writers to sleep");
