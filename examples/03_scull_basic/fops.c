#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>

#include "main.h"
#include "fops.h"

int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;

	pr_debug("%s() is invoked\n", __FUNCTION__);

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
	pr_debug("%s() is invoked\n", __FUNCTION__);
	return 0;
}

ssize_t scull_read(struct file *filp, char __user *buff, size_t count,
		   loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_block *pblock = NULL;
	loff_t retval = -ENOMEM;
	loff_t tblock = 0, toffset = 0;
	struct list_head *plist = NULL;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	tblock = *f_pos / SCULL_BLOCK_SIZE;
	toffset = *f_pos % SCULL_BLOCK_SIZE;

	if (mutex_lock_interruptible(&dev->mutex))
	    return -ERESTARTSYS;

	if (tblock + 1 > dev->block_counter) {
		retval = 0;
		goto end_of_file;
	}

	plist = &dev->block_list;
	for (int i = 0; i < tblock + 1; ++i) {
		plist = plist->next;
	}

	pblock = list_entry(plist, struct scull_block, block_list);
	if (toffset >= pblock->offset) {
		retval = 0;
		goto end_of_file;
	}

	if (count > pblock->offset)
		count = pblock->offset;

	if (copy_to_user(buff, pblock->data, count)) {
		retval = -EFAULT;
		goto cpy_user_error;
	}

	retval = count;
	*f_pos += count;

end_of_file:
cpy_user_error:
	pr_debug("RD pos = %lld, block = %lld, offset = %lld, read %lu bytes\n",
	       *f_pos, tblock, toffset, count);

	mutex_unlock(&dev->mutex);
	return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buff, size_t count,
		    loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_block *pblock = NULL;
	loff_t retval = -ENOMEM;
	loff_t tblock = 0, toffset = 0;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	tblock = *f_pos / SCULL_BLOCK_SIZE;
	toffset = *f_pos % SCULL_BLOCK_SIZE;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	/*
	 * For simplicity, we write one block each write request.
	 */
	while (tblock + 1 > dev->block_counter) {
		if (!(pblock = kmalloc(sizeof(struct scull_block), GFP_KERNEL)))
			goto malloc_error;
		memset(pblock, 0, sizeof(struct scull_block));
		INIT_LIST_HEAD(&pblock->block_list);
		list_add_tail(&pblock->block_list, &dev->block_list);
		dev->block_counter++;
	}
	pblock = list_last_entry(&dev->block_list, struct scull_block, block_list);

	if (count > SCULL_BLOCK_SIZE - toffset)
		count = SCULL_BLOCK_SIZE - toffset;

	if (copy_from_user(pblock->data + toffset, buff, count)) {
		retval = -EFAULT;
		goto cpy_user_error;
	}
	
	retval = count;
	pblock->offset += count;
	*f_pos += count;

malloc_error:
cpy_user_error:
	pr_debug("WR pos = %lld, block = %lld, offset = %lld, write %lu bytes\n",
	       *f_pos, tblock, toffset, count);

	mutex_unlock(&dev->mutex);
	return retval;
}

void scull_trim(struct scull_dev *dev)
{
	struct scull_block *cur = NULL, *tmp = NULL;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	list_for_each_entry_safe(cur, tmp, &dev->block_list, block_list) {
		list_del(&cur->block_list);
		memset(cur, 0, sizeof(*cur));
		kfree(cur);
	}
	dev->block_counter = 0;
}

