#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include "main.h"
#include "fops.h"

int pipe_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);

	filp->private_data = container_of(inode->i_cdev, struct pipe_dev, cdev);

	return 0;
}

ssize_t pipe_read(struct file *filp, char __user *buff, size_t count,
	loff_t *f_pos)
{
	int retval = 0;
	struct pipe_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	while (!dev->buff_len) {
		DEFINE_WAIT(wait);

		mutex_unlock(&dev->mutex);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		pr_debug("read: process %d(%s) is going to sleep\n",
		       current->pid, current->comm);

		prepare_to_wait(&dev->rd_queue, &wait, TASK_INTERRUPTIBLE);
		if (!dev->buff_len)
			schedule();
		finish_wait(&dev->rd_queue, &wait);

		if (signal_pending(current))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
	}

	if (count > dev->buff_len - *f_pos)
		count = dev->buff_len - *f_pos;

	if (copy_to_user(buff, dev->buff + *f_pos, count)) {
		pr_debug("copy to user error!\n");
		retval = -EFAULT;
		goto copy_error;
	}

	pr_debug("read: f_pos=%lld, count=%lu, buff_len=%d\n",
	       *f_pos, count, dev->buff_len);

	*f_pos += count;

	// all data in the buff have been read
	if (*f_pos >= dev->buff_len) {
		dev->buff_len = 0;
		*f_pos = 0;
		pr_debug("read: process %d(%s) awakening the writers...\n",
		       current->pid, current->comm);
		wake_up_interruptible(&dev->wr_queue);
	}

	retval = count;

copy_error:
	mutex_unlock(&dev->mutex);
	return retval;
}

ssize_t pipe_write(struct file *filp, const char __user *buff,
		   size_t count, loff_t *f_pos)
{
	int retval = 0;
	struct pipe_dev *dev = filp->private_data;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	while (dev->buff_len) {
		DEFINE_WAIT(wait);

		mutex_unlock(&dev->mutex);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		pr_debug("process %d(%s) is going to sleep\n",
		       current->pid, current->comm);

		prepare_to_wait(&dev->wr_queue, &wait, TASK_INTERRUPTIBLE);
		if (dev->buff_len)
			schedule();
		finish_wait(&dev->wr_queue, &wait);

		if (signal_pending(current))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
	}

	if (count > BUFF_SIZE - *f_pos)
		count = BUFF_SIZE - *f_pos;

	if (copy_from_user(dev->buff + *f_pos, buff, count)) {
		pr_debug("write: copy to user error!\n");
		retval = -EFAULT;
		goto copy_error;
	}

	pr_debug("write: f_pos=%lld, count=%lu, buff_len=%d\n",
	       *f_pos, count, dev->buff_len);

	// we have successfully write something in the buff
	if (count > 0) {
		dev->buff_len = count;
		*f_pos = 0;

		pr_debug("write: process %d(%s) awakening the readers...\n",
		       current->pid, current->comm);
		wake_up_interruptible(&dev->rd_queue);
	}

	retval = count;

copy_error:
	mutex_unlock(&dev->mutex);
	return retval;
}


