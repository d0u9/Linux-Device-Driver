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
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "main.h"

static unsigned int xor;
module_param(xor, uint, 0644);
MODULE_PARM_DESC(xor, "byte the device XORs into data it sends to RAM (default 0)");

static dev_t devno;

static const struct pci_device_id dma_ids[] = {
	{ PCI_DEVICE(LDD_PCIE_VENDOR_ID, LDD_PCIE_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, dma_ids);

/*
 * Move @len bytes between our buffer and the on-board memory at @pos, and
 * wait until the device is done. Called with dma->lock held.
 *
 * The buffer is streaming DMA: dma_map_single() hands it to the device for
 * this one transfer, in direction @dir, and gives its bus address;
 * dma_unmap_single() hands it back. In between the CPU must not touch it.
 * On the way the DMA API does what the platform needs: cache flushes, an
 * IOMMU mapping, or a copy through a bounce buffer the device can reach.
 */
static int dma_xfer(struct dma_dev *dma, loff_t pos, size_t len,
		    enum dma_data_direction dir)
{
	struct device *d = &dma->pdev->dev;
	u32 ctrl = LDD_DMA_CTRL_START;
	dma_addr_t bus;
	int err = 0;

	bus = dma_map_single(d, dma->buf, len, dir);
	if (dma_mapping_error(d, bus))
		return -ENOMEM;

	/*
	 * Without an IOMMU the bus address is the physical one; with one,
	 * it is whatever the IOMMU was told to translate. virt_to_phys() is
	 * only for printing: the device is given what the DMA API returned.
	 */
	dev_info_once(d, "buffer at phys %pa is at bus %pad\n",
		      &(phys_addr_t){ virt_to_phys(dma->buf) }, &bus);

	/*
	 * The descriptor table is coherent memory: what the CPU writes is
	 * seen by the device without syncing. iowrite32() orders these
	 * stores before the register writes that start the transfer.
	 */
	dma->desc[0].addr = cpu_to_le64(bus);
	dma->desc[0].len = cpu_to_le32(len);
	dma->desc[0].flags = 0;

	/* the registers take 32-bit accesses only: the address in halves */
	iowrite32(lower_32_bits(dma->desc_bus),
		  dma->regs + LDD_REG_DESC_ADDR_LO);
	iowrite32(upper_32_bits(dma->desc_bus),
		  dma->regs + LDD_REG_DESC_ADDR_HI);
	iowrite32(1, dma->regs + LDD_REG_DESC_COUNT);
	iowrite32(pos, dma->regs + LDD_REG_DEV_OFFSET);
	if (dir == DMA_FROM_DEVICE) {
		ctrl |= LDD_DMA_CTRL_TO_RAM;
		iowrite32(READ_ONCE(xor) & 0xff, dma->regs + LDD_REG_XOR);
	}

	reinit_completion(&dma->dma_done);
	iowrite32(ctrl, dma->regs + LDD_REG_DMA_CTRL);

	if (!wait_for_completion_timeout(&dma->dma_done,
					 msecs_to_jiffies(1000))) {
		/*
		 * The device may still be reading or writing the buffer, so
		 * it must not be unmapped yet: reset the device, which
		 * abandons the transfer without an interrupt. A reset also
		 * masks every interrupt; unmask them again, and wait for a
		 * handler that may be running late.
		 */
		dev_err(d, "DMA timed out, resetting the device\n");
		iowrite32(LDD_CTRL_RESET, dma->regs + LDD_REG_CTRL);
		synchronize_irq(dma->irq);
		iowrite32(LDD_INT_DMA_DONE | LDD_INT_DMA_ERROR,
			  dma->regs + LDD_REG_INT_MASK);
		err = -ETIMEDOUT;
	} else if (READ_ONCE(dma->int_status) & LDD_INT_DMA_ERROR) {
		u32 status = ioread32(dma->regs + LDD_REG_STATUS);

		dev_err(d, "DMA failed, error code %lu\n",
			FIELD_GET(LDD_STATUS_ERR_CODE, status));
		err = -EIO;
	}

	dma_unmap_single(d, bus, len, dir);
	return err;
}

static irqreturn_t dma_handler(int irq, void *dev_id)
{
	struct dma_dev *dma = dev_id;
	u32 status;

	status = ioread32(dma->regs + LDD_REG_INT_STATUS) &
		 (LDD_INT_DMA_DONE | LDD_INT_DMA_ERROR);
	if (!status)
		return IRQ_NONE;	/* not ours: an INTx line may be shared */

	iowrite32(status, dma->regs + LDD_REG_INT_STATUS);
	WRITE_ONCE(dma->int_status, status);
	complete(&dma->dma_done);
	return IRQ_HANDLED;
}

/*
 * /dev/dma is the device's 1 MiB of on-board memory: write() puts data
 * there by DMA, read() fetches it by DMA, at the file position. A call
 * moves at most one buffer, dma->chunk bytes, and returns how much.
 */
static int dma_open(struct inode *inode, struct file *filp)
{
	struct dma_dev *dma = container_of(inode->i_cdev, struct dma_dev, cdev);

	get_device(&dma->dev);		/* the file keeps dma alive */
	filp->private_data = dma;
	return 0;
}

static int dma_release(struct inode *inode, struct file *filp)
{
	struct dma_dev *dma = filp->private_data;

	put_device(&dma->dev);
	return 0;
}

static ssize_t dma_read(struct file *filp, char __user *ubuf, size_t count,
			loff_t *ppos)
{
	struct dma_dev *dma = filp->private_data;
	ssize_t ret;

	if (*ppos >= LDD_MEM_SIZE)
		return 0;
	count = min3(count, (size_t)(LDD_MEM_SIZE - *ppos), dma->chunk);
	if (!count)
		return 0;

	if (mutex_lock_interruptible(&dma->lock))
		return -ERESTARTSYS;
	if (dma->removed) {
		ret = -ENODEV;
		goto out;
	}

	ret = dma_xfer(dma, *ppos, count, DMA_FROM_DEVICE);
	if (ret)
		goto out;
	/* unmapped: the buffer is the CPU's again */
	if (copy_to_user(ubuf, dma->buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	*ppos += count;
	ret = count;
out:
	mutex_unlock(&dma->lock);
	return ret;
}

static ssize_t dma_write(struct file *filp, const char __user *ubuf,
			 size_t count, loff_t *ppos)
{
	struct dma_dev *dma = filp->private_data;
	ssize_t ret;

	if (*ppos >= LDD_MEM_SIZE)
		return -ENOSPC;
	count = min3(count, (size_t)(LDD_MEM_SIZE - *ppos), dma->chunk);
	if (!count)
		return 0;

	if (mutex_lock_interruptible(&dma->lock))
		return -ERESTARTSYS;
	if (dma->removed) {
		ret = -ENODEV;
		goto out;
	}

	/* fill the buffer before it is mapped and becomes the device's */
	if (copy_from_user(dma->buf, ubuf, count)) {
		ret = -EFAULT;
		goto out;
	}
	ret = dma_xfer(dma, *ppos, count, DMA_TO_DEVICE);
	if (ret)
		goto out;

	*ppos += count;
	ret = count;
out:
	mutex_unlock(&dma->lock);
	return ret;
}

static loff_t dma_llseek(struct file *filp, loff_t off, int whence)
{
	return fixed_size_llseek(filp, off, whence, LDD_MEM_SIZE);
}

static const struct file_operations dma_fops = {
	.owner   = THIS_MODULE,
	.open    = dma_open,
	.release = dma_release,
	.read    = dma_read,
	.write   = dma_write,
	.llseek  = dma_llseek,
};

/*
 * dma->dev is the device behind /dev/dma. cdev_device_add() makes the cdev
 * hold a reference to it, and every open file holds one too, so this runs
 * only when the device is removed and the last file is closed.
 */
static void dma_dev_release(struct device *d)
{
	kfree(container_of(d, struct dma_dev, dev));
}

/*
 * pcim_enable_device(), pcim_iomap_region(), dmam_alloc_coherent(),
 * devm_kmalloc() and devm_request_irq() are the managed versions of the
 * calls below, undone automatically when probe() fails or after remove().
 */
static int dma_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct dma_dev *dma;
	unsigned long flags;
	int err, nvec;
	u32 caps;

	dma = kzalloc_obj(*dma);
	if (!dma)
		return -ENOMEM;
	dma->pdev = pdev;
	mutex_init(&dma->lock);
	init_completion(&dma->dma_done);
	device_initialize(&dma->dev);	/* from here on, put_device() frees */
	dma->dev.release = dma_dev_release;

	err = pci_enable_device(pdev);
	if (err)
		goto err_put;

	err = pci_request_regions(pdev, MODULE_NAME);
	if (err)
		goto err_disable;

	dma->regs = pci_iomap(pdev, 0, 0);
	if (!dma->regs) {
		err = -ENOMEM;
		goto err_regions;
	}
	if (ioread32(dma->regs + LDD_REG_ID) != LDD_ID) {
		err = -ENODEV;
		goto err_unmap;
	}

	/* DMA, and MSI, are memory accesses by the device */
	pci_set_master(pdev);

	/*
	 * Tell the DMA API which addresses the device can reach, so that it
	 * hands out only those, using a bounce buffer or an IOMMU if needed.
	 * ldd-pcie says in CAPS whether it drives all 64 bits.
	 */
	caps = ioread32(dma->regs + LDD_REG_CAPS);
	err = dma_set_mask_and_coherent(&pdev->dev, caps & LDD_CAPS_DMA64 ?
					DMA_BIT_MASK(64) : DMA_BIT_MASK(32));
	if (err)
		goto err_master;

	/*
	 * Coherent memory for the descriptor table: allocated once, written
	 * by the CPU and read by the device again and again, with no syncing.
	 */
	dma->desc = dma_alloc_coherent(&pdev->dev, sizeof(*dma->desc),
				       &dma->desc_bus, GFP_KERNEL);
	if (!dma->desc) {
		err = -ENOMEM;
		goto err_master;
	}

	/*
	 * Ordinary memory for the data, mapped per transfer. kmalloc()
	 * memory is physically contiguous, as one descriptor needs; vmalloc()
	 * memory is not, and could not be mapped as one piece.
	 */
	dma->chunk = min_t(size_t, DMA_CHUNK,
			   ioread32(dma->regs + LDD_REG_MAX_SEG));
	dma->buf = kmalloc(dma->chunk, GFP_KERNEL);
	if (!dma->buf) {
		err = -ENOMEM;
		goto err_coherent;
	}

	/*
	 * One vector, MSI or INTx. Not MSI-X: there DMA_ERROR has a vector of
	 * its own, vector 1, and with one vector its interrupt would be lost.
	 */
	nvec = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_INTX);
	if (nvec < 0) {
		err = nvec;
		goto err_buf;
	}
	dma->irq = pci_irq_vector(pdev, 0);

	/* pci_request_irq() would choose IRQF_SHARED by itself */
	flags = pci_dev_msi_enabled(pdev) ? 0 : IRQF_SHARED;
	err = request_irq(dma->irq, dma_handler, flags, MODULE_NAME, dma);
	if (err)
		goto err_vectors;

	iowrite32(LDD_INT_DMA_DONE | LDD_INT_DMA_ERROR,
		  dma->regs + LDD_REG_INT_STATUS);
	iowrite32(LDD_INT_DMA_DONE | LDD_INT_DMA_ERROR,
		  dma->regs + LDD_REG_INT_MASK);

	/* /dev/dma: devtmpfs creates the node for a device with a devt */
	dma->dev.parent = &pdev->dev;
	dma->dev.devt = devno;
	err = dev_set_name(&dma->dev, MODULE_NAME);
	if (err)
		goto err_irq;
	cdev_init(&dma->cdev, &dma_fops);
	dma->cdev.owner = THIS_MODULE;
	err = cdev_device_add(&dma->cdev, &dma->dev);
	if (err)
		goto err_irq;

	pci_set_drvdata(pdev, dma);
	dev_info(&pdev->dev, "using %s, %s DMA, descriptors at bus %pad\n",
		 pci_dev_msi_enabled(pdev) ? "MSI" : "INTx",
		 caps & LDD_CAPS_DMA64 ? "64-bit" : "32-bit", &dma->desc_bus);
	return 0;

err_irq:
	iowrite32(0, dma->regs + LDD_REG_INT_MASK);
	free_irq(dma->irq, dma);
err_vectors:
	pci_free_irq_vectors(pdev);
err_buf:
	kfree(dma->buf);
err_coherent:
	dma_free_coherent(&pdev->dev, sizeof(*dma->desc), dma->desc,
			  dma->desc_bus);
err_master:
	pci_clear_master(pdev);
err_unmap:
	pci_iounmap(pdev, dma->regs);
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
err_put:
	put_device(&dma->dev);
	return err;
}

/*
 * Files may still be open. Remove the node, mark the device gone so that
 * their reads and writes fail, and release the hardware. The structure
 * itself goes when the last file is closed.
 */
static void dma_remove(struct pci_dev *pdev)
{
	struct dma_dev *dma = pci_get_drvdata(pdev);

	cdev_device_del(&dma->cdev, &dma->dev);

	mutex_lock(&dma->lock);		/* waits for a transfer in progress */
	dma->removed = true;
	mutex_unlock(&dma->lock);

	iowrite32(0, dma->regs + LDD_REG_INT_MASK);
	free_irq(dma->irq, dma);
	pci_free_irq_vectors(pdev);
	kfree(dma->buf);
	dma_free_coherent(&pdev->dev, sizeof(*dma->desc), dma->desc,
			  dma->desc_bus);
	pci_clear_master(pdev);
	pci_iounmap(pdev, dma->regs);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	put_device(&dma->dev);
}

static struct pci_driver dma_driver = {
	.name     = MODULE_NAME,
	.id_table = dma_ids,
	.probe    = dma_probe,
	.remove   = dma_remove,
};

static int __init m_init(void)
{
	int err;

	err = alloc_chrdev_region(&devno, 0, 1, MODULE_NAME);
	if (err)
		return err;

	err = pci_register_driver(&dma_driver);
	if (err)
		unregister_chrdev_region(devno, 1);
	return err;
}

static void __exit m_exit(void)
{
	pci_unregister_driver(&dma_driver);
	unregister_chrdev_region(devno, 1);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Single-segment DMA with the ldd-pcie device");
