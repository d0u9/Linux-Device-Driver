// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/capability.h>
#include <linux/cleanup.h>
#include <linux/minmax.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fops.h"
#include "ioctl_cmd.h"

/* Caller holds dev->lock, or the device is not visible to userspace yet. */
void ioctl_dev_set_default(struct ioctl_dev *dev)
{
	dev->howmany = DEFAULT_HOWMANY;
	dev->buf_len = strlen(DEFAULT_MESSAGE);
	memset(dev->buff, 0, BUFF_SIZE);
	memcpy(dev->buff, DEFAULT_MESSAGE, dev->buf_len);
}

int ioctl_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __func__);

	filp->private_data = container_of(inode->i_cdev, struct ioctl_dev, cdev);

	return 0;
}

ssize_t ioctl_read(struct file *filp, char __user *buff, size_t count,
		   loff_t *f_pos)
{
	struct ioctl_dev *dev = filp->private_data;
	loff_t round;
	size_t offset;

	pr_debug("%s() is invoked\n", __func__);

	/* Released automatically on every return below. */
	guard(mutex)(&dev->lock);

	if (dev->buf_len == 0)
		return 0;

	/* The message is repeated, so split f_pos into round and offset. */
	round = *f_pos / dev->buf_len;
	offset = *f_pos % dev->buf_len;

	pr_debug("round = %lld, offset = %zu, howmany = %u\n",
		 round, offset, dev->howmany);

	if (round >= dev->howmany)
		return 0;

	count = min(count, dev->buf_len - offset);

	if (copy_to_user(buff, dev->buff + offset, count)) {
		pr_debug("error occurs when copying to userspace\n");
		return -EFAULT;
	}

	*f_pos += count;
	return count;
}

static long ioctl_howmany(struct ioctl_dev *dev, unsigned long arg)
{
	long old;

	/* permission check */
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (arg > INT_MAX)
		return -EINVAL;

	old = dev->howmany;
	dev->howmany = arg;
	pr_debug("set howmany = %u\n", dev->howmany);

	return old;
}

static long ioctl_message(struct ioctl_dev *dev, void __user *arg)
{
	struct ioctl_msg_arg msg_arg;

	if (copy_from_user(&msg_arg, arg, sizeof(msg_arg))) {
		pr_debug("copy arguments from user error\n");
		return -EFAULT;
	}

	if (msg_arg.len < 0 || msg_arg.len > BUFF_SIZE) {
		pr_debug("invalid message length: %d bytes\n", msg_arg.len);
		return -EINVAL;
	}

	if (copy_from_user(dev->buff, (void __user *)msg_arg.msg,
			   msg_arg.len)) {
		pr_debug("copy message from user error\n");
		return -EFAULT;
	}
	memset(dev->buff + msg_arg.len, 0, BUFF_SIZE - msg_arg.len);
	dev->buf_len = msg_arg.len;

	return 0;
}

long ioctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ioctl_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __func__);

	if (_IOC_TYPE(cmd) != IOCTL_IOC_MAGIC) {
		pr_debug("ioctl command error\n");
		return -ENOTTY;
	}

	if (_IOC_NR(cmd) > IOCTL_MAXNR) {
		pr_debug("ioctl command number error\n");
		return -ENOTTY;
	}

	guard(mutex)(&dev->lock);

	switch (cmd) {
	case IOCTL_RESET:
		pr_debug("ioctl -> cmd: reset\n");
		ioctl_dev_set_default(dev);
		return 0;
	case IOCTL_HOWMANY:
		pr_debug("ioctl -> cmd: set howmany\n");
		return ioctl_howmany(dev, arg);
	case IOCTL_MESSAGE:
		pr_debug("ioctl -> cmd: set print message\n");
		return ioctl_message(dev, (void __user *)arg);
	default:
		return -ENOTTY;
	}
}
