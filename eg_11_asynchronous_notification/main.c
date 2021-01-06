#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fops.h"

static int async_notify_major = 0, async_notify_minor = 0;
static struct async_notify_dev *async_notify_dev = NULL;

static struct file_operations fops = {
	.open  = async_notify_open,
	.read  = async_notify_read,
	.write = async_notify_write,
	.fasync = async_notify_fasync,
	.release = async_notify_release,
};

static
int __init m_init(void)
{
	dev_t devno;
	int err = 0;

	printk(KERN_WARNING MODULE_NAME " is loaded\n");

	async_notify_dev = kmalloc(sizeof(struct async_notify_dev), GFP_KERNEL);
	if (!async_notify_dev) {
		pr_debug("Cannot alloc memory!\n");
		return -ENOMEM;
	}
	memset(async_notify_dev, 0, sizeof(struct async_notify_dev));

	err = alloc_chrdev_region(&devno, async_notify_minor, ASYNC_NOTIFY_DEV_NR, MODULE_NAME);
	if (err < 0) {
		pr_debug("Can't get major!\n");
		goto on_error;
	}
	async_notify_major = MAJOR(devno);

	mutex_init(&async_notify_dev->mutex);
	cdev_init(&async_notify_dev->cdev, &fops);
	async_notify_dev->cdev.owner = THIS_MODULE;

	devno = MKDEV(async_notify_major, async_notify_minor);
	err = cdev_add(&async_notify_dev->cdev, devno, ASYNC_NOTIFY_DEV_NR);
	if (err) {
		pr_debug("Error when adding ioctl dev");
		goto on_error;
	}

	return 0;

on_error:
	kfree(async_notify_dev);
	return err;
}

static
void __exit m_exit(void)
{
	dev_t devno;

	printk(KERN_WARNING MODULE_NAME " unloaded\n");

	cdev_del(&async_notify_dev->cdev);

	devno = MKDEV(async_notify_major, async_notify_minor);
	unregister_chrdev_region(devno, ASYNC_NOTIFY_DEV_NR);

	kfree(async_notify_dev);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Asynchronous notification for non-block IO");

