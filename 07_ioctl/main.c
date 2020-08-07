#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fops.h"

static int ioctl_major = 0, ioctl_minor = 0;
static struct ioctl_dev *ioctl_dev = NULL;

static struct file_operations fops = {
	.open = ioctl_open,
	.read = ioctl_read,
	.unlocked_ioctl = ioctl_ioctl,
};

static
int __init m_init(void)
{
	dev_t devno;
	int err = 0;

	printk(KERN_WARNING MODULE_NAME " is loaded\n");

	ioctl_dev = kmalloc(sizeof(struct ioctl_dev), GFP_KERNEL);
	if (!ioctl_dev) {
		pr_debug("Cannot alloc memory!\n");
		return -ENOMEM;
	}

	memset(ioctl_dev, 0, sizeof(struct ioctl_dev));

	ioctl_dev->howmany = DEFAULT_HOWMANY;
	memcpy(ioctl_dev->buff, DEFAULT_MESSAGE, ARRAY_SIZE(DEFAULT_MESSAGE));
	ioctl_dev->buf_len = ARRAY_SIZE(DEFAULT_MESSAGE);

	err = alloc_chrdev_region(&devno, ioctl_minor, IOCTL_DEV_NR, MODULE_NAME);
	if (err < 0) {
		pr_debug("Can't get major!\n");
		goto on_error;
	}
	ioctl_major = MAJOR(devno);

	cdev_init(&ioctl_dev->cdev, &fops);
	ioctl_dev->cdev.owner = THIS_MODULE;

	devno = MKDEV(ioctl_major, ioctl_minor);
	err = cdev_add(&ioctl_dev->cdev, devno, IOCTL_DEV_NR);
	if (err) {
		pr_debug("Error when adding ioctl dev");
		goto on_error;
	}

	return 0;

on_error:
	kfree(ioctl_dev);
	return err;
}

static
void __exit m_exit(void)
{
	dev_t devno;

	printk(KERN_WARNING MODULE_NAME " unloaded\n");

	cdev_del(&ioctl_dev->cdev);

	devno = MKDEV(ioctl_major, ioctl_minor);
	unregister_chrdev_region(devno, IOCTL_DEV_NR);

	kfree(ioctl_dev);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("An ioctl() example");

