#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#include "main.h"
#include "fops.h"

static int pipe_major = 0, pipe_minor = 0;
static struct pipe_dev *pipe_dev[PIPE_DEV_NR];

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open  = pipe_open,
	.read  = pipe_read,
	.write = pipe_write
};

static
void __init init_pipe_dev(struct pipe_dev *dev)
{
	memset(dev, 0, sizeof(struct pipe_dev));
	mutex_init(&dev->mutex);

	cdev_init(&dev->cdev, &fops);
	dev->cdev.owner = THIS_MODULE;

	init_waitqueue_head(&dev->rd_queue);
	init_waitqueue_head(&dev->wr_queue);
}

static
int __init m_init(void)
{
	dev_t devno;
	int err = 0;

	printk(KERN_WARNING MODULE_NAME " is loaded\n");

	err = alloc_chrdev_region(&devno, pipe_minor, PIPE_DEV_NR, MODULE_NAME);
	if (err < 0) {
		pr_debug("Can't get major!\n");
		return err;
	}
	pipe_major = MAJOR(devno);

	for (int i = 0; i < PIPE_DEV_NR; ++i) {
		pipe_dev[i] = kmalloc(sizeof(struct pipe_dev), GFP_KERNEL);
		if (!pipe_dev[i]) {
			pr_debug("Error(%d): kmalloc failed on pipe%d\n", err, i);
			continue;
		}

		init_pipe_dev(pipe_dev[i]);

		devno = MKDEV(pipe_major, pipe_minor + i);
		err = cdev_add(&pipe_dev[i]->cdev, devno, 1);
		if (err) {
			pr_debug("Error(%d): Adding pipe%d error\n", err, i);
			kfree(pipe_dev[i]);
			pipe_dev[i] = NULL;
		}
	}

	return 0;
}

static
void __exit m_exit(void)
{
	dev_t devno;

	printk(KERN_WARNING MODULE_NAME " unloaded\n");

	for (int i = 0; i < PIPE_DEV_NR; ++i) {
		cdev_del(&pipe_dev[i]->cdev);
		kfree(pipe_dev[i]);
	}

	devno = MKDEV(pipe_major, pipe_minor);
	unregister_chrdev_region(devno, PIPE_DEV_NR);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("A pipe like device to illustrate the skill of how to put"
		   "the read/write process into sleep");

