#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include "global.h"

MODULE_AUTHOR("Douglas");
MODULE_LICENSE("GPL");

static int pipe_open(struct inode *, struct file*);
static ssize_t pipe_write(struct file *, const char __user *, size_t, loff_t *);
static ssize_t pipe_read(struct file *, char __user *, size_t, loff_t *);

static struct pipe_t *pipes[MAX_DEV_NR];
static int dev_major = 0, dev_minor = 0;

struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = pipe_open,
	.write = pipe_write,
	.read = pipe_read,
};

static int __init pipe_init(void)
{
	dev_t devno;
	int i = 0, err = 0;
	PDEBUG("\n\t-pipe advanced sleep- module is loaded!\n");

	err = alloc_chrdev_region(&devno, dev_minor, MAX_DEV_NR, "pipe");
	if (err < 0) {
		PDEBUG("Can't alloc device number!\n");
		return err;
	} else {
		dev_major = MAJOR(devno);
	}

	for (i = 0; i < MAX_DEV_NR; ++i) {
		pipes[i] = kmalloc(sizeof(struct pipe_t), GFP_KERNEL);
		memset(pipes[i], 0, sizeof(*(pipes[i])));

		mutex_init(&pipes[i]->mutex);
		init_waitqueue_head(&pipes[i]->read_q);
		init_waitqueue_head(&pipes[i]->write_q);
		cdev_init(&pipes[i]->dev, &fops);
		
		devno = MKDEV(dev_major, dev_minor + i);
		err = cdev_add(&pipes[i]->dev, devno, 1);
		if (err) {
			PDEBUG("Add new char device error!\n");
			kfree(pipes[i]);
		}
	}

	return 0;
}

static void __exit pipe_exit(void)
{
	int i = 0;
	dev_t devno;
	PDEBUG("\n\t-pipe advanced sleep- module is un-loaded!\n");

	for (i = 0; i < MAX_DEV_NR; ++i) {
		cdev_del(&pipes[i]->dev);
		kfree(pipes[i]);
	}

	devno = MKDEV(dev_major, dev_minor);
	unregister_chrdev_region(devno, MAX_DEV_NR);
}

module_init(pipe_init);
module_exit(pipe_exit);

static int pipe_open(struct inode *inode, struct file *filp)
{
	PDEBUG("`open` is invoked!\n");
	filp->private_data = container_of(inode->i_cdev, struct pipe_t, dev);

	return 0;
}

static ssize_t pipe_write(struct file *filp, const char __user *buff,
	size_t count, loff_t *f_pos)
{
	struct pipe_t *pipe = filp->private_data;

	PDEBUG("`write` is invoked!\n");

	if (mutex_lock_interruptible(&pipe->mutex)) {
		return -ERESTARTSYS;
	}

	while (pipe->valid_elements == MAX_BUFF_SIZE) {
		DEFINE_WAIT(wait);
		PDEBUG("Condition not suitable\n");

		mutex_unlock(&pipe->mutex);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("%s(%d) is going to sleep when write\n", current->comm,
			current->pid);

		prepare_to_wait(&pipe->write_q, &wait, TASK_INTERRUPTIBLE);
		if (pipe->valid_elements == MAX_BUFF_SIZE)
			schedule();
		finish_wait(&pipe->write_q, &wait);
		
		if (signal_pending(current))
			return -ERESTARTSYS;
		if (mutex_lock_interruptible(&pipe->mutex))
			return -ERESTARTSYS;
	}

	if (pipe->write_pos >= pipe->read_pos)
		count = min(count, (size_t)(MAX_BUFF_SIZE - pipe->write_pos));
	else
		count = min(count, (size_t)(pipe->read_pos - pipe->write_pos));

	PDEBUG("Prepare to write %li bytes into internel buff\n", count);
	if (copy_from_user(pipe->buff + pipe->write_pos, buff, count)) {
		mutex_unlock(&pipe->mutex);
		return -EFAULT;
	}

	pipe->write_pos += count;
	if (pipe->write_pos == MAX_BUFF_SIZE)
		pipe->write_pos = 0;
	pipe->valid_elements += count;

	PDEBUG("`write` finished! r_pos = %d, w_pos = %d\n", pipe->read_pos,
		pipe->write_pos);
	mutex_unlock(&pipe->mutex);

	wake_up_interruptible(&pipe->read_q);

	return count;
}

static ssize_t pipe_read(struct file *filp, char __user *buff, size_t count,
	loff_t *f_pos)
{
	struct pipe_t *pipe = filp->private_data;
	PDEBUG("`read` is invoked!\n");

	while (pipe->valid_elements == 0) {
		DEFINE_WAIT(wait);

		mutex_unlock(&pipe->mutex);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("%s(%d) is going to sleep when write\n", current->comm,
			current->pid);

		prepare_to_wait(&pipe->read_q, &wait, TASK_INTERRUPTIBLE);
		if (pipe->valid_elements == 0)
			schedule();
		finish_wait(&pipe->read_q, &wait);

		if (signal_pending(current))
			return -ERESTARTSYS;
		if (mutex_lock_interruptible(&pipe->mutex))
			return -ERESTARTSYS;
	}

	PDEBUG("Originally need to read %li bytes!\n", count);
	if (pipe->write_pos > pipe->read_pos)
		count = min(count, (size_t)(pipe->write_pos - pipe->read_pos));
	else
		count = min(count, (size_t)(MAX_BUFF_SIZE - pipe->read_pos));
	PDEBUG("Going to read %li bytes\n", count);

	if (copy_to_user(buff, pipe->buff + pipe->read_pos, count)) {
		mutex_unlock(&pipe->mutex);
		return -EFAULT;
	}

	pipe->read_pos += count;
	if (pipe->read_pos == MAX_BUFF_SIZE)
		pipe->read_pos = 0;
	pipe->valid_elements -= count;

	PDEBUG("`read` finished!\n");

	mutex_unlock(&pipe->mutex);

	wake_up_interruptible(&pipe->write_q);

	return count;
}
