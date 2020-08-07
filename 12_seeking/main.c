#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include "main.h"
#include "fops.h"

static int seeking_major = 0, seeking_minor = 0;
static struct seeking_dev *seeking_dev = NULL;

static struct file_operations fops = {
	.owner  = THIS_MODULE,
	.open   = seeking_open,
	.read   = seeking_read,
	.llseek = seeking_llseek,
};

static
int __init m_init(void)
{
	dev_t devno;
	int err = 0;

	printk(KERN_WARNING MODULE_NAME " is loaded\n");

	seeking_dev = kmalloc(sizeof(struct seeking_dev), GFP_KERNEL);
	if (!seeking_dev) {
		pr_debug("Cannot malloc memeory!\n");
		return -ENOMEM;
	}
	memset(seeking_dev, 0, sizeof(struct seeking_dev));

	err = alloc_chrdev_region(&devno, seeking_minor, SEEKING_DEV_NR, MODULE_NAME);
	if (err < 0) {
		pr_debug("Can't get major!\n");
		goto on_error;
	}
	seeking_major = MAJOR(devno);

	// we initialize the internal buffer with DEC-HEX table.
	memcpy(seeking_dev->buff, HEX_DICT, HEX_DICT_LEN);
	mutex_init(&seeking_dev->mutex);
	cdev_init(&seeking_dev->cdev, &fops);
	seeking_dev->cdev.owner = THIS_MODULE;

	devno = MKDEV(seeking_major, seeking_minor);
	err = cdev_add(&seeking_dev->cdev, devno, 1);
	if (err) {
		pr_debug("Cannot add %s\n", MODULE_NAME);
		goto on_error;
	}

	return 0;

on_error:
	kfree(seeking_dev);
	return err;
}

static
void __exit m_exit(void)
{
	dev_t devno;

	printk(KERN_WARNING MODULE_NAME " unloaded\n");

	cdev_del(&seeking_dev->cdev);

	devno = MKDEV(seeking_major, seeking_minor);
	unregister_chrdev_region(devno, SEEKING_DEV_NR);

	kfree(seeking_dev);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Illustrate how to make a file seekable.");
