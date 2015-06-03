#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>

#include "main.h"
#include "fops.h"

static int scull_trim(struct scull_dev *dev);

int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;
	int err = 0;
	PDEBUG("`OPEN` is invoked\n");

	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev;
	atomic_inc(&dev->open_counter);

	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		err = scull_trim(dev);
		if (err)
			return err;
	}
	return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
	PDEBUG("`RELEASE` is invoked\n");
	return 0;
}

ssize_t scull_read(struct file *filp, char __user *buff, size_t count,
	loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct store_block *cur = NULL;
	int block_index = 0, pos_in_block = 0;
	int need_to_read = 0;
	ssize_t retval = 0;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	block_index = (long) *f_pos / SCULL_BUFF_SIZE + 1;
	pos_in_block = (long) *f_pos % SCULL_BUFF_SIZE;

	if (block_index > atomic_read(&dev->list_entry_counter))
		return 0;

	need_to_read = (pos_in_block + count) > SCULL_BUFF_SIZE ? 
		SCULL_BUFF_SIZE : count;
	PDEBUG("Need to read %d bytes\n", need_to_read);

	list_for_each_entry(cur, &dev->list, list) {
		if (!(--block_index))
			break;
	}

	if (copy_to_user(buff, cur->data + pos_in_block, need_to_read)) {
	    retval = -EFAULT;
	    goto out;
	}

	*f_pos += need_to_read;
	retval = need_to_read;

out:
	mutex_unlock(&dev->mutex);
	return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buff, size_t count,
	loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct store_block *cur_block = NULL;
	int block_index = 0, pos_in_block = 0, blocks = 0;
	int need_to_write = 0;
	ssize_t retval = -ENOMEM;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	block_index = (long) *f_pos / SCULL_BUFF_SIZE + 1;
	pos_in_block = (long) *f_pos % SCULL_BUFF_SIZE;
	blocks = atomic_read(&dev->list_entry_counter);

	if (pos_in_block == 0) {
		PDEBUG("Need to create a new block\n");
		cur_block = (struct store_block *)
			kmalloc(sizeof(*cur_block), GFP_KERNEL);
		memset(cur_block->data, 0, SCULL_BUFF_SIZE);
		INIT_LIST_HEAD(&cur_block->list);
		list_add_tail(&cur_block->list, &dev->list);
		atomic_inc(&dev->list_entry_counter);
	} else {
		PDEBUG("Do not need to create a new block\n");
		list_last_entry(&cur_block->list, struct store_block, list);
	}

	need_to_write = ((SCULL_BUFF_SIZE - pos_in_block) < count) ?
			(SCULL_BUFF_SIZE - pos_in_block) : count;
	PDEBUG("Prepare to write %d bytes to file\n", need_to_write);

	if (copy_from_user(cur_block->data + pos_in_block, buff, need_to_write)) {
		retval = -EFAULT;
		goto out;
	}

	*f_pos += need_to_write;
	retval = need_to_write;

out:
	mutex_unlock(&dev->mutex);
	return retval;
}

static int scull_trim(struct scull_dev *dev)
{
	struct store_block *tmp = NULL, *cur = NULL;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	list_for_each_entry_safe(cur, tmp, &dev->list, list) {
		list_del(&cur->list);
		kfree(cur);
	}

	INIT_LIST_HEAD(&dev->list);
	atomic_set(&dev->list_entry_counter, 0);
	mutex_unlock(&dev->mutex);

	return 0;
}

