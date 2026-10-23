// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/minmax.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fops.h"

int pipe_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __func__);

	filp->private_data = container_of(inode->i_cdev, struct pipe_dev, cdev);

	/*
	 * Like a real pipe, this device has no file position: the buffer and
	 * the read position are shared by everyone who opens it. stream_open()
	 * makes lseek(), pread() and pwrite() fail with -ESPIPE, and read() and
	 * write() are then called with f_pos == NULL, so never touch it.
	 */
	return stream_open(inode, filp);
}

ssize_t pipe_read(struct file *filp, char __user *buff, size_t count,
		  loff_t *f_pos)
{
	struct pipe_dev *dev = filp->private_data;
	ssize_t retval;

	pr_debug("%s() is invoked\n", __func__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	/* if no data is available, put the process to sleep */
	while (!dev->buff_len) {
		mutex_unlock(&dev->mutex);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		pr_debug("read: process %d(%s) is going to sleep\n",
			 current->pid, current->comm);

		/* buff_len is read without the mutex here, hence READ_ONCE() */
		if (wait_event_interruptible(dev->rd_queue,
					     READ_ONCE(dev->buff_len)))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
	}

	count = min(count, dev->buff_len - dev->rd_pos);

	if (copy_to_user(buff, dev->buff + dev->rd_pos, count)) {
		pr_debug("read: copy to user error!\n");
		retval = -EFAULT;
		goto out_unlock;
	}

	pr_debug("read: rd_pos=%zu, count=%zu, buff_len=%zu\n",
		 dev->rd_pos, count, dev->buff_len);

	dev->rd_pos += count;

	/* all data in the buffer has been read */
	if (dev->rd_pos >= dev->buff_len) {
		/* pairs with the lockless READ_ONCE() in pipe_write() */
		WRITE_ONCE(dev->buff_len, 0);
		dev->rd_pos = 0;
		pr_debug("read: process %d(%s) awakening the writers...\n",
			 current->pid, current->comm);
		wake_up_interruptible(&dev->wr_queue);
	}

	retval = count;

out_unlock:
	mutex_unlock(&dev->mutex);
	return retval;
}

ssize_t pipe_write(struct file *filp, const char __user *buff, size_t count,
		   loff_t *f_pos)
{
	struct pipe_dev *dev = filp->private_data;
	ssize_t retval;

	pr_debug("%s() is invoked\n", __func__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	/* wait until the readers have drained the buffer */
	while (dev->buff_len) {
		mutex_unlock(&dev->mutex);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		pr_debug("write: process %d(%s) is going to sleep\n",
			 current->pid, current->comm);

		if (wait_event_interruptible(dev->wr_queue,
					     !READ_ONCE(dev->buff_len)))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
	}

	count = min_t(size_t, count, BUFF_SIZE);

	if (copy_from_user(dev->buff, buff, count)) {
		pr_debug("write: copy from user error!\n");
		retval = -EFAULT;
		goto out_unlock;
	}

	pr_debug("write: count=%zu\n", count);

	/* we have successfully written something into the buffer */
	if (count > 0) {
		/* pairs with the lockless READ_ONCE() in pipe_read() */
		WRITE_ONCE(dev->buff_len, count);
		dev->rd_pos = 0;

		pr_debug("write: process %d(%s) awakening the readers...\n",
			 current->pid, current->comm);
		wake_up_interruptible(&dev->rd_queue);
	}

	retval = count;

out_unlock:
	mutex_unlock(&dev->mutex);
	return retval;
}
