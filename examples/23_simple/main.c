#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/mm_types.h>

#include "main.h"

struct page *last_page = NULL;
static unsigned long pfn;


static int major;
static struct cdev SimpleDevs[DEV_NR];
void simple_vma_open(struct vm_area_struct *vma)
{
	size_t len = vma->vm_end - vma->vm_start;
	pr_debug("vma_open -- vm_start: %#lx, vm_end: %#lx\n, len: %#lx, vm_pgoff: %#lx(%#lx), ops=%p\n",
		 vma->vm_start, vma->vm_end, len,
		 vma->vm_pgoff, vma->vm_pgoff << PAGE_SHIFT,
		 vma->vm_ops);
}

void simple_vma_close(struct vm_area_struct *vma)
{
	int refc;
	size_t len = vma->vm_end - vma->vm_start;

	last_page = pfn_to_page(pfn);
	refc = atomic_read(&last_page->_refcount);
	pr_debug("pfn = %lx, last page refc when close = %d\n", pfn, refc);
	pr_debug("vma_close -- vm_start: %#lx, vm_end: %#lx\n, len: %#lx, vm_pgoff: %#lx(%#lx), ops=%p\n",
		 vma->vm_start, vma->vm_end, len,
		 vma->vm_pgoff, vma->vm_pgoff << PAGE_SHIFT,
		 vma->vm_ops);
}

static struct vm_operations_struct simple_remap_vm_ops = {
	.open =  simple_vma_open,
	.close = simple_vma_close,
};

static int simple_remap_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int rv;
	size_t len = vma->vm_end - vma->vm_start;
	struct page *pages;

	/*
	 * To prevent from memory over run. Limit the maximux mmap size here
	 * to 10 * PAGE_SIZE
	 */
	if (len >> PAGE_SHIFT > 10) {
		pr_err("Map size overflow! len=%ld pages\n", len >> PAGE_SHIFT);
		return -EFAULT;
	}

	/*
	 * Allocate necessary pages and initialize it properly.
	 * We initialize all memory to a special value: 0xdeadbeef
	 */
	pages = alloc_pages(GFP_KERNEL, get_order(len));
	memset32(page_address(pages), 0xdeadbeef,
		 (1 << get_order(len)) * PAGE_SIZE / 4);

	rv = remap_pfn_range(vma, vma->vm_start, page_to_pfn(pages),
			    vma->vm_end - vma->vm_start,
			    vma->vm_page_prot);
	if (rv) {
		return -EAGAIN;
	}

	pr_debug("vm_start: %#lx, vm_end: %#lx\n, len: %#lx, vm_pgoff: %#lx(%#lx), ops=%p\n",
		 vma->vm_start, vma->vm_end, len,
		 vma->vm_pgoff, vma->vm_pgoff << PAGE_SHIFT,
		 vma->vm_ops);

	vma->vm_ops = &simple_remap_vm_ops;
	simple_vma_open(vma);

	return 0;
}

static struct file_operations simple_remap_ops = {
	.owner   = THIS_MODULE,
	.open    = simple_open,
	.mmap    = simple_remap_mmap,
};

static vm_fault_t simple_vma_fault(struct vm_fault *vmf)
{
	struct page *pageptr;

	pageptr = alloc_page(GFP_KERNEL);
	vmf->page = pageptr;
	memset(page_address(pageptr), 0xaa, PAGE_SIZE);
	pr_debug("xpfn = %#lx, pageptr = %px", page_to_pfn(pageptr), pageptr);

	return 0;
}

static struct vm_operations_struct simple_fault_vm_ops = {
	.open	=  simple_vma_open,
	.close	= simple_vma_close,
	.fault	= simple_vma_fault,
};

static int simple_fault_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &simple_fault_vm_ops;
	simple_vma_open(vma);
	return 0;
}

static struct file_operations simple_fault_ops = {
	.owner   = THIS_MODULE,
	.open    = simple_open,
	.mmap    = simple_fault_mmap,
};

static void simple_setup_cdev(struct cdev *cdev, int minor,
			      struct file_operations *fops)
{
	dev_t devno = MKDEV(major, minor);

	cdev_init(cdev, fops);
	cdev->owner = THIS_MODULE;
	if (cdev_add(cdev, devno, 1))
		pr_notice("Error when adding simple%d\n", minor);
}

static
int __init m_init(void)
{
	int result;
	dev_t devno;

#ifdef pte_index
	pr_debug("pte_index defined!\n");
#else
	pr_debug("pte_index not defined!\n");
#endif


	result = alloc_chrdev_region(&devno, 0, DEV_NR, MODULE_NAME);
	if (result < 0) {
		pr_err("Unable to get major devno\n");
		return result;
	}
	major = MAJOR(devno);

	simple_setup_cdev(SimpleDevs + 0, 0, &simple_remap_ops);
	simple_setup_cdev(SimpleDevs + 1, 1, &simple_fault_ops);

	return 0;
}

static
void __exit m_exit(void)
{
	cdev_del(SimpleDevs + 0);
	cdev_del(SimpleDevs + 1);
	unregister_chrdev_region(MKDEV(major, 0), DEV_NR);
}


module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("A simple mmap mangement");
