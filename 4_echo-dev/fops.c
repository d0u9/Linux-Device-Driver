#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/atomic.h>

#include "global.h"
#include "fops.h"

int echo_open(struct inode *inode, struct file *filp) {
	struct echo_dev *dev;
	int opt_counter = 0;
	PDEBUG("`Open` invoked!\n");

	dev = container_of(inode->i_cdev, struct echo_dev, cdev);
	filp->private_data = dev;

	atomic_inc(&dev->opt_counter);
	opt_counter = atomic_read(&dev->opt_counter);
	PDEBUG("Have been opened %d times\n", opt_counter);

	return 0;
}

ssize_t echo_write(struct file *filp, const char __user *buf, size_t count,
		   loff_t *f_pos) {
	struct echo_dev *dev = filp->private_data;
	int retval = 0;
	PDEBUG("`Open` is invoked\n");

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;
	
	count = (count > dev->data_len - 1) ? dev->data_len - 1 : count;
	if (copy_from_user(dev->data, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	PDEBUG("BUFF: %s\n", dev->data);
	retval = count;
	*f_pos = 0;
out:
	mutex_unlock(&dev->mutex);
	return retval;
}

