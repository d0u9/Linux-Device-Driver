// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
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

static bool msi = true;
module_param(msi, bool, 0444);
MODULE_PARM_DESC(msi, "use MSI (default), or the INTx line if 0");

static dev_t devno;

static const struct pci_device_id dma_dev_ids[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, dma_dev_ids);

/*
 * Copy @len bytes between our buffer and edu's buffer at @edu_addr, and
 * wait until edu is done. Called with dev->lock held.
 *
 * The device is given bus addresses (dma_addr_t), never kernel pointers.
 * writeq() stores the values little-endian, as PCI registers are, so no
 * cpu_to_le64() is needed. edu runs the transfer about 100 ms later and
 * then interrupts with EDU_IRQ_DMA in its status.
 */
static int edu_dma(struct dma_dev *dev, u64 edu_addr, bool to_ram, size_t len)
{
	u64 cmd = EDU_DMA_START | EDU_DMA_IRQ;

	reinit_completion(&dev->dma_done);

	if (to_ram) {
		writeq(edu_addr, dev->bar0 + EDU_REG_DMA_SRC);
		writeq(dev->buf_bus, dev->bar0 + EDU_REG_DMA_DST);
		cmd |= EDU_DMA_TO_RAM;
	} else {
		writeq(dev->buf_bus, dev->bar0 + EDU_REG_DMA_SRC);
		writeq(edu_addr, dev->bar0 + EDU_REG_DMA_DST);
	}
	writeq(len, dev->bar0 + EDU_REG_DMA_COUNT);
	writeq(cmd, dev->bar0 + EDU_REG_DMA_CMD);

	if (!wait_for_completion_timeout(&dev->dma_done,
					 msecs_to_jiffies(1000)))
		return -ETIMEDOUT;
	return 0;
}

static irqreturn_t dma_dev_handler(int irq, void *dev_id)
{
	struct dma_dev *dev = dev_id;
	u32 status;

	status = ioread32(dev->bar0 + EDU_REG_IRQ_STATUS);
	if (!status)
		return IRQ_NONE;	/* not ours: an INTx line may be shared */

	iowrite32(status, dev->bar0 + EDU_REG_IRQ_ACK);
	if (status & EDU_IRQ_DMA)
		complete(&dev->dma_done);
	return IRQ_HANDLED;
}

/*
 * /dev/dma is edu's 4 KB buffer: write() puts data there by DMA from our
 * buffer, read() fetches it by DMA into our buffer, at the file position.
 */
static int dma_open(struct inode *inode, struct file *filp)
{
	struct dma_dev *dev = container_of(inode->i_cdev, struct dma_dev, cdev);

	get_device(&dev->dev);		/* the file keeps dev alive */
	filp->private_data = dev;
	return 0;
}

static int dma_release(struct inode *inode, struct file *filp)
{
	struct dma_dev *dev = filp->private_data;

	put_device(&dev->dev);
	return 0;
}

static ssize_t dma_read(struct file *filp, char __user *ubuf, size_t count,
			loff_t *ppos)
{
	struct dma_dev *dev = filp->private_data;
	ssize_t ret;

	if (*ppos >= EDU_BUF_SIZE)
		return 0;
	count = min_t(size_t, count, EDU_BUF_SIZE - *ppos);

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
	if (dev->removed) {
		ret = -ENODEV;
		goto out;
	}

	ret = edu_dma(dev, EDU_BUF_ADDR + *ppos, true, count);
	if (ret)
		goto out;
	if (copy_to_user(ubuf, dev->buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	*ppos += count;
	ret = count;
out:
	mutex_unlock(&dev->lock);
	return ret;
}

static ssize_t dma_write(struct file *filp, const char __user *ubuf,
			 size_t count, loff_t *ppos)
{
	struct dma_dev *dev = filp->private_data;
	ssize_t ret;

	if (*ppos >= EDU_BUF_SIZE)
		return -ENOSPC;
	count = min_t(size_t, count, EDU_BUF_SIZE - *ppos);

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
	if (dev->removed) {
		ret = -ENODEV;
		goto out;
	}

	if (copy_from_user(dev->buf, ubuf, count)) {
		ret = -EFAULT;
		goto out;
	}
	ret = edu_dma(dev, EDU_BUF_ADDR + *ppos, false, count);
	if (ret)
		goto out;

	*ppos += count;
	ret = count;
out:
	mutex_unlock(&dev->lock);
	return ret;
}

static loff_t dma_llseek(struct file *filp, loff_t off, int whence)
{
	return fixed_size_llseek(filp, off, whence, EDU_BUF_SIZE);
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
 * dev->dev is the device behind /dev/dma. cdev_device_add() makes the cdev
 * hold a reference to it, and every open file holds one too, so this runs
 * only when the device is removed and the last file is closed.
 */
static void dma_dev_release(struct device *d)
{
	kfree(container_of(d, struct dma_dev, dev));
}

/*
 * pcim_enable_device(), pcim_iomap_region(), dmam_alloc_coherent() and
 * devm_request_irq() are the managed versions of the calls below, undone
 * automatically when probe() fails or after remove().
 */
static int dma_dev_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	unsigned int types = msi ? PCI_IRQ_MSI : PCI_IRQ_INTX;
	struct dma_dev *dev;
	unsigned long flags;
	int err, nvec;

	dev = kzalloc_obj(*dev);
	if (!dev)
		return -ENOMEM;
	dev->pdev = pdev;
	mutex_init(&dev->lock);
	init_completion(&dev->dma_done);
	device_initialize(&dev->dev);	/* from here on, put_device() frees */
	dev->dev.release = dma_dev_release;

	err = pci_enable_device(pdev);
	if (err)
		goto err_put;

	err = pci_request_regions(pdev, MODULE_NAME);
	if (err)
		goto err_disable;

	dev->bar0 = pci_iomap(pdev, 0, 0);
	if (!dev->bar0) {
		err = -ENOMEM;
		goto err_regions;
	}

	/* DMA, and MSI, are memory accesses by the device */
	pci_set_master(pdev);

	/*
	 * Tell the DMA API which addresses the device can reach, so that it
	 * hands out only those, using a bounce buffer or an IOMMU if needed.
	 */
	err = dma_set_mask_and_coherent(&pdev->dev,
					DMA_BIT_MASK(EDU_DMA_BITS));
	if (err)
		goto err_unmap;

	/*
	 * Coherent memory: the CPU and the device see each other's writes
	 * without any syncing, for buffers used again and again. For data
	 * that is in a buffer of its own, streaming DMA maps it per transfer
	 * instead: dma_map_single(), then dma_unmap_single() when done.
	 */
	dev->buf = dma_alloc_coherent(&pdev->dev, EDU_BUF_SIZE, &dev->buf_bus,
				      GFP_KERNEL);
	if (!dev->buf) {
		err = -ENOMEM;
		goto err_unmap;
	}

	nvec = pci_alloc_irq_vectors(pdev, 1, 1, types);
	if (nvec < 0) {
		err = nvec;
		goto err_dma;
	}
	dev->irq = pci_irq_vector(pdev, 0);

	/* pci_request_irq() would choose IRQF_SHARED by itself */
	flags = pci_dev_msi_enabled(pdev) ? 0 : IRQF_SHARED;
	err = request_irq(dev->irq, dma_dev_handler, flags, MODULE_NAME, dev);
	if (err)
		goto err_vectors;

	/* /dev/dma: devtmpfs creates the node for a device with a devt */
	dev->dev.parent = &pdev->dev;
	dev->dev.devt = devno;
	err = dev_set_name(&dev->dev, MODULE_NAME);
	if (err)
		goto err_irq;
	cdev_init(&dev->cdev, &dma_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_device_add(&dev->cdev, &dev->dev);
	if (err)
		goto err_irq;

	pci_set_drvdata(pdev, dev);
	dev_info(&pdev->dev, "using %s, buffer at bus address %pad\n",
		 pci_dev_msi_enabled(pdev) ? "MSI" : "INTx", &dev->buf_bus);
	return 0;

err_irq:
	free_irq(dev->irq, dev);
err_vectors:
	pci_free_irq_vectors(pdev);
err_dma:
	dma_free_coherent(&pdev->dev, EDU_BUF_SIZE, dev->buf, dev->buf_bus);
err_unmap:
	pci_clear_master(pdev);
	pci_iounmap(pdev, dev->bar0);
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
err_put:
	put_device(&dev->dev);
	return err;
}

/*
 * Files may still be open. Remove the node, mark the device gone so that
 * their reads and writes fail, and release the hardware. The structure
 * itself goes when the last file is closed.
 */
static void dma_dev_remove(struct pci_dev *pdev)
{
	struct dma_dev *dev = pci_get_drvdata(pdev);

	cdev_device_del(&dev->cdev, &dev->dev);

	mutex_lock(&dev->lock);		/* waits for a transfer in progress */
	dev->removed = true;
	mutex_unlock(&dev->lock);

	free_irq(dev->irq, dev);
	pci_free_irq_vectors(pdev);
	dma_free_coherent(&pdev->dev, EDU_BUF_SIZE, dev->buf, dev->buf_bus);
	pci_clear_master(pdev);
	pci_iounmap(pdev, dev->bar0);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	put_device(&dev->dev);
}

static struct pci_driver dma_dev_driver = {
	.name     = MODULE_NAME,
	.id_table = dma_dev_ids,
	.probe    = dma_dev_probe,
	.remove   = dma_dev_remove,
};

static int __init m_init(void)
{
	int err;

	err = alloc_chrdev_region(&devno, 0, 1, MODULE_NAME);
	if (err)
		return err;

	err = pci_register_driver(&dma_dev_driver);
	if (err)
		unregister_chrdev_region(devno, 1);
	return err;
}

static void __exit m_exit(void)
{
	pci_unregister_driver(&dma_dev_driver);
	unregister_chrdev_region(devno, 1);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("DMA to and from QEMU's edu device");
