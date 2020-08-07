#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fops.h"

static int poll_major = 0, poll_minor = 0;
static struct poll_dev *poll_dev = NULL;

static struct file_operations fops = {
	.open  = poll_open,
	.read  = poll_read,
	.write = poll_write,
	.poll  = poll_poll,
};

static
void timer_fn(struct timer_list *t)
{
	struct poll_dev *dev = from_timer(dev, t, timer);

	pr_debug("%s() is invoked\n", __FUNCTION__);

	++dev->timer_counter;

	// make readable per one timer interval
	atomic_set(&dev->can_rd, 1);
	wake_up_interruptible(&dev->inq);

	// make writable per two timer interval
	if (dev->timer_counter % 2) {
		atomic_set(&dev->can_wr, 1);
		wake_up_interruptible(&dev->outq);
	}

	dev->timer.expires = jiffies + TIMER_INTERVAL;
	add_timer(&dev->timer);
}

static
void init_dev(struct poll_dev *dev)
{
	mutex_init(&dev->mutex);
	timer_setup(&dev->timer, timer_fn, 0);
	dev->timer.expires = jiffies + TIMER_INTERVAL;

	atomic_set(&dev->can_wr, 0);
	atomic_set(&dev->can_rd, 0);

	dev->timer_counter = 0;

	init_waitqueue_head(&dev->inq);
	init_waitqueue_head(&dev->outq);

	cdev_init(&dev->cdev, &fops);
	dev->cdev.owner = THIS_MODULE;

	dev->buf_len = ARRAY_SIZE(DFT_MSG);
	memcpy(dev->buff, DFT_MSG, dev->buf_len);
}

static
int __init m_init(void)
{
	dev_t devno;
	int err = 0;

	printk(KERN_WARNING MODULE_NAME " is loaded\n");

	poll_dev = kmalloc(sizeof(struct poll_dev), GFP_KERNEL);
	if (!poll_dev) {
		pr_debug("Cannot alloc memory!\n");
		return -ENOMEM;
	}
	memset(poll_dev, 0, sizeof(struct poll_dev));

	err = alloc_chrdev_region(&devno, poll_minor, POLL_DEV_NR, MODULE_NAME);
	if (err < 0) {
		pr_debug("Can't get major!\n");
		goto on_error;
	}
	poll_major = MAJOR(devno);

	init_dev(poll_dev);
	add_timer(&poll_dev->timer);

	devno = MKDEV(poll_major, poll_minor);
	err = cdev_add(&poll_dev->cdev, devno, POLL_DEV_NR);
	if (err) {
		pr_debug("Error when adding ioctl dev");
		goto on_error;
	}

on_error:
	kfree(poll_dev);
	return err;
}

static
void __exit m_exit(void)
{
	dev_t devno;

	printk(KERN_WARNING MODULE_NAME " unloaded\n");

	cdev_del(&poll_dev->cdev);
	del_timer_sync(&poll_dev->timer);

	devno = MKDEV(poll_major, poll_minor);
	unregister_chrdev_region(devno, POLL_DEV_NR);

	kfree(poll_dev);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple poll example");

