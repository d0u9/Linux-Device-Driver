// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include "main.h"

/*
 * The devices this driver handles. The PCI core calls skel_probe() for
 * every device whose IDs match an entry. MODULE_DEVICE_TABLE() exports the
 * table as module aliases, so that udev or mdev can load the module when
 * such a device shows up.
 */
static const struct pci_device_id skel_ids[] = {
	{ PCI_DEVICE(LDD_PCIE_VENDOR_ID, LDD_PCIE_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, skel_ids);

/* Print what the configuration space tells about the device. */
static void skel_show_config(struct pci_dev *pdev)
{
	u8 revision, line, pin;

	/* pdev->class is the 24-bit class code, read by the PCI core */
	pci_read_config_byte(pdev, PCI_REVISION_ID, &revision);
	pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, &pin);
	/*
	 * PCI_INTERRUPT_LINE is a scratch register filled in by the firmware
	 * for older systems. The interrupt the kernel really assigned is
	 * pdev->irq; drivers use that one.
	 */
	pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &line);

	dev_info(&pdev->dev, "vendor %04x device %04x revision %02x class %06x\n",
		 pdev->vendor, pdev->device, revision, pdev->class);
	dev_info(&pdev->dev, "interrupt pin %c, line %u, irq %u\n",
		 pin ? 'A' + pin - 1 : '-', line, pdev->irq);

	/*
	 * Walk the capability list for the PCI Express capability.
	 * pci_is_pcie() is the shorthand for this test.
	 */
	if (pci_find_capability(pdev, PCI_CAP_ID_EXP))
		dev_info(&pdev->dev, "PCI Express device\n");
	else
		dev_info(&pdev->dev, "conventional PCI device\n");

	/*
	 * A 64-bit BAR takes two of the six slots, the next one holding the
	 * upper half of the address: ldd-pcie's BAR 0 and BAR 2 are 64-bit,
	 * so BAR 1 and BAR 3 have no length of their own and are skipped.
	 */
	for (int bar = 0; bar < PCI_STD_NUM_BARS; bar++) {
		if (!pci_resource_len(pdev, bar))
			continue;
		dev_info(&pdev->dev, "BAR%d: %pR\n", bar,
			 pci_resource_n(pdev, bar));
	}
}

/*
 * Talk to the device to show that the mappings work: registers in BAR 0,
 * memory in BAR 2.
 */
static void skel_show_ldd_pcie(struct pci_dev *pdev, struct skel_dev *skel)
{
	u32 id = ioread32(skel->regs + LDD_REG_ID);
	u32 ver = ioread32(skel->regs + LDD_REG_VERSION);
	u32 caps = ioread32(skel->regs + LDD_REG_CAPS);
	u32 val;

	dev_info(&pdev->dev, "id %c%c%c%c (%#010x), version %u.%u, %s DMA\n",
		 id >> 24, id >> 16, id >> 8, id, id, ver >> 16, ver & 0xffff,
		 caps & LDD_CAPS_DMA64 ? "64-bit" : "32-bit");

	iowrite32(0x12345678, skel->regs + LDD_REG_SCRATCH);
	val = ioread32(skel->regs + LDD_REG_SCRATCH);
	dev_info(&pdev->dev, "scratch: wrote 0x12345678, read %#010x%s\n",
		 val, val == 0x12345678 ? "" : " (wrong)");

	/* BAR 2 is plain memory on the device, reached the same way */
	iowrite32(0xcafef00d, skel->mem + 0x100);
	val = ioread32(skel->mem + 0x100);
	dev_info(&pdev->dev, "on-board memory: wrote 0xcafef00d, read %#010x%s\n",
		 val, val == 0xcafef00d ? "" : " (wrong)");
}

static int skel_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct skel_dev *skel;
	int err;

	skel = kzalloc_obj(*skel);
	if (!skel)
		return -ENOMEM;

	/*
	 * Enable, claim and map by hand, undoing each step on failure and in
	 * skel_remove(). The managed pcim_enable_device() and
	 * pcim_iomap_region() do the same and are undone automatically when
	 * the driver is unbound; so is devm_kzalloc().
	 */
	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "cannot enable device: %d\n", err);
		goto err_free;
	}

	/* claim all the BARs: they show up as "pci_skel" in /proc/iomem */
	err = pci_request_regions(pdev, MODULE_NAME);
	if (err) {
		dev_err(&pdev->dev, "cannot claim BARs: %d\n", err);
		goto err_disable;
	}

	/* map all of a BAR; works for memory and I/O BARs alike */
	skel->regs = pci_iomap(pdev, 0, 0);
	if (!skel->regs) {
		dev_err(&pdev->dev, "cannot map BAR 0\n");
		err = -ENOMEM;
		goto err_release;
	}

	/* a device that is not ours, behind the same IDs, is left alone */
	if (ioread32(skel->regs + LDD_REG_ID) != LDD_ID) {
		dev_err(&pdev->dev, "not an ldd-pcie device\n");
		err = -ENODEV;
		goto err_unmap_regs;
	}

	skel->mem = pci_iomap(pdev, LDD_MEM_BAR, 0);
	if (!skel->mem) {
		dev_err(&pdev->dev, "cannot map BAR %d\n", LDD_MEM_BAR);
		err = -ENOMEM;
		goto err_unmap_regs;
	}

	/*
	 * probe's locals are gone when it returns, and remove() gets only
	 * @pdev. pci_set_drvdata() hangs one pointer on the device for it,
	 * pci_get_drvdata() gives it back. A global would not do: the driver
	 * may be bound to several devices at once.
	 */
	pci_set_drvdata(pdev, skel);

	skel_show_config(pdev);
	skel_show_ldd_pcie(pdev, skel);

	return 0;

err_unmap_regs:
	pci_iounmap(pdev, skel->regs);
err_release:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
err_free:
	kfree(skel);
	return err;
}

/* Called when the device goes away or the driver is unbound or unloaded. */
static void skel_remove(struct pci_dev *pdev)
{
	struct skel_dev *skel = pci_get_drvdata(pdev);

	pci_iounmap(pdev, skel->mem);
	pci_iounmap(pdev, skel->regs);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	kfree(skel);
}

static struct pci_driver skel_driver = {
	.name     = MODULE_NAME,
	.id_table = skel_ids,
	.probe    = skel_probe,
	.remove   = skel_remove,
};

/*
 * Loading the module only registers the driver; probing happens when a
 * matching device is found, now or later. module_pci_driver(skel_driver)
 * generates exactly these two functions.
 */
static int __init m_init(void)
{
	return pci_register_driver(&skel_driver);
}

static void __exit m_exit(void)
{
	pci_unregister_driver(&skel_driver);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("PCI driver skeleton for the ldd-pcie device");
