// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/minmax.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "main.h"
#include "fops.h"

int poll_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __func__);

	filp->private_data = container_of(inode->i_cdev, struct poll_dev, cdev);

	/* No file position: read() returns the whole message every time. */
	return stream_open(inode, filp);
}

/*
 * Take the readiness flag set by the timer, sleeping until it is set unless
 * O_NONBLOCK is given. atomic_xchg() makes sure only one caller gets it.
 */
static int take_flag(struct file *filp, atomic_t *flag, wait_queue_head_t *wq)
{
	while (!atomic_xchg(flag, 0)) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible(*wq, atomic_read(flag)))
			return -ERESTARTSYS;
	}

	return 0;
}

ssize_t poll_read(struct file *filp, char __user *buff, size_t count,
		  loff_t *f_pos)
{
	struct poll_dev *dev = filp->private_data;
	ssize_t retval;

	pr_debug("%s() is invoked\n", __func__);

	retval = take_flag(filp, &dev->can_rd, &dev->inq);
	if (retval)
		return retval;

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

ssize_t poll_write(struct file *filp, const char __user *buff, size_t count,
		   loff_t *f_pos)
{
	struct poll_dev *dev = filp->private_data;
	ssize_t retval;

	pr_debug("%s() is invoked\n", __func__);

	retval = take_flag(filp, &dev->can_wr, &dev->outq);
	if (retval)
		return retval;

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

out_unlock:
	mutex_unlock(&dev->mutex);
	return retval;
}

/*
 * Called by poll(), select() and epoll, possibly several times per call.
 * It must only register the wait queues and report the current state;
 * the flags are consumed by read() and write(), never here.
 */
__poll_t poll_poll(struct file *filp, poll_table *wait)
{
	struct poll_dev *dev = filp->private_data;
	__poll_t mask = 0;

	pr_debug("%s() is invoked\n", __func__);

	poll_wait(filp, &dev->inq, wait);
	poll_wait(filp, &dev->outq, wait);

	if (atomic_read(&dev->can_rd)) {
		pr_debug("now fd can be read\n");
		mask |= EPOLLIN | EPOLLRDNORM;
	}

	if (atomic_read(&dev->can_wr)) {
		pr_debug("now fd can be written\n");
		mask |= EPOLLOUT | EPOLLWRNORM;
	}

	pr_debug("return mask = 0x%x\n", (__force unsigned int)mask);
	return mask;
}
