// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/cleanup.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/minmax.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "main.h"

static unsigned long base = 0x200;
module_param(base, ulong, 0444);
MODULE_PARM_DESC(base, "Base I/O port of the ldd-io device (default 0x200)");

static int irq;		/* found by short_probe_irq() */

static dev_t short_devno;
static struct cdev short_cdev;

/* written by the interrupt handler, read by short_read() */
static atomic_t short_irq_probes = ATOMIC_INIT(0);
static u8 short_last;

/*
 * The interrupt handler. It runs in hard interrupt context, with
 * interrupts disabled on this CPU: it must be short and must not sleep.
 */
static irqreturn_t short_interrupt(int irqnr, void *dev_id)
{
	u8 status = inb(base + LDD_IO_STATUS);

	/* not raised by our device: let the kernel know */
	if (!(status & LDD_IO_STATUS_RX_READY))
		return IRQ_NONE;

	/* take the byte, then acknowledge it so the next one can come in */
	WRITE_ONCE(short_last, inb(base + LDD_IO_DATA));
	outb(LDD_IO_STATUS_RX_READY, base + LDD_IO_STATUS);

	atomic_inc(&short_irq_probes);
	return IRQ_HANDLED;
}

/* One line: the number of interrupts handled and the last byte received. */
static ssize_t short_read(struct file *filp, char __user *buf, size_t count,
			  loff_t *f_pos)
{
	char line[40];
	int len;

	len = scnprintf(line, sizeof(line), "irqs %d, last 0x%02x\n",
			atomic_read(&short_irq_probes), READ_ONCE(short_last));

	return simple_read_from_buffer(buf, count, f_pos, line, len);
}

/*
 * Send every byte to DATA. With LOOPBACK set the device also receives the
 * byte, sets RX_READY and raises an interrupt.
 */
static ssize_t short_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *f_pos)
{
	u8 *kbuf __free(kfree) = NULL;	/* freed on every return */

	count = min_t(size_t, count, SHORT_MAX_WRITE);

	kbuf = memdup_user(buf, count);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);	/* __free(kfree) skips error pointers */

	for (size_t i = 0; i < count; i++)
		outb(kbuf[i], base + LDD_IO_DATA);

	return count;
}

static const struct file_operations short_fops = {
	.owner = THIS_MODULE,
	.read  = short_read,
	.write = short_write,
};

/*
 * Find the interrupt line by making the device interrupt once.
 *
 * probe_irq_on() enables every unused interrupt line that may be probed and
 * returns a mask of them. probe_irq_off() reports which one has fired
 * since: the line number, 0 for none, or a negative number if several
 * fired. No handler is installed while probing.
 */
static int short_probe_irq(void)
{
	for (int tries = 0; tries < 5; tries++) {
		unsigned long mask;
		int found;

		/* drop a byte left over, so that RX_READY can go 0 -> 1 */
		while (inb(base + LDD_IO_STATUS) & LDD_IO_STATUS_RX_READY)
			outb(LDD_IO_STATUS_RX_READY, base + LDD_IO_STATUS);

		mask = probe_irq_on();		/* sleeps about 0.1 s */

		/* receive one byte through LOOPBACK: one interrupt pulse */
		outb(LDD_IO_CTRL_IRQ_EN | LDD_IO_CTRL_LOOPBACK,
		     base + LDD_IO_CTRL);
		outb(0, base + LDD_IO_DATA);
		udelay(5);			/* give it time to arrive */

		found = probe_irq_off(mask);

		/* quiet the device again */
		outb(0, base + LDD_IO_CTRL);
		outb(LDD_IO_STATUS_RX_READY, base + LDD_IO_STATUS);

		if (found > 0)
			return found;
		if (found == 0)
			pr_warn("probe: no interrupt seen\n");
		else
			pr_warn("probe: several interrupts seen\n");
	}

	return -ENODEV;
}

/*
 * The handler may run as soon as request_irq() returns and as long as the
 * device can interrupt. So everything it uses is set up before, the device
 * is told to interrupt last, and m_exit() undoes it in reverse order.
 */
static int __init m_init(void)
{
	int err;

	if (!request_region(base, LDD_IO_SIZE, MODULE_NAME)) {
		pr_err("cannot get I/O ports %#lx-%#lx\n",
		       base, base + LDD_IO_SIZE - 1);
		return -EBUSY;
	}

	irq = short_probe_irq();
	if (irq < 0) {
		pr_err("cannot find the interrupt line\n");
		err = irq;
		goto err_release;
	}
	pr_info("using IRQ %d\n", irq);

	/* dev_id identifies this handler; free_irq() must pass the same */
	err = request_irq(irq, short_interrupt, 0, MODULE_NAME, &short_cdev);
	if (err) {
		pr_err("cannot get IRQ %d: %d\n", irq, err);
		goto err_release;
	}

	err = alloc_chrdev_region(&short_devno, 0, 1, MODULE_NAME);
	if (err < 0) {
		pr_err("cannot get major number: %d\n", err);
		goto err_free_irq;
	}

	cdev_init(&short_cdev, &short_fops);
	short_cdev.owner = THIS_MODULE;

	err = cdev_add(&short_cdev, short_devno, 1);
	if (err) {
		pr_err("adding cdev failed: %d\n", err);
		goto err_unregister;
	}

	/* bytes written to DATA come back as input and raise an interrupt */
	outb(LDD_IO_CTRL_IRQ_EN | LDD_IO_CTRL_LOOPBACK, base + LDD_IO_CTRL);

	return 0;

err_unregister:
	unregister_chrdev_region(short_devno, 1);
err_free_irq:
	free_irq(irq, &short_cdev);
err_release:
	release_region(base, LDD_IO_SIZE);
	return err;
}

static void __exit m_exit(void)
{
	/* stop the device first, then the handler can go */
	outb(0, base + LDD_IO_CTRL);
	free_irq(irq, &short_cdev);	/* waits for a running handler */

	cdev_del(&short_cdev);
	unregister_chrdev_region(short_devno, 1);
	release_region(base, LDD_IO_SIZE);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Probing the interrupt line of the ldd-io device");
