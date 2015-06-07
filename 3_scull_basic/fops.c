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


int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;
	PDEBUG("`OPEN` is invoked\n");

	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev;
	atomic_inc(&dev->open_counter);

	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
		scull_trim(dev);
		mutex_unlock(&dev->mutex);
	}

	PDEBUG("`OPEN` finished\n");
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
	int target_block = 0, target_pos = 0;
	int retval = 0;

	PDEBUG("`Read` invoked\n");

	target_block = *f_pos / SCULL_BUFF_SIZE;
	target_pos = *f_pos %SCULL_BUFF_SIZE;

	if (mutex_lock_interruptible(&dev->mutex))
	    return -ERESTARTSYS;

	if (target_block >= dev->list_entry_counter) {
		retval = 0;
		goto out;
	}

	list_for_each_entry(cur, &dev->list, list) {
		if (!(target_block--))
			break;
	}

	if (target_pos >= cur->pos) {
		retval = 0;
		goto out;
	}

	if (count > cur->pos)
		count = cur->pos;
	PDEBUG("read %d bytes\n", count);

	if (copy_to_user(buff, cur->data, count)) {
		retval = EFAULT;
		goto out;
	}

	retval = count;
	*f_pos += count;

out:
	mutex_unlock(&dev->mutex);
	PDEBUG("`Read` Finished\n");
	return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buff, size_t count,
	loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct store_block *cur_block = NULL;
	int retval = -ENOMEM;
	int target_block = 0, target_pos = 0;
	PDEBUG("`write` invoked\n");

	target_block = *f_pos / SCULL_BUFF_SIZE;
	target_pos = *f_pos % SCULL_BUFF_SIZE;

	PDEBUG("target block %d, Now, there are %d blocks in\n",
		target_block, dev->list_entry_counter);
	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;
	
	while (target_block >= dev->list_entry_counter) {
		cur_block = (struct store_block *)
			kmalloc(sizeof(*cur_block), GFP_KERNEL);
		PDEBUG("Create a new block!\n");
		if (!cur_block)
			goto out;
		memset(cur_block, 0, sizeof(*cur_block));
		INIT_LIST_HEAD(&cur_block->list);
		list_add_tail(&cur_block->list, &dev->list);
		dev->list_entry_counter++;
	}
	cur_block = list_last_entry(&dev->list, struct store_block, list);

		
	if (count > SCULL_BUFF_SIZE - target_pos)
		count = SCULL_BUFF_SIZE - target_pos;
	PDEBUG("SCULL_BUFF_SIZE = %d, target_pos = %d\n", SCULL_BUFF_SIZE, target_pos);
	PDEBUG("write %d bytes\n", count);

	if (copy_from_user(cur_block->data + target_pos, buff, count)) {
		retval = -EFAULT;
		goto out;
	}

	retval = count;
	cur_block->pos += count;
	*f_pos += count;

out:
	mutex_unlock(&dev->mutex);
	PDEBUG("Write finished\n");
	return retval;
}

void scull_trim(struct scull_dev *dev)
{
	struct store_block *cur = NULL, *tmp = NULL;
	PDEBUG("Start trim\n");
	list_for_each_entry_safe(cur, tmp, &dev->list, list) {
		list_del(&cur->list);
		PDEBUG("TrimA\n");
		memset(cur, 0, sizeof(*cur));
		PDEBUG("TrimB\n");
		kfree(cur);
		PDEBUG("TrimC\n");
	}
	dev->list_entry_counter = 0;
	PDEBUG("Finished Trim\n");
}

