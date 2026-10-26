// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/pci.h>

#include "main.h"

/*
 * The devices this driver handles. The PCI core calls skel_probe() for
 * every device whose IDs match an entry. MODULE_DEVICE_TABLE() exports the
 * table as module aliases, so that udev or mdev can load the module when
 * such a device shows up.
 */
static const struct pci_device_id skel_ids[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, skel_ids);

/* Print what the configuration space tells about the device. */
static void skel_show_config(struct pci_dev *pdev)
{
	u8 revision, line, pin;

	pci_read_config_byte(pdev, PCI_REVISION_ID, &revision);
	pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, &pin);
	/*
	 * PCI_INTERRUPT_LINE is a scratch register filled in by the firmware
	 * for older systems. The interrupt the kernel really assigned is
	 * pdev->irq; drivers use that one.
	 */
	pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &line);

	dev_info(&pdev->dev, "vendor %04x device %04x revision %02x\n",
		 pdev->vendor, pdev->device, revision);
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

	for (int bar = 0; bar < PCI_STD_NUM_BARS; bar++) {
		if (!pci_resource_len(pdev, bar))
			continue;
		dev_info(&pdev->dev, "BAR%d: %pR\n", bar,
			 pci_resource_n(pdev, bar));
	}
}

/* Talk to the device through BAR 0 to show that the mapping works. */
static void skel_show_edu(struct pci_dev *pdev, void __iomem *regs)
{
	u32 id = ioread32(regs + EDU_REG_ID);
	u32 val;

	dev_info(&pdev->dev, "edu version %u.%u (id %#010x)\n",
		 id >> 24, (id >> 16) & 0xff, id);

	iowrite32(0x12345678, regs + EDU_REG_LIVENESS);
	val = ioread32(regs + EDU_REG_LIVENESS);
	dev_info(&pdev->dev, "liveness: wrote 0x12345678, read %#010x%s\n",
		 val, val == ~0x12345678U ? "" : " (wrong)");
}

static int skel_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	void __iomem *regs;
	int err;

	/*
	 * Enable, claim and map by hand, undoing each step on failure and in
	 * skel_remove(). The managed pcim_enable_device() and
	 * pcim_iomap_region() do the same and are undone automatically when
	 * the driver is unbound.
	 */
	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "cannot enable device: %d\n", err);
		return err;
	}

	/* claim the BARs: shows up as "pci_skel" in /proc/iomem */
	err = pci_request_regions(pdev, MODULE_NAME);
	if (err) {
		dev_err(&pdev->dev, "cannot claim BARs: %d\n", err);
		goto err_disable;
	}

	/* map all of BAR 0; works for memory and I/O BARs alike */
	regs = pci_iomap(pdev, 0, 0);
	if (!regs) {
		dev_err(&pdev->dev, "cannot map BAR 0\n");
		err = -ENOMEM;
		goto err_release;
	}

	/* keep the mapping for skel_remove() */
	pci_set_drvdata(pdev, regs);

	skel_show_config(pdev);
	skel_show_edu(pdev, regs);

	return 0;

err_release:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return err;
}

/* Called when the device goes away or the driver is unbound or unloaded. */
static void skel_remove(struct pci_dev *pdev)
{
	void __iomem *regs = pci_get_drvdata(pdev);

	pci_iounmap(pdev, regs);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
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
MODULE_DESCRIPTION("PCI driver skeleton for QEMU's edu device");
