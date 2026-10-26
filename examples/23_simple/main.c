// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "main.h"

/*
 * Each device maps its own buffer of SIMPLE_PAGES pages:
 * - simple0 maps it all at mmap() time with remap_pfn_range(), which needs
 *   physically contiguous memory, so the buffer comes from alloc_pages();
 * - simple1 maps one page at a time when it is first touched, from
 *   .fault, so the buffer can be vmalloc()ed and its pages scattered.
 */
static struct page *remap_pages;
static void *fault_buf;

static int major;
static struct cdev simple_devs[DEV_NR];

/* The requested range, offset and length, must lie inside the buffer. */
static bool simple_range_ok(struct vm_area_struct *vma)
{
	return vma->vm_pgoff < SIMPLE_PAGES &&
	       vma_pages(vma) <= SIMPLE_PAGES - vma->vm_pgoff;
}

/*
 * open is called when a mapping is copied, by fork() or by splitting it
 * with a partial munmap() or mprotect(); close when one goes away. The
 * mmap() that creates a mapping does not call open, so it does itself.
 */
static void simple_vma_open(struct vm_area_struct *vma)
{
	pr_debug("vma open:  %#lx-%#lx, page offset %lu\n",
		 vma->vm_start, vma->vm_end, vma->vm_pgoff);
}

static void simple_vma_close(struct vm_area_struct *vma)
{
	pr_debug("vma close: %#lx-%#lx, page offset %lu\n",
		 vma->vm_start, vma->vm_end, vma->vm_pgoff);
}

/* simple0: map the whole range at once */
static const struct vm_operations_struct simple_remap_vm_ops = {
	.open  = simple_vma_open,
	.close = simple_vma_close,
};

static int simple_remap_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long pfn;
	int err;

	if (!simple_range_ok(vma))
		return -EINVAL;

	/*
	 * remap_pfn_range() fills in the page tables for every page now. It
	 * is meant for memory the kernel does not manage as pages and marks
	 * the mapping VM_IO | VM_PFNMAP; registers are mapped the same way
	 * with io_remap_pfn_range() and pgprot_noncached(). For RAM like this
	 * buffer, vm_map_pages() is the newer way and keeps page refcounts.
	 */
	pfn = page_to_pfn(remap_pages) + vma->vm_pgoff;
	err = remap_pfn_range(vma, vma->vm_start, pfn,
			      vma->vm_end - vma->vm_start, vma->vm_page_prot);
	if (err)
		return err;

	vma->vm_ops = &simple_remap_vm_ops;
	simple_vma_open(vma);
	return 0;
}

static const struct file_operations simple_remap_ops = {
	.owner = THIS_MODULE,
	.open  = simple_open,
	.mmap  = simple_remap_mmap,
};

/* simple1: map each page when it is first touched */
static vm_fault_t simple_vma_fault(struct vm_fault *vmf)
{
	struct page *page;

	/* vmf->pgoff is the page's offset in the file, not in the mapping */
	if (vmf->pgoff >= SIMPLE_PAGES)
		return VM_FAULT_SIGBUS;

	page = vmalloc_to_page(fault_buf + vmf->pgoff * PAGE_SIZE);
	get_page(page);		/* the mapping's reference, put at unmap */
	vmf->page = page;

	pr_debug("fault at %#lx: page %lu\n", vmf->address, vmf->pgoff);
	return 0;
}

static const struct vm_operations_struct simple_fault_vm_ops = {
	.open  = simple_vma_open,
	.close = simple_vma_close,
	.fault = simple_vma_fault,
};

static int simple_fault_mmap(struct file *filp, struct vm_area_struct *vma)
{
	if (!simple_range_ok(vma))
		return -EINVAL;

	/* no mremap() past the buffer, and no core dumps of it */
	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_ops = &simple_fault_vm_ops;
	simple_vma_open(vma);
	return 0;
}

static const struct file_operations simple_fault_ops = {
	.owner = THIS_MODULE,
	.open  = simple_open,
	.mmap  = simple_fault_mmap,
};

static int simple_setup_cdev(struct cdev *cdev, int minor,
			     const struct file_operations *fops)
{
	cdev_init(cdev, fops);
	cdev->owner = THIS_MODULE;
	return cdev_add(cdev, MKDEV(major, minor), 1);
}

/*
 * The buffers live as long as the module. A mapping holds a reference to
 * the file and the file one to the module, so the module cannot be
 * unloaded, and the buffers freed, while anything is still mapped.
 */
static int __init m_init(void)
{
	dev_t devno;
	int err, i;

	remap_pages = alloc_pages(GFP_KERNEL, get_order(SIMPLE_SIZE));
	if (!remap_pages)
		return -ENOMEM;

	fault_buf = vmalloc(SIMPLE_SIZE);
	if (!fault_buf) {
		err = -ENOMEM;
		goto err_pages;
	}

	for (i = 0; i < SIMPLE_PAGES; i++) {
		memset32(page_address(remap_pages + i), SIMPLE_PATTERN + i,
			 PAGE_SIZE / 4);
		memset32(fault_buf + i * PAGE_SIZE, SIMPLE_PATTERN + i,
			 PAGE_SIZE / 4);
	}

	err = alloc_chrdev_region(&devno, 0, DEV_NR, MODULE_NAME);
	if (err)
		goto err_vfree;
	major = MAJOR(devno);

	err = simple_setup_cdev(&simple_devs[0], 0, &simple_remap_ops);
	if (err)
		goto err_region;
	err = simple_setup_cdev(&simple_devs[1], 1, &simple_fault_ops);
	if (err)
		goto err_cdev0;

	return 0;

err_cdev0:
	cdev_del(&simple_devs[0]);
err_region:
	unregister_chrdev_region(devno, DEV_NR);
err_vfree:
	vfree(fault_buf);
err_pages:
	__free_pages(remap_pages, get_order(SIMPLE_SIZE));
	return err;
}

static void __exit m_exit(void)
{
	cdev_del(&simple_devs[1]);
	cdev_del(&simple_devs[0]);
	unregister_chrdev_region(MKDEV(major, 0), DEV_NR);
	vfree(fault_buf);
	__free_pages(remap_pages, get_order(SIMPLE_SIZE));
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("mmap with remap_pfn_range() and with .fault");
