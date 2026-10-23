// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/minmax.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fops.h"

/* Return the nr-th block of dev, counting from 0. Caller holds dev->mutex. */
static struct scull_block *scull_get_block(struct scull_dev *dev,
					   unsigned long nr)
{
	struct scull_block *pblock;

	list_for_each_entry(pblock, &dev->block_list, block_list)
		if (nr-- == 0)
			return pblock;

	return NULL;
}

int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;

	pr_debug("%s() is invoked\n", __func__);

	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev;

	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
		scull_trim(dev);
		mutex_unlock(&dev->mutex);
	}

	return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __func__);
	return 0;
}

ssize_t scull_read(struct file *filp, char __user *buff, size_t count,
		   loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_block *pblock;
	unsigned long tblock = *f_pos / SCULL_BLOCK_SIZE;
	size_t toffset = *f_pos % SCULL_BLOCK_SIZE;
	ssize_t retval = 0;

	pr_debug("%s() is invoked\n", __func__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	pblock = scull_get_block(dev, tblock);
	if (!pblock || toffset >= pblock->offset) {
		count = 0;
		goto out_unlock;	/* end of file */
	}

	/* Read at most up to the end of the data in this block. */
	count = min(count, pblock->offset - toffset);

	if (copy_to_user(buff, pblock->data + toffset, count)) {
		retval = -EFAULT;
		goto out_unlock;
	}

	retval = count;
	*f_pos += count;

out_unlock:
	pr_debug("RD pos = %lld, block = %lu, offset = %zu, read %zu bytes\n",
		 *f_pos, tblock, toffset, count);

	mutex_unlock(&dev->mutex);
	return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buff, size_t count,
		    loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_block *pblock;
	unsigned long tblock = *f_pos / SCULL_BLOCK_SIZE;
	size_t toffset = *f_pos % SCULL_BLOCK_SIZE;
	ssize_t retval = -ENOMEM;

	pr_debug("%s() is invoked\n", __func__);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	/* Grow the list until block tblock exists. */
	while (tblock + 1 > dev->block_counter) {
		pblock = kzalloc_obj(*pblock);
		if (!pblock)
			goto out_unlock;
		list_add_tail(&pblock->block_list, &dev->block_list);
		dev->block_counter++;
	}
	pblock = scull_get_block(dev, tblock);

	/* For simplicity, a single write never crosses a block boundary. */
	count = min(count, SCULL_BLOCK_SIZE - toffset);

	if (copy_from_user(pblock->data + toffset, buff, count)) {
		retval = -EFAULT;
		goto out_unlock;
	}

	retval = count;
	pblock->offset = max(pblock->offset, toffset + count);
	*f_pos += count;

out_unlock:
	pr_debug("WR pos = %lld, block = %lu, offset = %zu, write %zu bytes\n",
		 *f_pos, tblock, toffset, count);

	mutex_unlock(&dev->mutex);
	return retval;
}

void scull_trim(struct scull_dev *dev)
{
	struct scull_block *cur, *tmp;

	pr_debug("%s() is invoked\n", __func__);

	list_for_each_entry_safe(cur, tmp, &dev->block_list, block_list) {
		list_del(&cur->block_list);
		kfree(cur);
	}
	dev->block_counter = 0;
}
