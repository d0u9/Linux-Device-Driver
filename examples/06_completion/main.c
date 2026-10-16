#include <linux/module.h>
#include <linux/init.h> 
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/completion.h>

#include "main.h"

static int completion_major = 0, completion_minor = 0;

static struct completion_dev completion_dev;

static
int completion_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);

	filp->private_data = container_of(inode->i_cdev,
					  struct completion_dev, cdev);

	return 0;
}

static
ssize_t completion_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
	struct completion_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	pr_debug("process %d(%s) is going to sleep\n", current->pid, current->comm);
	wait_for_completion(&dev->completion);
	pr_debug("awoken %d(%s)\n", current->pid, current->comm);

	return 0;
}

static
ssize_t completion_write(struct file *filp, const char __user *buf, size_t count,
		       loff_t *pos)
{
	struct completion_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	pr_debug("process %d(%s) awakening the readers...\n",
	       current->pid, current->comm);
	complete(&dev->completion);

	return count;
}

static struct file_operations completion_fops = {
	.owner = THIS_MODULE,
	.open  = completion_open, 
	.read  = completion_read,
	.write = completion_write,
};

static
int __init m_init(void)
{
	int err = 0;
	dev_t devno;

	printk(KERN_WARNING MODULE_NAME " is loaded\n");

	init_completion(&completion_dev.completion);

	err = alloc_chrdev_region(&devno, completion_minor, 1, MODULE_NAME);
	if (err < 0) {
		pr_debug("Cant't get major");
		return err;
	}
	completion_major = MAJOR(devno);

	cdev_init(&completion_dev.cdev, &completion_fops);

	devno = MKDEV(completion_major, completion_minor);
	err = cdev_add(&completion_dev.cdev, devno, 1);
	if (err) {
		pr_debug("Error(%d): Adding completion device error\n", err);
		return err;
	}

	return 0;
}

static
void __exit m_exit(void)
{
	dev_t devno;

	printk(KERN_WARNING MODULE_NAME " unloaded\n");

	cdev_del(&completion_dev.cdev);

	devno = MKDEV(completion_major, completion_minor);
	unregister_chrdev_region(devno, 1);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Example of Kernel's completion mechanism");
