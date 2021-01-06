#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>

#include "main.h"

#define PCI_VENDOR_ID_QEMU		0x1234
#define PCI_KMOD_EDU_VENDOR_ID		PCI_VENDOR_ID_QEMU
#define PCI_KMOD_EDU_DEVICE_ID		0x7863

#define PCI_KMOD_EDU_BAR_NUM		6

static struct pci_device_id ids[] = {
	{ PCI_DEVICE(PCI_KMOD_EDU_VENDOR_ID, PCI_KMOD_EDU_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, ids);

static int map_single_bar(struct pci_dev *dev, int idx)
{
	resource_size_t bar_start;
	resource_size_t bar_len;
	resource_size_t map_len;

	bar_start = pci_resource_start(dev, idx);
	bar_len = pci_resource_len(dev, idx);
	map_len = bar_len;

	if (!bar_len) {
		return 0;
	}

	pr_info("BAR%d at 0x%llx mapped at 0x%p, length=%llu(/%llx)\n", idx,
		(u64)bar_start, NULL, (u64)map_len, (u64)bar_len);

	return (int)map_len;
}

static int map_bars(struct pci_dev *dev)
{
	int i, rv;
	int bar_len;

	for (i = 0; i < PCI_KMOD_EDU_BAR_NUM; i++) {
		bar_len = map_single_bar(dev, i);
		if (bar_len == 0) {
			continue;
		} else if (bar_len < 0) {
			rv = -EINVAL;
			goto fail;
		}
	}

	return 0;

fail:
	// unmap_bars(xdev, dev);
	return rv;
}

static int probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int rv;
	u8 revision, irq;
	u16 cap;

	pr_debug("New pci device probing!\n");

	if (pci_enable_device(dev)) {
		dev_err(&dev->dev, "can't enable PCI device\n");
		return -ENODEV;
	}

	pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
	pr_debug("Revision: %d\n", revision);

	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
	pr_debug("IRQ: %d\n", irq);
	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &irq);
	pr_debug("IRQ: %d\n", irq);

	if (pci_find_capability(dev, PCI_CAP_ID_EXP)) {
		pr_debug("CAP PCI_CAP_ID_EXP: on\n");
		pcie_capability_read_word(dev, PCI_EXP_FLAGS, &cap);
		pr_debug("PCI_EXP_FLAGS: %x, PCI_EXP_FLAGS_VERS=%d\n",
			 cap, cap & PCI_EXP_FLAGS_VERS);
	} else {
		pr_debug("CAP PCI_CAP_ID_EXP: off\n");
	}

	rv = map_bars(dev);
	if (rv < 0) {
		dev_err(&dev->dev, "can't map bars\n");
		return rv;
	}

	return 0;
}

static void remove(struct pci_dev *dev)
{

}

static struct pci_driver pci_driver = {
	.name		= MODULE_NAME,
	.id_table	= ids,
	.probe		= probe,
	.remove		= remove,
};

static
int __init m_init(void)
{
	return pci_register_driver(&pci_driver);
}

static
void __exit m_exit(void)
{
	pci_unregister_driver(&pci_driver);
}


module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("PCI Driver skel");
