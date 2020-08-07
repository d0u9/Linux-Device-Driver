#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include "main.h"
#include "fops.h"

int seeking_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);

	filp->private_data = container_of(inode->i_cdev, struct seeking_dev, cdev);

	return 0;
}

ssize_t seeking_read(struct file *filp, char __user *buff, size_t count,
 		 loff_t *f_pos)
{
	struct seeking_dev *dev = filp->private_data;
	int retval = 0;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	if (count > HEX_DICT_LEN - *f_pos)
		count = HEX_DICT_LEN - *f_pos;

	if (copy_to_user(buff, dev->buff + *f_pos, count)) {
		pr_debug("copy to user error!\n");
		retval = -EFAULT;
		goto copy_error;
	}

	*f_pos = (*f_pos + count ) % HEX_DICT_LEN;

	retval = count;

copy_error:
	mutex_unlock(&dev->mutex);
	return retval;
}

loff_t seeking_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	switch(whence) {
	case 0: /* SEEK_SET */
		newpos = off % HEX_DICT_LEN;
		break;

	case 1: /* SEEK_CUR */
		newpos = (filp->f_pos + off) % HEX_DICT_LEN;
		break;

	case 2: /* SEEK_END */
		newpos = off % HEX_DICT_LEN;
		break;

	default:
		return -EINVAL;

	}

	if (newpos >= HEX_DICT_LEN)
		return -EINVAL;

	filp->f_pos = newpos;

	return newpos;
}
