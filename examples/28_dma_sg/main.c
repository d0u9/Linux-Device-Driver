// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/bitfield.h>
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#include "main.h"

static unsigned int xor;
module_param(xor, uint, 0644);
MODULE_PARM_DESC(xor, "byte the device XORs into data it sends to RAM (default 0)");

static dev_t devno;

static const struct pci_device_id dma_sg_ids[] = {
	{ PCI_DEVICE(LDD_PCIE_VENDOR_ID, LDD_PCIE_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, dma_sg_ids);

/*
 * Run one transfer of @nents descriptors, already filled in, and wait
 * until the device is done. Called with dsg->lock held.
 */
static int dma_sg_run(struct dma_sg_dev *dsg, int nents, loff_t pos,
		      enum dma_data_direction dir)
{
	struct device *d = &dsg->pdev->dev;
	u32 ctrl = LDD_DMA_CTRL_START;

	iowrite32(lower_32_bits(dsg->desc_bus),
		  dsg->regs + LDD_REG_DESC_ADDR_LO);
	iowrite32(upper_32_bits(dsg->desc_bus),
		  dsg->regs + LDD_REG_DESC_ADDR_HI);
	iowrite32(nents, dsg->regs + LDD_REG_DESC_COUNT);
	iowrite32(pos, dsg->regs + LDD_REG_DEV_OFFSET);
	if (dir == DMA_FROM_DEVICE) {
		ctrl |= LDD_DMA_CTRL_TO_RAM;
		iowrite32(READ_ONCE(xor) & 0xff, dsg->regs + LDD_REG_XOR);
	}

	reinit_completion(&dsg->dma_done);
	iowrite32(ctrl, dsg->regs + LDD_REG_DMA_CTRL);

	if (!wait_for_completion_timeout(&dsg->dma_done,
					 msecs_to_jiffies(1000))) {
		/*
		 * The device may still be using the pages, which must not be
		 * unmapped yet: reset it, which abandons the transfer and
		 * masks the interrupts. Unmask them, and wait for a handler
		 * that may be running late.
		 */
		dev_err(d, "DMA timed out, resetting the device\n");
		iowrite32(LDD_CTRL_RESET, dsg->regs + LDD_REG_CTRL);
		synchronize_irq(dsg->irq);
		iowrite32(LDD_INT_DMA_DONE | LDD_INT_DMA_ERROR,
			  dsg->regs + LDD_REG_INT_MASK);
		return -ETIMEDOUT;
	}
	if (READ_ONCE(dsg->int_status) & LDD_INT_DMA_ERROR) {
		u32 status = ioread32(dsg->regs + LDD_REG_STATUS);

		dev_err(d, "DMA failed, error code %lu at descriptor %u\n",
			FIELD_GET(LDD_STATUS_ERR_CODE, status),
			ioread32(dsg->regs + LDD_REG_ERR_DESC));
		return -EIO;
	}
	return 0;
}

/*
 * Move @count bytes between the user's buffer at @ubuf and the on-board
 * memory at @pos, by DMA straight to or from the user's pages: no copy
 * through a buffer of the driver's.
 *
 * 1. pin_user_pages_fast() finds the pages behind the buffer and keeps
 *    them in memory. They are wherever the process's memory happens to
 *    be, scattered.
 * 2. sg_alloc_table_from_pages_segment() describes them as a scatterlist,
 *    joining pages that are next to each other, up to the device's
 *    largest segment.
 * 3. dma_map_sgtable() maps the list for the device. An IOMMU may join
 *    more segments, so sgt.nents, the segments the device sees, can be
 *    fewer than sgt.orig_nents.
 * 4. One descriptor per mapped segment, and one transfer for them all.
 */
static ssize_t dma_sg_xfer(struct dma_sg_dev *dsg, char __user *ubuf,
			   size_t count, loff_t pos,
			   enum dma_data_direction dir)
{
	unsigned long start = (unsigned long)ubuf;
	unsigned int offset = offset_in_page(start);
	struct device *d = &dsg->pdev->dev;
	struct scatterlist *sg;
	struct sg_table sgt;
	int npages, pinned, i, err;

	/* the device writes to the pages when reading: FOLL_WRITE */
	npages = DIV_ROUND_UP(offset + count, PAGE_SIZE);
	pinned = pin_user_pages_fast(start & PAGE_MASK, npages,
				     dir == DMA_FROM_DEVICE ? FOLL_WRITE : 0,
				     dsg->pages);
	if (pinned <= 0)
		return pinned ? pinned : -EFAULT;
	if (pinned < npages) {
		/* only the start of the buffer is there: move that much */
		npages = pinned;
		count = (size_t)npages * PAGE_SIZE - offset;
	}

	err = sg_alloc_table_from_pages_segment(&sgt, dsg->pages, npages,
						offset, count,
						dma_get_max_seg_size(d),
						GFP_KERNEL);
	if (err)
		goto out_unpin;

	/*
	 * Hand the pages to the device: from here until dma_unmap_sgtable()
	 * the CPU must not touch them. Mapping also gives each segment its
	 * bus address and length, sg_dma_address() and sg_dma_len().
	 */
	err = dma_map_sgtable(d, &sgt, dir, 0);
	if (err)
		goto out_free;

	/* the DMA segments, not the pages: for_each_sgtable_dma_sg() */
	for_each_sgtable_dma_sg(&sgt, sg, i) {
		dsg->desc[i].addr = cpu_to_le64(sg_dma_address(sg));
		dsg->desc[i].len = cpu_to_le32(sg_dma_len(sg));
		dsg->desc[i].flags = 0;
	}

	dsg->last_pages = npages;
	dsg->last_segs = sgt.orig_nents;
	dsg->last_descs = sgt.nents;

	err = dma_sg_run(dsg, sgt.nents, pos, dir);

	dma_unmap_sgtable(d, &sgt, dir, 0);
out_free:
	sg_free_table(&sgt);
out_unpin:
	/* pages the device wrote to are dirty: their contents changed */
	if (dir == DMA_FROM_DEVICE)
		unpin_user_pages_dirty_lock(dsg->pages, npages, !err);
	else
		unpin_user_pages(dsg->pages, npages);
	return err ? err : count;
}

static irqreturn_t dma_sg_handler(int irq, void *dev_id)
{
	struct dma_sg_dev *dsg = dev_id;
	u32 status;

	status = ioread32(dsg->regs + LDD_REG_INT_STATUS) &
		 (LDD_INT_DMA_DONE | LDD_INT_DMA_ERROR);
	if (!status)
		return IRQ_NONE;	/* not ours: an INTx line may be shared */

	iowrite32(status, dsg->regs + LDD_REG_INT_STATUS);
	WRITE_ONCE(dsg->int_status, status);
	complete(&dsg->dma_done);
	return IRQ_HANDLED;
}

/*
 * /dev/dma_sg is the device's 1 MiB of on-board memory: read() and write()
 * move the data by DMA straight between it and the caller's buffer. A call
 * moves at most what max_desc pages hold, and returns how much.
 */
static int dma_sg_open(struct inode *inode, struct file *filp)
{
	struct dma_sg_dev *dsg = container_of(inode->i_cdev,
					      struct dma_sg_dev, cdev);

	get_device(&dsg->dev);		/* the file keeps dsg alive */
	filp->private_data = dsg;
	return 0;
}

static int dma_sg_release(struct inode *inode, struct file *filp)
{
	struct dma_sg_dev *dsg = filp->private_data;

	put_device(&dsg->dev);
	return 0;
}

static ssize_t dma_sg_rw(struct file *filp, char __user *ubuf, size_t count,
			 loff_t *ppos, enum dma_data_direction dir)
{
	struct dma_sg_dev *dsg = filp->private_data;
	size_t room;
	ssize_t ret;

	if (*ppos >= LDD_MEM_SIZE)
		return dir == DMA_TO_DEVICE ? -ENOSPC : 0;

	/* no more pages than descriptors: one segment may be one page */
	room = (size_t)dsg->max_desc * PAGE_SIZE -
	       offset_in_page((unsigned long)ubuf);
	count = min3(count, (size_t)(LDD_MEM_SIZE - *ppos), room);
	if (!count)
		return 0;

	if (mutex_lock_interruptible(&dsg->lock))
		return -ERESTARTSYS;
	if (dsg->removed)
		ret = -ENODEV;
	else
		ret = dma_sg_xfer(dsg, ubuf, count, *ppos, dir);
	if (ret > 0)
		*ppos += ret;
	mutex_unlock(&dsg->lock);
	return ret;
}

static ssize_t dma_sg_read(struct file *filp, char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	return dma_sg_rw(filp, ubuf, count, ppos, DMA_FROM_DEVICE);
}

static ssize_t dma_sg_write(struct file *filp, const char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	/* the device only reads these pages */
	return dma_sg_rw(filp, (char __user *)ubuf, count, ppos,
			 DMA_TO_DEVICE);
}

static loff_t dma_sg_llseek(struct file *filp, loff_t off, int whence)
{
	return fixed_size_llseek(filp, off, whence, LDD_MEM_SIZE);
}

static const struct file_operations dma_sg_fops = {
	.owner   = THIS_MODULE,
	.open    = dma_sg_open,
	.release = dma_sg_release,
	.read    = dma_sg_read,
	.write   = dma_sg_write,
	.llseek  = dma_sg_llseek,
};

/* /sys/bus/pci/devices/<addr>/dma_sg/last: what the last transfer was */
static ssize_t last_show(struct device *d, struct device_attribute *attr,
			 char *buf)
{
	struct dma_sg_dev *dsg = container_of(d, struct dma_sg_dev, dev);
	ssize_t ret;

	mutex_lock(&dsg->lock);
	ret = sysfs_emit(buf, "pages %u segments %u descriptors %u\n",
			 dsg->last_pages, dsg->last_segs, dsg->last_descs);
	mutex_unlock(&dsg->lock);
	return ret;
}
static DEVICE_ATTR_RO(last);

static struct attribute *dma_sg_attrs[] = {
	&dev_attr_last.attr,
	NULL
};
ATTRIBUTE_GROUPS(dma_sg);

/*
 * dsg->dev is the device behind /dev/dma_sg. cdev_device_add() makes the
 * cdev hold a reference to it, and every open file holds one too, so this
 * runs only when the device is removed and the last file is closed.
 */
static void dma_sg_dev_release(struct device *d)
{
	struct dma_sg_dev *dsg = container_of(d, struct dma_sg_dev, dev);

	kfree(dsg->pages);
	kfree(dsg);
}

/*
 * pcim_enable_device(), pcim_iomap_region(), dmam_alloc_coherent() and
 * devm_request_irq() are the managed versions of the calls below, undone
 * automatically when probe() fails or after remove().
 */
static int dma_sg_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct dma_sg_dev *dsg;
	unsigned long flags;
	u32 caps, max_seg;
	int err, nvec;

	dsg = kzalloc_obj(*dsg);
	if (!dsg)
		return -ENOMEM;
	dsg->pdev = pdev;
	mutex_init(&dsg->lock);
	init_completion(&dsg->dma_done);
	device_initialize(&dsg->dev);	/* from here on, put_device() frees */
	dsg->dev.release = dma_sg_dev_release;

	err = pci_enable_device(pdev);
	if (err)
		goto err_put;

	err = pci_request_regions(pdev, MODULE_NAME);
	if (err)
		goto err_disable;

	dsg->regs = pci_iomap(pdev, 0, 0);
	if (!dsg->regs) {
		err = -ENOMEM;
		goto err_regions;
	}
	if (ioread32(dsg->regs + LDD_REG_ID) != LDD_ID) {
		err = -ENODEV;
		goto err_unmap;
	}

	/*
	 * sg_alloc_table_from_pages_segment() builds the scatterlist from
	 * whole pages and refuses a segment limit below a page. A device with
	 * smaller segments needs the list built by hand: sg_alloc_table(),
	 * then sg_set_page() for each piece, several per page. This example
	 * keeps to the helper and refuses such a device.
	 */
	max_seg = ioread32(dsg->regs + LDD_REG_MAX_SEG);
	if (max_seg < PAGE_SIZE) {
		dev_err(&pdev->dev, "segments of %u bytes, less than a page\n",
			max_seg);
		err = -ENODEV;
		goto err_unmap;
	}
	dsg->max_desc = ioread32(dsg->regs + LDD_REG_MAX_DESC);

	pci_set_master(pdev);

	caps = ioread32(dsg->regs + LDD_REG_CAPS);
	err = dma_set_mask_and_coherent(&pdev->dev, caps & LDD_CAPS_DMA64 ?
					DMA_BIT_MASK(64) : DMA_BIT_MASK(32));
	if (err)
		goto err_master;

	/*
	 * The largest segment the device takes: scatterlists are built, and
	 * an IOMMU joins segments, only up to it.
	 */
	dma_set_max_seg_size(&pdev->dev, max_seg);

	dsg->desc = dma_alloc_coherent(&pdev->dev,
				       dsg->max_desc * sizeof(*dsg->desc),
				       &dsg->desc_bus, GFP_KERNEL);
	if (!dsg->desc) {
		err = -ENOMEM;
		goto err_master;
	}

	dsg->pages = kmalloc_objs(*dsg->pages, dsg->max_desc);
	if (!dsg->pages) {
		err = -ENOMEM;
		goto err_coherent;
	}

	/* not MSI-X: DMA_ERROR would need a vector of its own */
	nvec = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_INTX);
	if (nvec < 0) {
		err = nvec;
		goto err_coherent;
	}
	dsg->irq = pci_irq_vector(pdev, 0);

	/* pci_request_irq() would choose IRQF_SHARED by itself */
	flags = pci_dev_msi_enabled(pdev) ? 0 : IRQF_SHARED;
	err = request_irq(dsg->irq, dma_sg_handler, flags, MODULE_NAME, dsg);
	if (err)
		goto err_vectors;

	iowrite32(LDD_INT_DMA_DONE | LDD_INT_DMA_ERROR,
		  dsg->regs + LDD_REG_INT_STATUS);
	iowrite32(LDD_INT_DMA_DONE | LDD_INT_DMA_ERROR,
		  dsg->regs + LDD_REG_INT_MASK);

	/* /dev/dma_sg: devtmpfs creates the node for a device with a devt */
	dsg->dev.parent = &pdev->dev;
	dsg->dev.devt = devno;
	dsg->dev.groups = dma_sg_groups;
	err = dev_set_name(&dsg->dev, MODULE_NAME);
	if (err)
		goto err_irq;
	cdev_init(&dsg->cdev, &dma_sg_fops);
	dsg->cdev.owner = THIS_MODULE;
	err = cdev_device_add(&dsg->cdev, &dsg->dev);
	if (err)
		goto err_irq;

	pci_set_drvdata(pdev, dsg);
	dev_info(&pdev->dev, "using %s, %s DMA, up to %u descriptors of %u bytes\n",
		 pci_dev_msi_enabled(pdev) ? "MSI" : "INTx",
		 caps & LDD_CAPS_DMA64 ? "64-bit" : "32-bit",
		 dsg->max_desc, max_seg);
	return 0;

err_irq:
	iowrite32(0, dsg->regs + LDD_REG_INT_MASK);
	free_irq(dsg->irq, dsg);
err_vectors:
	pci_free_irq_vectors(pdev);
err_coherent:
	dma_free_coherent(&pdev->dev, dsg->max_desc * sizeof(*dsg->desc),
			  dsg->desc, dsg->desc_bus);
err_master:
	pci_clear_master(pdev);
err_unmap:
	pci_iounmap(pdev, dsg->regs);
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
err_put:
	put_device(&dsg->dev);		/* frees dsg->pages too */
	return err;
}

/*
 * Files may still be open. Remove the node, mark the device gone so that
 * their reads and writes fail, and release the hardware. The structure
 * itself goes when the last file is closed.
 */
static void dma_sg_remove(struct pci_dev *pdev)
{
	struct dma_sg_dev *dsg = pci_get_drvdata(pdev);

	cdev_device_del(&dsg->cdev, &dsg->dev);

	mutex_lock(&dsg->lock);		/* waits for a transfer in progress */
	dsg->removed = true;
	mutex_unlock(&dsg->lock);

	iowrite32(0, dsg->regs + LDD_REG_INT_MASK);
	free_irq(dsg->irq, dsg);
	pci_free_irq_vectors(pdev);
	dma_free_coherent(&pdev->dev, dsg->max_desc * sizeof(*dsg->desc),
			  dsg->desc, dsg->desc_bus);
	pci_clear_master(pdev);
	pci_iounmap(pdev, dsg->regs);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	put_device(&dsg->dev);
}

static struct pci_driver dma_sg_driver = {
	.name     = MODULE_NAME,
	.id_table = dma_sg_ids,
	.probe    = dma_sg_probe,
	.remove   = dma_sg_remove,
};

static int __init m_init(void)
{
	int err;

	err = alloc_chrdev_region(&devno, 0, 1, MODULE_NAME);
	if (err)
		return err;

	err = pci_register_driver(&dma_sg_driver);
	if (err)
		unregister_chrdev_region(devno, 1);
	return err;
}

static void __exit m_exit(void)
{
	pci_unregister_driver(&dma_sg_driver);
	unregister_chrdev_region(devno, 1);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Scatter-gather DMA to user pages with the ldd-pcie device");
