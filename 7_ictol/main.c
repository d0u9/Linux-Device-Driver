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
static long ioctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

static int dev_minor = 0, dev_major = 0;
static char *internal_buffer = NULL;
static int print_counter = 10;
static int str_len = DEFAULT_STR_LEN;

module_param(print_counter, int, S_IRUGO);

static struct cdev dev;
static struct file_operations fops = {
	.open = ioctl_open,
	.read = ioctl_read,
	.unlocked_ioctl = ioctl_ioctl,
};

static int __init ioctl_init(void)
{
	dev_t devno;
	int err = 0;

	PDEBUG("\n\tModule is loaded!\n");
	str_len = DEFAULT_STR_LEN;
	internal_buffer = kmalloc(BUFF_SIZE, GFP_KERNEL);
	memset(internal_buffer, 0, BUFF_SIZE);
	memcpy(internal_buffer, DEFAULT_STR, DEFAULT_STR_LEN);


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
	kfree(internal_buffer);

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
	int iterations = 0;
	int pos = 0;
	PDEBUG("Prepare to read %zd bytes at %lld\n", cout, *f_pos);
	if (*f_pos >= str_len * print_counter)
		return 0;

	iterations = *f_pos / str_len;
	pos = *f_pos % str_len;

	if (iterations >= print_counter)
		return 0;
	if (cout > str_len - pos)
		cout = str_len - pos;

	PDEBUG("Actuall read %zd bytes\n", cout);
	if (copy_to_user(buff, internal_buffer + pos, cout))
		return -EFAULT;

	*f_pos += cout;
	
	return cout;
}

static long ioctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, tmp;
	long retval = 0;

	PDEBUG("ioctl invoked!\n");
	PDEBUG("IOCTL_RESET = %d,"
		"IOCTL_LOOPNR = %d,\n"
		"IOCTL_STR = %d,\n", IOCTL_RESET, IOCTL_LOOPNR, IOCTL_STR);
	if (_IOC_TYPE(cmd) != IOCTL_IOC_MAGIC) {
		PDEBUG("CMD error!\n");
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > IOCTL_MAXNR) {
		PDEBUG("exceeds max nr!\n");
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	
	if (err)
		return -EFAULT;

	PDEBUG("Parameter verification OK!\n");

	switch (cmd) {
	case IOCTL_RESET:
		PDEBUG("ioctl cmd -> reset\n");
		memset(internal_buffer, 0, BUFF_SIZE);
		memcpy(internal_buffer, DEFAULT_STR, DEFAULT_STR_LEN);
		str_len = DEFAULT_STR_LEN;
		break;

	default:
		return -ENOTTY;
	}

	return retval;
}
