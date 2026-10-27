// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kstrtox.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#include "main.h"

static bool msi = true;
module_param(msi, bool, 0444);
MODULE_PARM_DESC(msi, "use MSI (default), or the INTx line if 0");

static const struct pci_device_id irq_dev_ids[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, irq_dev_ids);

/*
 * edu keeps a status word: writing to IRQ_RAISE ORs the value in and sends
 * an MSI, or raises its INTx line, which stays raised until IRQ_ACK has
 * cleared every bit. An MSI is a single message and needs no ack to stop,
 * but INTx is level-triggered: without the ack the handler would be called
 * again and again, until the kernel disables the line ("nobody cared").
 */
static irqreturn_t irq_dev_handler(int irq, void *dev_id)
{
	struct irq_dev *dev = dev_id;
	u32 status;

	status = ioread32(dev->bar0 + EDU_REG_IRQ_STATUS);
	if (!status)
		return IRQ_NONE;	/* not ours: an INTx line may be shared */

	iowrite32(status, dev->bar0 + EDU_REG_IRQ_ACK);

	WRITE_ONCE(dev->last_status, status);
	atomic_inc(&dev->count);
	return IRQ_HANDLED;
}

/*
 * sysfs files in the device's directory, /sys/bus/pci/devices/<addr>/.
 * The driver core adds them after probe() and removes them before
 * remove(), so the driver data is valid whenever they are used.
 */
static ssize_t raise_store(struct device *d, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct irq_dev *dev = dev_get_drvdata(d);
	u32 val;
	int err;

	err = kstrtou32(buf, 0, &val);
	if (err)
		return err;
	if (!val)
		return -EINVAL;		/* no bit set, no interrupt */

	iowrite32(val, dev->bar0 + EDU_REG_IRQ_RAISE);
	return count;
}
static DEVICE_ATTR_WO(raise);

static ssize_t interrupts_show(struct device *d, struct device_attribute *attr,
			       char *buf)
{
	struct irq_dev *dev = dev_get_drvdata(d);

	return sysfs_emit(buf, "%d\n", atomic_read(&dev->count));
}
static DEVICE_ATTR_RO(interrupts);

static ssize_t last_status_show(struct device *d,
				struct device_attribute *attr, char *buf)
{
	struct irq_dev *dev = dev_get_drvdata(d);

	return sysfs_emit(buf, "%#x\n", READ_ONCE(dev->last_status));
}
static DEVICE_ATTR_RO(last_status);

static struct attribute *irq_dev_attrs[] = {
	&dev_attr_raise.attr,
	&dev_attr_interrupts.attr,
	&dev_attr_last_status.attr,
	NULL
};
ATTRIBUTE_GROUPS(irq_dev);

/*
 * pcim_enable_device(), pcim_iomap_region() and devm_request_irq() are the
 * managed versions of the calls below: they are undone automatically when
 * probe() fails or after remove(). They are written out here to show each
 * step and its undo.
 */
static int irq_dev_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	unsigned int types = msi ? PCI_IRQ_MSI : PCI_IRQ_INTX;
	struct irq_dev *dev;
	unsigned long flags;
	int err, nvec;

	dev = kzalloc_obj(*dev);
	if (!dev)
		return -ENOMEM;
	dev->pdev = pdev;
	atomic_set(&dev->count, 0);

	err = pci_enable_device(pdev);
	if (err)
		goto err_free;

	err = pci_request_regions(pdev, MODULE_NAME);
	if (err)
		goto err_disable;

	dev->bar0 = pci_iomap(pdev, 0, 0);
	if (!dev->bar0) {
		err = -ENOMEM;
		goto err_regions;
	}

	/* an MSI is a memory write by the device: it must master the bus */
	pci_set_master(pdev);

	/*
	 * Ask for between 1 and 1 vectors of the given types; the return
	 * value is how many were allocated. edu has a single MSI vector and
	 * no MSI-X. PCI_IRQ_ALL_TYPES would try MSI-X, then MSI, then INTx.
	 * Older drivers called pci_enable_msi() or pci_enable_msix_range().
	 */
	nvec = pci_alloc_irq_vectors(pdev, 1, 1, types);
	if (nvec < 0) {
		err = nvec;
		goto err_unmap;
	}
	dev->irq = pci_irq_vector(pdev, 0);

	/*
	 * An MSI vector belongs to this device alone, an INTx line may be
	 * shared with others. pci_request_irq() picks the flag by itself.
	 */
	flags = pci_dev_msi_enabled(pdev) ? 0 : IRQF_SHARED;
	err = request_irq(dev->irq, irq_dev_handler, flags, MODULE_NAME, dev);
	if (err)
		goto err_vectors;

	pci_set_drvdata(pdev, dev);
	dev_info(&pdev->dev, "using %s, irq %d\n",
		 pci_dev_msi_enabled(pdev) ? "MSI" : "INTx", dev->irq);
	return 0;

err_vectors:
	pci_free_irq_vectors(pdev);
err_unmap:
	pci_clear_master(pdev);
	pci_iounmap(pdev, dev->bar0);
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
err_free:
	kfree(dev);
	return err;
}

static void irq_dev_remove(struct pci_dev *pdev)
{
	struct irq_dev *dev = pci_get_drvdata(pdev);

	free_irq(dev->irq, dev);
	pci_free_irq_vectors(pdev);
	pci_clear_master(pdev);
	pci_iounmap(pdev, dev->bar0);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	kfree(dev);
}

static struct pci_driver irq_dev_driver = {
	.name       = MODULE_NAME,
	.id_table   = irq_dev_ids,
	.probe      = irq_dev_probe,
	.remove     = irq_dev_remove,
	.dev_groups = irq_dev_groups,
};

/* module_pci_driver(irq_dev_driver) generates exactly these two functions */
static int __init m_init(void)
{
	return pci_register_driver(&irq_dev_driver);
}

static void __exit m_exit(void)
{
	pci_unregister_driver(&irq_dev_driver);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("MSI and INTx interrupts of QEMU's edu device");
