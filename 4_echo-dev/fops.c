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
	PDEBUG("\n`Open` invoked!\n");

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
	int retval = -ENOMEM;
	PDEBUG("`Write` is invoked\n");

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;


	count = (count > (dev->data_max_len - *f_pos)) ?
		(dev->data_max_len - *f_pos) : count;
	if (copy_from_user(dev->data + *f_pos, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	PDEBUG("Write %d bytes at %d\n", (int)count, (int)(*f_pos));
	retval = count;
	*f_pos += count;
	dev->data_len = (dev->data_len > *f_pos) ? dev->data_len : *f_pos;
out:
	mutex_unlock(&dev->mutex);
	return retval;
}

ssize_t echo_read(struct file *filp, char __user *buf, size_t count,
		  loff_t *f_pos) {
	struct echo_dev *dev = filp->private_data;
	int retval = 0;

	PDEBUG("`read` is invoked\n");

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	count = (count > (dev->data_len - *f_pos)) ?
	         (dev->data_len - *f_pos) : count;
	if (copy_to_user(buf, dev->data + *f_pos, count)) {
		retval = -EFAULT;
		goto out;
	}
	PDEBUG("Read %d bytes at %d\n", (int)count, (int)(*f_pos));

	retval = count;
	*f_pos += count;

out:
	mutex_unlock(&dev->mutex);
	return retval;
}

