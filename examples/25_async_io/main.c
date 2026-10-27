// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/minmax.h>
#include <linux/slab.h>
#include <linux/uio.h>
#include <linux/workqueue.h>

#include "main.h"

static struct async_io_dev *async_io_devs;
static dev_t devno;
static struct workqueue_struct *async_wq;

static int async_io_open(struct inode *inode, struct file *filp)
{
	filp->private_data = container_of(inode->i_cdev, struct async_io_dev,
					  cdev);
	return 0;
}

/* Take as much as there is, up to the end of the buffer, into @to. */
static ssize_t async_io_do_read(struct async_io_dev *dev, struct iov_iter *to)
{
	size_t want, done;
	ssize_t ret;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	want = min_t(size_t, BUFF_SIZE - dev->read_cur, dev->len);
	want = min(want, iov_iter_count(to));
	done = copy_to_iter(dev->buff + dev->read_cur, want, to);
	if (want && !done) {
		ret = -EFAULT;
		goto out;
	}

	dev->read_cur = (dev->read_cur + done) % BUFF_SIZE;
	dev->len -= done;
	ret = done;
	pr_debug("read %zu, %d left\n", done, dev->len);
out:
	mutex_unlock(&dev->mutex);
	return ret;
}

/* Store as much of @from as fits, up to the end of the buffer. */
static ssize_t async_io_do_write(struct async_io_dev *dev,
				 struct iov_iter *from)
{
	size_t want, done;
	ssize_t ret;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	if (dev->len == BUFF_SIZE) {
		ret = -ENOSPC;
		goto out;
	}

	want = min_t(size_t, BUFF_SIZE - dev->write_cur, BUFF_SIZE - dev->len);
	want = min(want, iov_iter_count(from));
	done = copy_from_iter(dev->buff + dev->write_cur, want, from);
	if (want && !done) {
		ret = -EFAULT;
		goto out;
	}

	dev->write_cur = (dev->write_cur + done) % BUFF_SIZE;
	dev->len += done;
	ret = done;
	pr_debug("wrote %zu, %d in buffer\n", done, dev->len);
out:
	mutex_unlock(&dev->mutex);
	return ret;
}

/*
 * Asynchronous requests. The data is copied when the request is submitted,
 * while the iov_iter is still valid and we run in the submitter's context:
 * a worker runs in a kernel thread, without the user's memory, and the
 * iov_iter lives on the submitter's stack. Only the completion is deferred,
 * as if a device took ASYNC_DELAY_MS to do the transfer; a real driver
 * would call ki_complete() from its interrupt handler instead.
 */
struct async_work {
	struct delayed_work work;
	struct kiocb *iocb;
	ssize_t result;
};

static void async_io_complete(struct work_struct *work)
{
	struct async_work *aw = container_of(to_delayed_work(work),
					     struct async_work, work);

	pr_debug("complete: %zd\n", aw->result);
	/* hand the result to whoever submitted it: AIO or io_uring */
	aw->iocb->ki_complete(aw->iocb, aw->result);
	kfree(aw);
}

static ssize_t async_io_defer(struct kiocb *iocb, ssize_t result)
{
	struct async_work *aw;

	aw = kmalloc_obj(*aw);
	if (!aw)
		return result;	/* complete now; the caller reports it */

	aw->iocb = iocb;
	aw->result = result;
	INIT_DELAYED_WORK(&aw->work, async_io_complete);
	queue_delayed_work(async_wq, &aw->work,
			   msecs_to_jiffies(ASYNC_DELAY_MS));

	/* a promise: ki_complete() will be called later */
	return -EIOCBQUEUED;
}

/*
 * read(2) comes here too, with a synchronous kiocb; io_submit(2) and
 * io_uring bring asynchronous ones, which have ki_complete set.
 */
static ssize_t async_io_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	ssize_t ret = async_io_do_read(iocb->ki_filp->private_data, to);

	if (is_sync_kiocb(iocb) || ret < 0)
		return ret;
	return async_io_defer(iocb, ret);
}

static ssize_t async_io_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	ssize_t ret = async_io_do_write(iocb->ki_filp->private_data, from);

	if (is_sync_kiocb(iocb) || ret < 0)
		return ret;
	return async_io_defer(iocb, ret);
}

/*
 * Only the _iter methods: .read and .write are the older interface, and
 * when both exist read(2) and write(2) use .read and .write.
 */
static const struct file_operations async_io_fops = {
	.owner      = THIS_MODULE,
	.open       = async_io_open,
	.read_iter  = async_io_read_iter,
	.write_iter = async_io_write_iter,
};

static int __init m_init(void)
{
	int i, err;

	/*
	 * A workqueue of our own, so that unloading can wait for the last
	 * completion to return. A pending request holds its file, and the
	 * file the module, but the worker still runs module code afterwards.
	 */
	async_wq = alloc_workqueue(MODULE_NAME, WQ_UNBOUND, 0);
	if (!async_wq)
		return -ENOMEM;

	async_io_devs = kzalloc_objs(*async_io_devs, DEV_NR);
	if (!async_io_devs) {
		err = -ENOMEM;
		goto err_wq;
	}

	err = alloc_chrdev_region(&devno, 0, DEV_NR, MODULE_NAME);
	if (err)
		goto err_free;

	for (i = 0; i < DEV_NR; i++) {
		mutex_init(&async_io_devs[i].mutex);
		cdev_init(&async_io_devs[i].cdev, &async_io_fops);
		async_io_devs[i].cdev.owner = THIS_MODULE;
		err = cdev_add(&async_io_devs[i].cdev, devno + i, 1);
		if (err)
			goto err_cdev;
	}

	return 0;

err_cdev:
	while (i--)
		cdev_del(&async_io_devs[i].cdev);
	unregister_chrdev_region(devno, DEV_NR);
err_free:
	kfree(async_io_devs);
err_wq:
	destroy_workqueue(async_wq);
	return err;
}

static void __exit m_exit(void)
{
	int i;

	for (i = 0; i < DEV_NR; i++)
		cdev_del(&async_io_devs[i].cdev);
	unregister_chrdev_region(devno, DEV_NR);
	kfree(async_io_devs);
	destroy_workqueue(async_wq);	/* waits for running completions */
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Asynchronous I/O with ki_complete()");
