// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/minmax.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fops.h"

int seeking_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __func__);

	filp->private_data = container_of(inode->i_cdev, struct seeking_dev,
					  cdev);

	return 0;
}

ssize_t seeking_read(struct file *filp, char __user *buff, size_t count,
		     loff_t *f_pos)
{
	struct seeking_dev *dev = filp->private_data;
	ssize_t retval;
	size_t pos;

	pr_debug("%s() is invoked\n", __func__);

	/*
	 * *f_pos does not always come from seeking_llseek(): pread() passes
	 * any offset the caller likes. Check it before using it as an index.
	 */
	if (*f_pos < 0)
		return -EINVAL;
	pos = *f_pos % HEX_DICT_LEN;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	count = min(count, HEX_DICT_LEN - pos);

	if (copy_to_user(buff, dev->buff + pos, count)) {
		pr_debug("copy to user error!\n");
		retval = -EFAULT;
		goto out_unlock;
	}

	/* wrap around after the last entry */
	*f_pos = (pos + count) % HEX_DICT_LEN;
	retval = count;

out_unlock:
	mutex_unlock(&dev->mutex);
	return retval;
}

loff_t seeking_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;

	pr_debug("%s() is invoked\n", __func__);

	switch (whence) {
	case SEEK_SET:
		newpos = off;
		break;
	case SEEK_CUR:
		newpos = filp->f_pos + off;
		break;
	case SEEK_END:
		newpos = HEX_DICT_LEN + off;
		break;
	default:
		return -EINVAL;
	}

	if (newpos < 0)
		return -EINVAL;

	/* positions wrap around the table */
	newpos %= HEX_DICT_LEN;
	filp->f_pos = newpos;

	return newpos;
}
