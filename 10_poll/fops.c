#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fops.h"

int poll_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);

	filp->private_data = container_of(inode->i_cdev, struct poll_dev, cdev);

	return 0;
}

ssize_t poll_read(struct file *filp, char __user *buff, size_t count,
			  loff_t *f_pos)
{
	int retval;
	struct poll_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	if (count > dev->buf_len - *f_pos)
		count = dev->buf_len - *f_pos;
	
	if (copy_to_user(buff, dev->buff + *f_pos, count)) {
		retval = -EFAULT;
		goto cpy_user_error;
	}

	*f_pos += count;
	retval = count;

cpy_user_error:
	mutex_unlock(&dev->mutex);
	return retval;
}

ssize_t poll_write(struct file *filp, const char __user *buff,
			   size_t count, loff_t *f_pos)
{
	int retval;
	struct poll_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	if (count > BUFF_SIZE - *f_pos)
		count = BUFF_SIZE - *f_pos;

	if (copy_from_user(dev->buff + *f_pos, buff, count)) {
		retval = -EFAULT;
		goto cpy_user_error;
	}

	*f_pos += count;
	dev->buf_len = *f_pos;
	retval = count;

cpy_user_error:
	mutex_unlock(&dev->mutex);
	return retval;
}

unsigned int poll_poll(struct file *filp, poll_table *wait)
{
	struct poll_dev *dev = filp->private_data;
	unsigned int mask = 0;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	mutex_lock(&dev->mutex);

	poll_wait(filp, &dev->inq, wait);
	poll_wait(filp, &dev->outq, wait);

	if (atomic_dec_and_test(&dev->can_rd)) {
		pr_debug("Now fd can be read\n");
		mask |= POLLIN | POLLRDNORM;
	}

	if (atomic_dec_and_test(&dev->can_wr)) {
		pr_debug("Now fd can be written\n");
		mask |= (POLLOUT | POLLWRNORM);
	}

	mutex_unlock(&dev->mutex);

	pr_debug("return mask = 0x%x\n", mask);
	return mask;
}
