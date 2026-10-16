#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <asm/io.h>

#include "main.h"

static int major = 0;
static int short_irq = 6;

static unsigned long base = 0x200;
unsigned long short_base = 0;
struct short_dev *dev;

int short_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int short_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t short_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	unsigned long flags;
	int val;
	spin_lock_irqsave(&dev->lock, flags);
	val = dev->cnt;
	spin_unlock_irqrestore(&dev->lock, flags);

	if (copy_to_user(buf, &val, sizeof(val))) {
		return -EFAULT;
	}

	return count;
}

ssize_t short_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	unsigned long port = short_base;
	unsigned char *kbuf, *ptr;
	size_t cnt = count;

	kbuf = kmalloc(cnt, GFP_KERNEL);

	if (!kbuf)
		return -ENOMEM;
	if (copy_from_user(kbuf, buf, cnt))
		return -EFAULT;

	ptr = kbuf;

	while (cnt--) {
		pr_debug("port=%lu(%#lx), val=%d(%#x)\n", port, port, *ptr, *ptr);
		outb(*(ptr++), port);
		wmb();
	}

	kfree(kbuf);

	return count;
}

static struct file_operations fops = {
	.owner	 = THIS_MODULE,
	.read	 = short_read,
	.write	 = short_write,
	.open	 = short_open,
	.release = short_release,
};

irqreturn_t irq_service(int irq, void *dev_id)
{
	struct short_dev *dev = (struct short_dev*)(dev_id);
	unsigned long flags;

	if (short_irq != irq)
		short_irq = -irq;

	spin_lock_irqsave(&dev->lock, flags);
	dev->cnt++;
	pr_debug("IRQ triggerd %d times\n", dev->cnt);
	spin_unlock_irqrestore(&dev->lock, flags);

	return IRQ_HANDLED;
}

static
int __init m_init(void)
{
	int result = 0;
	short_base = base;

	dev = kzalloc(sizeof(struct short_dev), GFP_KERNEL);
	if (!dev) {
		pr_err("Allocate device descriptor failed!\n");
		return -ENOMEM;
	}
	spin_lock_init(&dev->lock);

	if (!request_region(short_base, SHORT_NR_PORTS, MODULE_NAME)) {
		pr_err("short_ioport: cannot get I/O port address %#lx\n",
		       short_base);
		result = -ENODEV;
		goto out;
	}

	if (short_irq >= 0) {
		result = request_irq(short_irq, irq_service, 0,
				     MODULE_NAME, dev);
		if (result) {
			pr_err("Request irq %d failed\n", short_irq);
			short_irq = -1;
			result = -ENODEV;
			goto unreg_region;
		}

		// enable interrupt
		outb(1, short_base + 1);
		wmb();
	}

	result = register_chrdev(major, MODULE_NAME, &fops);
	if (result < 0) {
		pr_err("cannot get major number!\n");
		goto unreg_irq;
	}
	major = (major == 0) ? result : major;

	return 0;

unreg_irq:
	free_irq(short_irq, NULL);

unreg_region:
	release_region(short_base, SHORT_NR_PORTS);

out:
	kfree(dev);
	return result;
}

static
void __exit m_exit(void)
{
	outb(0, short_base + 1);
	wmb();
	unregister_chrdev(major, MODULE_NAME);
	free_irq(short_irq, dev);
	release_region(short_base, SHORT_NR_PORTS);
	kfree(dev);
}


module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Interrupt handle");
