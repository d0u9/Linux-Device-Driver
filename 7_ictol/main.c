#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "global.h"

static int ioctl_open(struct inode *inode, struct file *filp);
static ssize_t ioctl_read(struct file *filp, char __user *buff, size_t cout,
	loff_t *f_pos);

static int dev_minor = 0, dev_major = 0;
static char internal_buffer[BUFF_SIZE] = {0};
static int print_counter = 10;

module_param(print_counter, int, S_IRUGO);

static struct cdev dev;
static struct file_operations fops = {
	.open = ioctl_open,
	.read = ioctl_read,
};

static int __init ioctl_init(void)
{
	dev_t devno;
	int err = 0;

	PDEBUG("\n\tModule is loaded!\n");
	memset(internal_buffer, 0, BUFF_SIZE);
	snprintf(internal_buffer, BUFF_SIZE, "Hello World!\n");

	err = alloc_chrdev_region(&devno, dev_minor, DEV_NR, "ioctl-test");
	if (err < 0) {
		PDEBUG("Can't get major!\n");
		return err;
	} else {
		dev_major = MAJOR(devno);
	}
	devno = MKDEV(dev_major, dev_minor);

	cdev_init(&dev, &fops);
	dev.owner = THIS_MODULE;

	err = cdev_add(&dev, devno, 1);
	if (err) {
		PDEBUG("Errno adding device\n");
		return err;
	}

	return 0;
}

static void __exit ioctl_exit(void)
{
	dev_t devno;

	PDEBUG("Module is unloaded\n");
	cdev_del(&dev);

	devno = MKDEV(dev_major, dev_minor);
	unregister_chrdev_region(devno, DEV_NR);
}

module_init(ioctl_init);
module_exit(ioctl_exit);

MODULE_AUTHOR("Douglas");
MODULE_LICENSE("GPL");

// Below are methods for file operations
static int ioctl_open(struct inode *inode, struct file *filp)
{
	PDEBUG("`open`\n");
	filp->private_data = inode->i_cdev;

	return 0;
}

static ssize_t ioctl_read(struct file *filp, char __user *buff, size_t cout,
	loff_t *f_pos)
{
	PDEBUG("Prepare to read %zd bytes at %lld\n", cout, *f_pos);
	if (*f_pos >= DEV_NR)
		return 0;

	if (cout >BUFF_SIZE  - *f_pos)
		cout = BUFF_SIZE - *f_pos;

	PDEBUG("Actuall read %zd bytes\n", cout);
	if (copy_to_user(buff, internal_buffer + *f_pos, cout))
		return -EFAULT;

	*f_pos += cout;
	
	return cout;
}
