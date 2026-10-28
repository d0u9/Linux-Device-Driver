// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kstrtox.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include "main.h"

static char *mode = "all";
module_param(mode, charp, 0444);
MODULE_PARM_DESC(mode, "msix, msi, intx, or all (default): the first that works");

/* PCI_IRQ_* types from @mode, checked when the module is loaded */
static unsigned int irq_types;

static const struct pci_device_id msi_ids[] = {
	{ PCI_DEVICE(LDD_PCIE_VENDOR_ID, LDD_PCIE_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, msi_ids);

static const char * const event_names[LDD_NR_EVENTS] = {
	"done", "error", "soft"
};

static void count_events(struct msi_dev *msi, u32 status)
{
	for (int i = 0; i < LDD_NR_EVENTS; i++)
		if (status & BIT(i))
			atomic_inc(&msi->count[i]);
	WRITE_ONCE(msi->last_status, status);
}

/*
 * With MSI-X every event has a vector of its own, so the handler knows the
 * event without asking the device. It still has to clear the bit: ldd-pcie
 * sends a message when a bit goes from 0 to 1, and a bit left set would
 * swallow the next event of its kind.
 */
static irqreturn_t msi_vec_handler(int irq, void *dev_id)
{
	struct msi_vec *vec = dev_id;

	iowrite32(vec->bit, vec->msi->regs + LDD_REG_INT_STATUS);
	count_events(vec->msi, vec->bit);
	return IRQ_HANDLED;
}

/*
 * With one vector for everything, MSI or INTx, the handler reads which
 * events are pending and clears those, writing 1s back (write 1 to
 * clear). Several events may come with one interrupt.
 *
 * INTx is level-triggered: the line stays asserted while a bit is set and
 * unmasked, so without the clear the handler would be called again and
 * again, until the kernel disables the line ("nobody cared"). And the line
 * may be shared: no bit set means the interrupt is another device's.
 */
static irqreturn_t msi_handler(int irq, void *dev_id)
{
	struct msi_vec *vec = dev_id;
	struct msi_dev *msi = vec->msi;
	u32 status;

	status = ioread32(msi->regs + LDD_REG_INT_STATUS) & LDD_INT_ALL;
	if (!status)
		return IRQ_NONE;

	iowrite32(status, msi->regs + LDD_REG_INT_STATUS);
	count_events(msi, status);
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
	struct msi_dev *msi = dev_get_drvdata(d);
	u32 val;
	int err;

	err = kstrtou32(buf, 0, &val);
	if (err)
		return err;
	if (!val || val & ~LDD_INT_ALL)
		return -EINVAL;

	/* the device acts as if the events had happened */
	iowrite32(val, msi->regs + LDD_REG_INT_RAISE);
	return count;
}
static DEVICE_ATTR_WO(raise);

static ssize_t interrupts_show(struct device *d, struct device_attribute *attr,
			       char *buf)
{
	struct msi_dev *msi = dev_get_drvdata(d);

	return sysfs_emit(buf, "done %d error %d soft %d\n",
			  atomic_read(&msi->count[0]),
			  atomic_read(&msi->count[1]),
			  atomic_read(&msi->count[2]));
}
static DEVICE_ATTR_RO(interrupts);

static ssize_t last_status_show(struct device *d,
				struct device_attribute *attr, char *buf)
{
	struct msi_dev *msi = dev_get_drvdata(d);

	return sysfs_emit(buf, "%#x\n", READ_ONCE(msi->last_status));
}
static DEVICE_ATTR_RO(last_status);

static const char *irq_mode(struct pci_dev *pdev)
{
	if (pdev->msix_enabled)
		return "msix";
	if (pdev->msi_enabled)
		return "msi";
	return "intx";
}

static ssize_t mode_show(struct device *d, struct device_attribute *attr,
			 char *buf)
{
	struct msi_dev *msi = dev_get_drvdata(d);

	return sysfs_emit(buf, "%s %d\n", irq_mode(msi->pdev), msi->nvec);
}
static DEVICE_ATTR_RO(mode);

static struct attribute *msi_attrs[] = {
	&dev_attr_raise.attr,
	&dev_attr_interrupts.attr,
	&dev_attr_last_status.attr,
	&dev_attr_mode.attr,
	NULL
};
ATTRIBUTE_GROUPS(msi);

/*
 * Get either 3 MSI-X vectors, one per event, or a single vector of the
 * other types. pci_alloc_irq_vectors(pdev, 1, 3, types) in one call could
 * also give 1 or 2 MSI-X vectors, and with 2 the third event would have
 * nowhere to go.
 */
static int alloc_vectors(struct pci_dev *pdev, unsigned int types)
{
	int nvec;

	if (types & PCI_IRQ_MSIX) {
		nvec = pci_alloc_irq_vectors(pdev, LDD_NR_EVENTS,
					     LDD_NR_EVENTS, PCI_IRQ_MSIX);
		if (nvec > 0 || types == PCI_IRQ_MSIX)
			return nvec;
	}
	/*
	 * No MSI-X: one vector of the types that are left. Clearing the
	 * MSIX bit leaves the other bits as they were:
	 *
	 *	mode=intx: INTX          -> INTX
	 *	mode=msi:  MSI           -> MSI
	 *	mode=all:  MSIX|MSI|INTX -> MSI|INTX, MSI if it works, else INTx
	 *
	 * mode=msix never gets here: it returned above, vectors or error.
	 */
	return pci_alloc_irq_vectors(pdev, 1, 1, types & ~PCI_IRQ_MSIX);
}

static void free_irqs(struct msi_dev *msi, int n)
{
	while (n--)
		free_irq(msi->vec[n].irq, &msi->vec[n]);
}

/*
 * pcim_enable_device(), pcim_iomap_region() and devm_request_irq() are the
 * managed versions of the calls below: they are undone automatically when
 * probe() fails or after remove(). They are written out here to show each
 * step and its undo.
 */
static int msi_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	irq_handler_t handler;
	struct msi_dev *msi;
	unsigned long flags;
	int err, i;

	msi = kzalloc_obj(*msi);
	if (!msi)
		return -ENOMEM;
	msi->pdev = pdev;

	err = pci_enable_device(pdev);
	if (err)
		goto err_free;

	err = pci_request_regions(pdev, MODULE_NAME);
	if (err)
		goto err_disable;

	/*
	 * The MSI-X table is in BAR 0 too, at 0x800; the PCI core maps and
	 * fills it itself. The driver only uses the registers below 0x100.
	 */
	msi->regs = pci_iomap(pdev, 0, 0);
	if (!msi->regs) {
		err = -ENOMEM;
		goto err_regions;
	}
	if (ioread32(msi->regs + LDD_REG_ID) != LDD_ID) {
		err = -ENODEV;
		goto err_unmap;
	}

	/* an MSI is a memory write by the device: it must master the bus */
	pci_set_master(pdev);

	/* Older drivers called pci_enable_msi() or pci_enable_msix_range(). */
	msi->nvec = alloc_vectors(pdev, irq_types);
	if (msi->nvec < 0) {
		err = msi->nvec;
		goto err_master;
	}

	/*
	 * An MSI or MSI-X vector belongs to this device alone, an INTx line
	 * may be shared with others. pci_request_irq() picks the flag by
	 * itself.
	 */
	flags = pci_dev_msi_enabled(pdev) ? 0 : IRQF_SHARED;
	handler = msi->nvec == LDD_NR_EVENTS ? msi_vec_handler :
					       msi_handler;

	/* nothing must be pending from before, and nothing interrupts yet */
	iowrite32(0, msi->regs + LDD_REG_INT_MASK);
	iowrite32(LDD_INT_ALL, msi->regs + LDD_REG_INT_STATUS);

	for (i = 0; i < msi->nvec; i++) {
		struct msi_vec *vec = &msi->vec[i];

		vec->msi = msi;
		vec->bit = BIT(i);
		/* pci_irq_vector() turns vector i into a Linux IRQ number */
		vec->irq = pci_irq_vector(pdev, i);
		if (msi->nvec == LDD_NR_EVENTS)
			snprintf(vec->name, sizeof(vec->name), "%s-%s",
				 MODULE_NAME, event_names[i]);
		else
			strscpy(vec->name, MODULE_NAME);

		err = request_irq(vec->irq, handler, flags, vec->name, vec);
		if (err) {
			free_irqs(msi, i);
			goto err_vectors;
		}
	}

	pci_set_drvdata(pdev, msi);

	/* the handlers are in place: let the device interrupt */
	iowrite32(LDD_INT_ALL, msi->regs + LDD_REG_INT_MASK);

	dev_info(&pdev->dev, "using %s, %d vector%s, first irq %d\n",
		 irq_mode(pdev), msi->nvec, msi->nvec > 1 ? "s" : "",
		 msi->vec[0].irq);
	return 0;

err_vectors:
	pci_free_irq_vectors(pdev);
err_master:
	pci_clear_master(pdev);
err_unmap:
	pci_iounmap(pdev, msi->regs);
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
err_free:
	kfree(msi);
	return err;
}

static void msi_remove(struct pci_dev *pdev)
{
	struct msi_dev *msi = pci_get_drvdata(pdev);

	/* stop the device interrupting before its handlers go away */
	iowrite32(0, msi->regs + LDD_REG_INT_MASK);

	free_irqs(msi, msi->nvec);
	pci_free_irq_vectors(pdev);
	pci_clear_master(pdev);
	pci_iounmap(pdev, msi->regs);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	kfree(msi);
}

static struct pci_driver msi_driver = {
	.name       = MODULE_NAME,
	.id_table   = msi_ids,
	.probe      = msi_probe,
	.remove     = msi_remove,
	.dev_groups = msi_groups,
};

/*
 * module_pci_driver(msi_driver) would generate these two functions,
 * without the check of @mode.
 */
static int __init m_init(void)
{
	if (!strcmp(mode, "msix"))
		irq_types = PCI_IRQ_MSIX;
	else if (!strcmp(mode, "msi"))
		irq_types = PCI_IRQ_MSI;
	else if (!strcmp(mode, "intx"))
		irq_types = PCI_IRQ_INTX;
	else if (!strcmp(mode, "all"))
		irq_types = PCI_IRQ_ALL_TYPES;
	else
		return -EINVAL;	/* insmod fails, no driver is registered */

	return pci_register_driver(&msi_driver);
}

static void __exit m_exit(void)
{
	pci_unregister_driver(&msi_driver);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("MSI-X, MSI and INTx interrupts of the ldd-pcie device");
