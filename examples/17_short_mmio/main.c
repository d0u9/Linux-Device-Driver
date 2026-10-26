// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/cleanup.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kstrtox.h>
#include <linux/ioport.h>
#include <linux/minmax.h>
#include <linux/mutex.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unaligned.h>

#include "main.h"

static unsigned long base = 0xff000000;
module_param(base, ulong, 0444);
MODULE_PARM_DESC(base, "Physical address of the ldd-io device (default 0xff000000)");

/*
 * How a write of all eight matrix rows reaches the device. The panel is
 * redrawn after every write access, so this decides how many redraws one
 * picture takes. Writable at run time:
 *   echo 1 > /sys/module/short_mmio/parameters/width
 */
static int width = 8;

static int width_set(const char *val, const struct kernel_param *kp)
{
	int w, err;

	err = kstrtoint(val, 0, &w);
	if (err)
		return err;
	if (w != 0 && w != 1 && w != 2 && w != 4 && w != 8)
		return -EINVAL;

	WRITE_ONCE(width, w);
	return 0;
}

static const struct kernel_param_ops width_ops = {
	.set = width_set,
	.get = param_get_int,
};
module_param_cb(width, &width_ops, &width, 0644);
MODULE_PARM_DESC(width, "Full matrix write: 8, 4, 2 or 1 byte accesses, 0 = memcpy_toio (default 8)");

/*
 * The registers, mapped uncached into kernel virtual memory by ioremap().
 * Unlike an ioport_map() cookie this is a real address: ioread8() and
 * friends turn into plain loads and stores on it.
 */
static void __iomem *io;

static dev_t short_devno;
static struct cdev short_cdev;
static DEFINE_MUTEX(short_lock);	/* one register sequence at a time */

/* ---- /dev/short_mmio_data: LEDs out, polled bytes in ---- */

/* Take the bytes that are ready, up to count. Called with short_lock held. */
static size_t data_take(u8 *kbuf, size_t count)
{
	size_t n = 0;

	while (n < count &&
	       (ioread8(io + LDD_IO_STATUS) & LDD_IO_STATUS_RX_READY)) {
		/* read DATA before acknowledging, see ldd-io.md */
		kbuf[n++] = ioread8(io + LDD_IO_DATA);
		iowrite8(LDD_IO_STATUS_RX_READY, io + LDD_IO_STATUS);
	}

	return n;
}

/*
 * Receive up to count bytes. Waits for the first byte by polling STATUS,
 * then returns what is ready without waiting again.
 */
static ssize_t data_read(struct file *filp, char __user *buf, size_t count,
			 loff_t *f_pos)
{
	u8 kbuf[64];
	size_t n = 0;
	u8 status;

	if (count == 0)
		return 0;
	count = min(count, sizeof(kbuf));

	for (;;) {
		scoped_cond_guard(mutex_intr, return -ERESTARTSYS, &short_lock)
			n = data_take(kbuf, count);

		if (n)
			break;
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		/*
		 * Wait for RX_READY: read STATUS at most SHORT_POLL_US apart, give
		 * up after SHORT_POLL_SLICE_US so that a signal can stop the
		 * reader. readx_poll_timeout() cannot be interrupted itself.
		 * STATUS is read without short_lock; reading it has no side
		 * effect, and the bytes are taken under the lock above.
		 */
		readx_poll_timeout(ioread8, io + LDD_IO_STATUS, status,
				   status & LDD_IO_STATUS_RX_READY,
				   SHORT_POLL_US, SHORT_POLL_SLICE_US);
		if (signal_pending(current))
			return -ERESTARTSYS;
	}

	if (copy_to_user(buf, kbuf, n))
		return -EFAULT;
	return n;
}

/*
 * Send every byte to DATA. iowrite8_rep() writes the whole buffer to the
 * same register, one byte access each; the LEDs end up showing the last
 * byte. memcpy_toio() would be wrong here: it walks forward through
 * consecutive addresses.
 */
static ssize_t data_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *f_pos)
{
	u8 *kbuf __free(kfree) = NULL;	/* freed on every return */

	count = min_t(size_t, count, SHORT_MAX_WRITE);

	/* allocates and copies in one call */
	kbuf = memdup_user(buf, count);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);	/* __free(kfree) skips error pointers */

	scoped_cond_guard(mutex_intr, return -ERESTARTSYS, &short_lock)
		iowrite8_rep(io + LDD_IO_DATA, kbuf, count);

	return count;
}

static int data_open(struct inode *inode, struct file *filp)
{
	/* a byte stream: no position, no seeking */
	return stream_open(inode, filp);
}

static const struct file_operations data_fops = {
	.owner = THIS_MODULE,
	.open  = data_open,
	.read  = data_read,
	.write = data_write,
};

/* ---- /dev/short_mmio_matrix: 8 bytes of LED rows, seekable ---- */

static ssize_t matrix_read(struct file *filp, char __user *buf, size_t count,
			   loff_t *f_pos)
{
	u8 rows[LDD_IO_MATRIX_ROWS];
	loff_t pos = *f_pos;

	if (pos < 0)
		return -EINVAL;
	if (pos >= LDD_IO_MATRIX_ROWS)
		return 0;
	count = min_t(size_t, count, LDD_IO_MATRIX_ROWS - pos);

	scoped_cond_guard(mutex_intr, return -ERESTARTSYS, &short_lock) {
		if (pos == 0 && count == LDD_IO_MATRIX_ROWS) {
			/* the whole picture in one 64-bit read */
			put_unaligned_le64(readq(io + LDD_IO_MATRIX), rows);
		} else {
			for (size_t i = 0; i < count; i++)
				rows[i] = ioread8(io + LDD_IO_MATRIX + pos + i);
		}
	}

	if (copy_to_user(buf, rows, count))
		return -EFAULT;
	*f_pos = pos + count;
	return count;
}

/*
 * Write a whole picture with the access width chosen by the width
 * parameter. The accessors are little endian, like ldd-io, and the
 * get_unaligned_le*() helpers read the rows as little endian, so rows[0]
 * always lands at offset 8, the top row.
 */
static void matrix_put(const u8 *rows)
{
	void __iomem *m = io + LDD_IO_MATRIX;

	switch (READ_ONCE(width)) {
	case 8:		/* one access, one redraw */
		writeq(get_unaligned_le64(rows), m);
		break;
	case 4:
		for (int i = 0; i < LDD_IO_MATRIX_ROWS; i += 4)
			iowrite32(get_unaligned_le32(rows + i), m + i);
		break;
	case 2:
		for (int i = 0; i < LDD_IO_MATRIX_ROWS; i += 2)
			iowrite16(get_unaligned_le16(rows + i), m + i);
		break;
	case 1:
		for (int i = 0; i < LDD_IO_MATRIX_ROWS; i++)
			iowrite8(rows[i], m + i);
		break;
	default:	/* 0: the widths are up to the architecture */
		memcpy_toio(m, rows, LDD_IO_MATRIX_ROWS);
		break;
	}
}

static ssize_t matrix_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *f_pos)
{
	u8 rows[LDD_IO_MATRIX_ROWS];
	loff_t pos = *f_pos;

	if (pos < 0)
		return -EINVAL;
	if (pos >= LDD_IO_MATRIX_ROWS)
		return -ENOSPC;
	count = min_t(size_t, count, LDD_IO_MATRIX_ROWS - pos);

	if (copy_from_user(rows, buf, count))
		return -EFAULT;

	scoped_cond_guard(mutex_intr, return -ERESTARTSYS, &short_lock) {
		if (pos == 0 && count == LDD_IO_MATRIX_ROWS) {
			matrix_put(rows);
		} else {
			for (size_t i = 0; i < count; i++)
				iowrite8(rows[i], io + LDD_IO_MATRIX + pos + i);
		}
	}

	*f_pos = pos + count;
	return count;
}

static loff_t matrix_llseek(struct file *filp, loff_t off, int whence)
{
	return fixed_size_llseek(filp, off, whence, LDD_IO_MATRIX_ROWS);
}

static const struct file_operations matrix_fops = {
	.owner  = THIS_MODULE,
	.read   = matrix_read,
	.write  = matrix_write,
	.llseek = matrix_llseek,
};

/* ---- one cdev, one set of file operations per minor ---- */

static const struct file_operations *const short_minor_fops[SHORT_NR_MINORS] = {
	[SHORT_DATA]   = &data_fops,
	[SHORT_MATRIX] = &matrix_fops,
};

/* On open, install the file operations of the minor in the file. */
static int short_open(struct inode *inode, struct file *filp)
{
	unsigned int minor = iminor(inode);

	if (minor >= SHORT_NR_MINORS)
		return -ENXIO;

	/* same module, so the reference taken for short_fops covers it */
	filp->f_op = short_minor_fops[minor];
	if (filp->f_op->open)
		return filp->f_op->open(inode, filp);

	return 0;
}

static const struct file_operations short_fops = {
	.owner = THIS_MODULE,
	.open  = short_open,
};

static int __init m_init(void)
{
	int err;

	/* claim the range first, it shows up in /proc/iomem */
	if (!request_mem_region(base, LDD_IO_SIZE, MODULE_NAME)) {
		pr_err("cannot get I/O memory %#lx-%#lx\n",
		       base, base + LDD_IO_SIZE - 1);
		return -EBUSY;
	}

	io = ioremap(base, LDD_IO_SIZE);
	if (!io) {
		pr_err("cannot map I/O memory %#lx\n", base);
		err = -ENOMEM;
		goto err_release;
	}

	err = alloc_chrdev_region(&short_devno, 0, SHORT_NR_MINORS,
				  MODULE_NAME);
	if (err < 0) {
		pr_err("cannot get major number: %d\n", err);
		goto err_unmap;
	}

	cdev_init(&short_cdev, &short_fops);
	short_cdev.owner = THIS_MODULE;

	err = cdev_add(&short_cdev, short_devno, SHORT_NR_MINORS);
	if (err) {
		pr_err("adding cdev failed: %d\n", err);
		goto err_unregister;
	}

	return 0;

err_unregister:
	unregister_chrdev_region(short_devno, SHORT_NR_MINORS);
err_unmap:
	iounmap(io);
err_release:
	release_mem_region(base, LDD_IO_SIZE);
	return err;
}

static void __exit m_exit(void)
{
	cdev_del(&short_cdev);
	unregister_chrdev_region(short_devno, SHORT_NR_MINORS);
	iounmap(io);
	release_mem_region(base, LDD_IO_SIZE);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Memory-mapped I/O access to the ldd-io device");
