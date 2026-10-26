// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "main.h"
#include "ioc_cmd.h"

static dev_t devno;
static struct cdev zc_cdev;

/*
 * Release the pinned pages. The kernel may have written to them, behind
 * the back of the user's page tables, so mark them dirty first: otherwise
 * the writes could be dropped when the pages are reclaimed or written back.
 * Called with ctx->lock held, or from release when nobody else can.
 */
static void zc_unpin(struct zc_ctx *ctx)
{
	if (!ctx->pages)
		return;

	unpin_user_pages_dirty_lock(ctx->pages, ctx->npages, true);
	kfree(ctx->pages);
	ctx->pages = NULL;
	ctx->npages = 0;
}

static long zc_pin(struct zc_ctx *ctx, struct zc_buf __user *arg)
{
	struct page **pages;
	struct zc_buf buf;
	int npages, pinned;

	if (copy_from_user(&buf, arg, sizeof(buf)))
		return -EFAULT;
	if (!PAGE_ALIGNED(buf.addr) || !PAGE_ALIGNED(buf.len) || !buf.len ||
	    buf.len > ZC_MAX_PAGES * PAGE_SIZE)
		return -EINVAL;
	if (ctx->pages)
		return -EBUSY;

	npages = buf.len >> PAGE_SHIFT;
	pages = kmalloc_objs(*pages, npages);
	if (!pages)
		return -ENOMEM;

	/*
	 * Find the pages behind the user addresses, faulting them in if
	 * needed, and pin them: they stay in memory, and stay the pages the
	 * process sees, until unpinned. FOLL_WRITE because the kernel writes
	 * to them, FOLL_LONGTERM because they are kept across system calls.
	 * Older drivers used get_user_pages_fast() and put_page(); a pin is
	 * counted apart from other references, so memory management can tell
	 * a page is in direct use and leave it alone.
	 */
	pinned = pin_user_pages_fast(buf.addr, npages,
				     FOLL_WRITE | FOLL_LONGTERM, pages);
	if (pinned != npages) {
		if (pinned > 0)
			unpin_user_pages(pages, pinned);
		kfree(pages);
		return pinned < 0 ? pinned : -EFAULT;
	}

	ctx->pages = pages;
	ctx->npages = npages;
	pr_debug("pinned %d pages at %#llx\n", npages, buf.addr);
	return 0;
}

/* Fill every pinned page with a value, through the kernel's own mapping. */
static long zc_fill(struct zc_ctx *ctx, u32 __user *arg)
{
	void *addr;
	u32 val;
	int i;

	if (get_user(val, arg))
		return -EFAULT;
	if (!ctx->pages)
		return -EINVAL;

	for (i = 0; i < ctx->npages; i++) {
		/* a kernel address for the page, valid until kunmap_local() */
		addr = kmap_local_page(ctx->pages[i]);
		memset32(addr, val, PAGE_SIZE / 4);
		kunmap_local(addr);
		/* for CPUs whose caches can alias; nothing to do on x86 */
		flush_dcache_page(ctx->pages[i]);
	}

	return 0;
}

/* Add up the first word of every pinned page, as user space left them. */
static long zc_sum(struct zc_ctx *ctx, u32 __user *arg)
{
	u32 *addr, sum = 0;
	int i;

	if (!ctx->pages)
		return -EINVAL;

	for (i = 0; i < ctx->npages; i++) {
		addr = kmap_local_page(ctx->pages[i]);
		pr_debug("page %d: %#x\n", i, addr[0]);
		sum += addr[0];
		kunmap_local(addr);
	}

	return put_user(sum, arg);
}

static long zc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct zc_ctx *ctx = filp->private_data;
	void __user *uarg = (void __user *)arg;
	long ret;

	mutex_lock(&ctx->lock);
	switch (cmd) {
	case ZC_PIN:
		ret = zc_pin(ctx, uarg);
		break;
	case ZC_FILL:
		ret = zc_fill(ctx, uarg);
		break;
	case ZC_SUM:
		ret = zc_sum(ctx, uarg);
		break;
	case ZC_UNPIN:
		zc_unpin(ctx);
		ret = 0;
		break;
	default:
		ret = -ENOTTY;
	}
	mutex_unlock(&ctx->lock);

	return ret;
}

static int zc_open(struct inode *inode, struct file *filp)
{
	struct zc_ctx *ctx;

	ctx = kzalloc_obj(*ctx);
	if (!ctx)
		return -ENOMEM;

	mutex_init(&ctx->lock);
	filp->private_data = ctx;
	return 0;
}

static int zc_release(struct inode *inode, struct file *filp)
{
	struct zc_ctx *ctx = filp->private_data;

	zc_unpin(ctx);
	mutex_destroy(&ctx->lock);
	kfree(ctx);
	return 0;
}

static const struct file_operations zc_fops = {
	.owner          = THIS_MODULE,
	.open           = zc_open,
	.release        = zc_release,
	.unlocked_ioctl = zc_ioctl,
};

static int __init m_init(void)
{
	int err;

	err = alloc_chrdev_region(&devno, 0, 1, MODULE_NAME);
	if (err)
		return err;

	cdev_init(&zc_cdev, &zc_fops);
	zc_cdev.owner = THIS_MODULE;
	err = cdev_add(&zc_cdev, devno, 1);
	if (err) {
		unregister_chrdev_region(devno, 1);
		return err;
	}

	return 0;
}

static void __exit m_exit(void)
{
	cdev_del(&zc_cdev);
	unregister_chrdev_region(devno, 1);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Access user space memory directly");
