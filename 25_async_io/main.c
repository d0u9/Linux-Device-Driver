#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/uio.h>

#include "main.h"

struct async_io_dev *async_io_devs;
int major;

int async_io_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);

	filp->private_data = container_of(inode->i_cdev, struct async_io_dev, cdev);
	return 0;
}

static
int async_io_release(struct inode *inode, struct file *filp)
{
	// remove the async_queue from the file
	// struct async_io_dev *dev = filp->private_data;

	return 0;
}

static
ssize_t async_io_read(struct file *filp, char __user *buff,
			  size_t count, loff_t *f_pos)
{
	struct async_io_dev *dev = filp->private_data;
	ssize_t len, ret;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	if (dev->len == 0) {
		ret = 0;
		goto out;
	}

	len = min(BUFF_SIZE - dev->read_cur, dev->len);
	len = min((size_t)len, count);

	if (copy_to_user(buff, dev->buff + dev->read_cur, len)) {
		ret = -EFAULT;
		goto out;
	}

	dev->read_cur = (dev->read_cur + len) % BUFF_SIZE;
	dev->len -= len;
	ret = len;

	pr_debug("Read: write_cur=%d, read_cur=%d, len=%d, ret=%ld\n",
		 dev->write_cur, dev->read_cur, dev->len, len);

out:
	mutex_unlock(&dev->mutex);
	return ret;
}

static
ssize_t async_io_write(struct file *filp, const char __user *buff,
			   size_t count, loff_t *f_pos)
{
	struct async_io_dev *dev = filp->private_data;
	ssize_t len, ret;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	if (dev->len == BUFF_SIZE) {
		ret = 0;
		goto out;
	}

	len = min(BUFF_SIZE - dev->write_cur, BUFF_SIZE - dev->len);
	len = min((size_t)len, count);

	if (copy_from_user(dev->buff + dev->write_cur, buff, len)) {
		ret = -EFAULT;
		goto out;
	}

	dev->write_cur = (dev->write_cur + len) % BUFF_SIZE;
	dev->len += len;
	ret = len;

	pr_debug("Write: write_cur=%d, read_cur=%d, len=%d, ret=%ld\n",
		 dev->write_cur, dev->read_cur, dev->len, len);

out:
	mutex_unlock(&dev->mutex);
	return ret;
}

struct async_work {
	struct delayed_work work;
	struct kiocb *iocb;
	struct iov_iter *tofrom;
};

static
void async_do_deferred_op(struct work_struct *work)
{
	struct async_work *stuff = container_of(work, struct async_work, work.work);
	if (iov_iter_rw(stuff->tofrom) == WRITE) {
		generic_file_write_iter(stuff->iocb, stuff->tofrom);
	} else {
		generic_file_read_iter(stuff->iocb, stuff->tofrom);
	}
	kfree(stuff);
}

static
int async_defer_op(struct kiocb *iocb, struct iov_iter *tofrom)
{
	struct async_work *stuff;
	int result;

	/* Otherwise defer the completion for a few milliseconds. */
	stuff = kmalloc (sizeof (*stuff), GFP_KERNEL);
	if (stuff == NULL)
		return result; /* No memory, just complete now */

	stuff->iocb = iocb;
	INIT_DELAYED_WORK(&stuff->work, async_do_deferred_op);
	schedule_delayed_work(&stuff->work, HZ/10000);

	return -EIOCBQUEUED;
}

static
ssize_t async_io_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	if (is_sync_kiocb(iocb))
		return generic_file_read_iter(iocb, to);

	return async_defer_op(iocb, to);
}

static
ssize_t async_io_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	if (is_sync_kiocb(iocb))
		return generic_file_write_iter(iocb, from);

	return async_defer_op(iocb, from);
}

static struct file_operations fops = {
	.open	 = async_io_open,
	.release = async_io_release,
	.read    = async_io_read,
	.write   = async_io_write,
	.read_iter  = async_io_read_iter,
	.write_iter = async_io_write_iter,
};

static
int __init m_init(void)
{
	dev_t devno;
	int i, err = 0;

	pr_debug(MODULE_NAME " is loaded\n");

	async_io_devs = kcalloc(DEV_NR, sizeof(struct async_io_dev), GFP_KERNEL);
	if (!async_io_devs) {
		pr_debug("Cannot alloc memory!\n");
		return -ENOMEM;
	}

	err = alloc_chrdev_region(&devno, 0, DEV_NR, MODULE_NAME);
	if (err < 0) {
		pr_debug("Can't get major!\n");
		goto err_major;
	}
	major = MAJOR(devno);

	for (i = 0; i < DEV_NR; i++) {
		cdev_init(&async_io_devs[i].cdev, &fops);
		async_io_devs[i].cdev.owner = THIS_MODULE;
		mutex_init(&async_io_devs[i].mutex);
		devno = MKDEV(major, i);
		err = cdev_add(&async_io_devs[i].cdev, devno, 1);
		if (err) {
			pr_err("Error when adding new device\n");
			goto err_add;
		}
	}

	return 0;

err_add:
	for (; i > 0; i--) {
		cdev_del(&async_io_devs[i].cdev);
	}
err_major:
	kfree(async_io_devs);
	return err;
}

static
void __exit m_exit(void)
{
	int i;
	dev_t devno;

	pr_debug(MODULE_NAME " unloaded\n");

	for (i = 0; i < DEV_NR; i++) {
		cdev_del(&async_io_devs[i].cdev);
	}

	devno = MKDEV(major, 0);
	unregister_chrdev_region(devno, DEV_NR);

	kfree(async_io_devs);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Asynchronous I/O");

