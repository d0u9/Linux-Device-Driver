#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fops.h"

int ioctl_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);

	filp->private_data = container_of(inode->i_cdev, struct ioctl_dev, cdev);

	return 0;
}

ssize_t ioctl_read(struct file *filp, char __user *buff, size_t count,
	loff_t *f_pos)
{
	int retval = 0;
	int howmany = 0, offset = 0;
	struct ioctl_dev *ioctl_dev = filp->private_data;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (ioctl_dev->buf_len == 0)
		return 0;

	howmany = *f_pos / ioctl_dev->buf_len;
	offset = *f_pos % ioctl_dev->buf_len;

	pr_debug("howmany = %d, offset=%d, many=%d\n", howmany, offset, ioctl_dev->howmany);

	if (howmany >= ioctl_dev->howmany)
		return 0;

	if (count > ioctl_dev->buf_len - offset)
		count = ioctl_dev->buf_len - offset;

	if (copy_to_user(buff, ioctl_dev->buff, count)) {
		pr_debug("Error occurs shen copy to userspace\n");
		retval = -EFAULT;
		return retval;
	}

	*f_pos += count;
	return count;
}

static
int ioctl_reset(struct ioctl_dev *dev)
{
	dev->howmany = DEFAULT_HOWMANY;
	dev->buf_len = 0;
	memset(dev->buff, 0, BUFF_SIZE);
	memcpy(dev->buff, DEFAULT_MESSAGE, ARRAY_SIZE(DEFAULT_MESSAGE));

	return 0;
}

static
int ioctl_howmany(struct ioctl_dev *dev, unsigned long arg)
{
	int retval = 0;

	//permission check
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	
	retval = dev->howmany;
	dev->howmany = arg;
	pr_debug("set howmany = %d\n", dev->howmany);

	return retval;
}

static
int ioctl_message(struct ioctl_dev *dev, void __user *arg)
{
	int retval = 0;
	struct ioctl_msg_arg msg_arg;

	if (copy_from_user(&msg_arg, arg, sizeof(struct ioctl_msg_arg))) {
		pr_debug("copy arguments from user error\n");
		retval = -EFAULT;
		return retval;
	}

	if (msg_arg.len > BUFF_SIZE) {
		pr_debug("message length (%d bytes) exceeds the limit\n", msg_arg.len);
		return -ENOMEM;
	}

	if (copy_from_user(dev->buff, (void __user *)msg_arg.msg, msg_arg.len)) {
		pr_debug("copy message from user error\n");
		retval = -EFAULT;
		return retval;
	}
	memset(dev->buff + msg_arg.len, 0, BUFF_SIZE - msg_arg.len);
	dev->buf_len = msg_arg.len;

	return 0;
}

long ioctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, retval = 0;
	struct ioctl_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (_IOC_TYPE(cmd) != IOCTL_IOC_MAGIC) {
		pr_debug("ioctl command error\n");
		return -ENOTTY;
	}

	if (_IOC_NR(cmd) > IOCTL_MAXNR) {
		pr_debug("Number of ioctl parameters error\n");
		return -ENOTTY;
	}

	err = access_ok(arg, _IOC_SIZE(cmd));
	if (!err)
		return -EFAULT;

	switch (cmd) {
	case IOCTL_RESET:
		pr_debug("ioctl -> cmd: reset\n");
		retval = ioctl_reset(dev);
		break;
	case IOCTL_HOWMANY:
		pr_debug("ioctl -> cmd: set howmany\n");
		retval = ioctl_howmany(dev, arg);
		break;
	case IOCTL_MESSAGE:
		pr_debug("ioctl -> cmd: set print message\n");
		retval = ioctl_message(dev, (void *__user)arg);
		break;
	default:
		return -ENOTTY;
	}

	return retval;

}

