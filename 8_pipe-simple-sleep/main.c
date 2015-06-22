#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#include "global.h"

static int pipe_open(struct inode *, struct file *);
static ssize_t pipe_write(struct file *, const char __user *, size_t, loff_t *);
static ssize_t pipe_read(struct file *, char __user *, size_t, loff_t *);

static struct buff_t *pipe_buff[PIPE_DEV_NR];
static int pipe_major = 0, pipe_minor = 0;

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = pipe_open,
	.write = pipe_write,
	.read = pipe_read,
};

static int __init pipe_init(void)
{
	dev_t devno;
	int err = 0, i = 0;

	PDEBUG("\nModule loaded!\n");

	err = alloc_chrdev_region(&devno, pipe_minor, PIPE_DEV_NR, "pipe");
	if (err < 0) {
		PDEBUG("Can't get devno major\n");
		return err;
	} else {
		pipe_major = MAJOR(devno);
	}

	for (i = 0; i < PIPE_DEV_NR; ++i) {
		pipe_buff[i] = kmalloc(sizeof (struct buff_t), GFP_KERNEL);
		if (!pipe_buff[i]) {
			PDEBUG("kmalloc error when i = %d\n", i);
			continue;
		}
		memset(pipe_buff[i], 0, sizeof (struct buff_t));

		mutex_init(&pipe_buff[i]->mutex);
		init_waitqueue_head(&pipe_buff[i]->read_queue);
		init_waitqueue_head(&pipe_buff[i]->write_queue);
		cdev_init(&pipe_buff[i]->dev, &fops);
		pipe_buff[i]->dev.owner = THIS_MODULE;

		devno = MKDEV(pipe_major, pipe_minor + i);
		err = cdev_add(&pipe_buff[i]->dev, devno, 1);
		if (err) {
			PDEBUG("add dev error at i = %d\n", i);
			kfree(pipe_buff[i]);
		}
	}

	return 0;
}

static void __exit pipe_exit(void)
{
	dev_t devno;
	int i = 0;

	PDEBUG("Module un-loaded!\n");

	for (i = 0; i < PIPE_DEV_NR; ++i) {
		cdev_del(&pipe_buff[i]->dev);
		kfree(pipe_buff[i]);
	}

	devno = MKDEV(pipe_major, pipe_minor);
	unregister_chrdev_region(devno, PIPE_DEV_NR);
}

module_init(pipe_init);
module_exit(pipe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Douglas");

static int pipe_open(struct inode *inode, struct file *file)
{
	PDEBUG("`open` is invoked!\n");
	file->private_data = container_of(inode->i_cdev, struct buff_t, dev);

	return 0;
}

static ssize_t pipe_write(struct file *file, const char __user * buff,
			  size_t count, loff_t * f_pos)
{
	struct buff_t *pipe = file->private_data;
	if (mutex_lock_interruptible(&pipe->mutex))
		return -ERESTARTSYS;

	while (pipe->size == BUFF_SIZE) {
		mutex_unlock(&pipe->mutex);
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		PDEBUG("%s(%d) is going to sleep when writing\n", current->comm,
		       current->pid);
		if (wait_event_interruptible
		    (pipe->write_queue, (pipe->size != BUFF_SIZE)))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&pipe->mutex))
			return -ERESTARTSYS;
	}

	if (pipe->write_pos >= pipe->read_pos)
		count = min(count, (size_t) (BUFF_SIZE - pipe->write_pos));
	else
		count = min(count, (size_t) (pipe->read_pos - pipe->write_pos));

	PDEBUG("Prepare to write %li bytes\n", count);
	if (copy_from_user(pipe->buff + pipe->write_pos, buff, count)) {
		mutex_unlock(&pipe->mutex);
		return -EFAULT;
	}

	pipe->write_pos += count;
	pipe->size += count;
	if (pipe->write_pos == BUFF_SIZE)
		pipe->write_pos = 0;

	mutex_unlock(&pipe->mutex);
	wake_up_interruptible(&pipe->read_queue);

	PDEBUG("`write` finished, r_pos = %d, w_pos = %d\n", pipe->read_pos,
	       pipe->write_pos);

	return count;
}

static ssize_t pipe_read(struct file *file, char __user * buff, size_t count,
			 loff_t * f_pos)
{
	struct buff_t *pipe = file->private_data;

	if (mutex_lock_interruptible(&pipe->mutex))
		return -ERESTARTSYS;

	while (pipe->size == 0) {
		mutex_unlock(&pipe->mutex);

		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		PDEBUG("%s(%d) is going to sleep when reading\n", current->comm,
		       current->pid);
		if (wait_event_interruptible
		    (pipe->read_queue, (pipe->size != 0)))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&pipe->mutex))
			return -ERESTARTSYS;
	}

	if (pipe->read_pos >= pipe->write_pos)
		count = min(count, (size_t) (BUFF_SIZE - pipe->read_pos));
	else
		count = min(count, (size_t) (pipe->write_pos - pipe->read_pos));

	PDEBUG("%li bytes will read\n", count);
	if (copy_to_user(buff, pipe->buff + pipe->read_pos, count)) {
		mutex_unlock(&pipe->mutex);
		return -EFAULT;
	}

	pipe->read_pos += count;
	pipe->size -= count;
	if (pipe->read_pos == BUFF_SIZE)
		pipe->read_pos = 0;

	mutex_unlock(&pipe->mutex);
	wake_up_interruptible(&pipe->write_queue);

	PDEBUG("read finished\n");

	return count;
}
