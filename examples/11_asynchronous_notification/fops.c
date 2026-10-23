// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/minmax.h>
#include <linux/signal.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fops.h"

int async_notify_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __func__);

	filp->private_data = container_of(inode->i_cdev,
					  struct async_notify_dev, cdev);

	/* No file position: read() returns the whole message every time. */
	return stream_open(inode, filp);
}

ssize_t async_notify_read(struct file *filp, char __user *buff, size_t count,
			  loff_t *f_pos)
{
	struct async_notify_dev *dev = filp->private_data;
	ssize_t retval;

	pr_debug("%s() is invoked\n", __func__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	count = min(count, dev->buf_len);

	if (copy_to_user(buff, dev->buff, count)) {
		retval = -EFAULT;
		goto out_unlock;
	}

	retval = count;

out_unlock:
	mutex_unlock(&dev->mutex);
	return retval;
}

ssize_t async_notify_write(struct file *filp, const char __user *buff,
			   size_t count, loff_t *f_pos)
{
	struct async_notify_dev *dev = filp->private_data;
	ssize_t retval;

	pr_debug("%s() is invoked\n", __func__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	/* the new message replaces the old one */
	count = min_t(size_t, count, BUFF_SIZE);

	if (copy_from_user(dev->buff, buff, count)) {
		retval = -EFAULT;
		goto out_unlock;
	}

	dev->buf_len = count;
	retval = count;

	/*
	 * Send SIGIO to every process that enabled O_ASYNC on this device.
	 * POLL_IN ends up in si_code and tells them data can be read.
	 */
	kill_fasync(&dev->async_queue, SIGIO, POLL_IN);

out_unlock:
	mutex_unlock(&dev->mutex);
	return retval;
}

/* Called when O_ASYNC is set or cleared with fcntl(F_SETFL). */
int async_notify_fasync(int fd, struct file *filp, int mode)
{
	struct async_notify_dev *dev = filp->private_data;

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

int async_notify_release(struct inode *inode, struct file *filp)
{
	struct async_notify_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __func__);

	/*
	 * Remove this file from the async queue. __fput() already does the
	 * same by calling ->fasync(-1, filp, 0) when FASYNC is set, so this
	 * is not strictly needed; it is written out to pair with
	 * async_notify_fasync().
	 */
	fasync_helper(-1, filp, 0, &dev->async_queue);

	return 0;
}
