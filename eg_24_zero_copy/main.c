#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/mm_types.h>

#include "main.h"
#include "ioc_cmd.h"


static int major;
static struct zero_copy_dev zdev;

int zero_open(struct inode *inode, struct file *filp)
{
	struct zero_copy_ctx *ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	filp->private_data = ctx;

	init_waitqueue_head(&ctx->get_queue);
	init_waitqueue_head(&ctx->put_queue);

	return 0;
}

int zero_release(struct inode *inode, struct file *filp)
{
	int i;
	struct zero_copy_ctx *ctx = filp->private_data;

	for (i = 0; i < ctx->pagenr; i++) {
		if (ctx->pages[i]) {
			put_page(ctx->pages[i]);
		}
	}

	kfree(ctx);
	return 0;
}

long ioc_open(struct file *filp, struct ioc_msg *__user arg)
{
	int i, ret = 0, pinned, npages;
	struct page **pages;
	struct zero_copy_ctx *ctx = filp->private_data;
	struct ioc_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (copy_from_user(msg, arg, sizeof(*msg))) {
	    pr_debug("Error copy from userspace\n");
	    return -EFAULT;
	}

	npages = msg->len >> PAGE_SHIFT;
	pages = kmalloc_array(npages, sizeof(*pages), GFP_KERNEL);
	pinned = get_user_pages_fast(msg->addr, npages, FOLL_WRITE, pages);
	if (pinned < 0) {
		ret = pinned;
		pr_debug("pinned < 0\n");
		goto out;
	} else if (pinned != npages) {
		ret = -EFAULT;
		goto put_pages;
	}
	ctx->pages = pages;
	ctx->pagenr = pinned;
	ctx->addr = msg->addr;
	pr_debug("addr=%#lx, len=%#lx, pagenr=%d\n", msg->addr, msg->len, pinned);

	kfree(msg);
	return 0;

put_pages:
	for (i = 0; i < pinned; i++) {
		if (pages[i])
			put_page(pages[i]);
	}
out:
	kfree(pages);
	kfree(msg);
	return ret;
}

long ioc_get(struct file *filp, unsigned long intval)
{
	int i;
	void *addr;
	u32 val = (u32)intval;
	struct zero_copy_ctx *ctx = filp->private_data;

	for (i = 0; i < ctx->pagenr; i++) {
		addr = page_to_virt(ctx->pages[i]);
		memset32(addr, val, PAGE_SIZE / 4);
	}

	return 0;
}

long ioc_put(struct file *filp)
{
	int i;
	void *addr;
	struct zero_copy_ctx *ctx = filp->private_data;

	for (i = 0; i < ctx->pagenr; i++) {
		addr = page_to_virt(ctx->pages[i]);
		pr_debug("%#x\n", *((u32*)addr));
	}

	return 0;	
}

long zero_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) != IOCTL_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case IOCTL_OPEN:
		pr_debug("IOCTL_OPEN\n");
		return ioc_open(filp, (struct ioc_msg *__user)arg);
	case IOCTL_GET:
		pr_debug("IOCTL_GET\n");
		return ioc_get(filp, arg);
	case IOCTL_PUT:
		pr_debug("IOCTL_PUT\n");
		return ioc_put(filp);
	default:
		return -ENOTTY;
	}

	return 0;
}

static struct file_operations fops = {
	.owner   = THIS_MODULE,
	.open    = zero_open,
	.release = zero_release,
	.unlocked_ioctl = zero_ioctl,
};

static
int __init m_init(void)
{
	int result;
	dev_t devno;

	result = alloc_chrdev_region(&devno, 0, DEV_NR, MODULE_NAME);
	if (result < 0) {
		pr_err("Unable to get major devno\n");
		return result;
	}

	major = MAJOR(devno);
	devno = MKDEV(major, 0);
	cdev_init(&zdev.cdev, &fops);
	zdev.cdev.owner = THIS_MODULE;
	if (cdev_add(&zdev.cdev, devno, 1)) {
		pr_notice("Error when adding simple%d\n", 0);
		return -EFAULT;
	}

	return 0;
}

static
void __exit m_exit(void)
{
	cdev_del(&zdev.cdev);
	unregister_chrdev_region(MKDEV(major, 0), DEV_NR);
}


module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Access userspace memory directly");
