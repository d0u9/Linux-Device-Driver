#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME			"msi_interrupt"

#define PCI_KMOD_EDU_BAR_NUM		6
#define PCI_KMOD_EDU_MAX_IRQ_VEC	8

struct irq_dev {
	dev_t devno;
	struct cdev cdev;
	struct pci_dev *pcidev;
	int irq_nr;
	int irqs[PCI_KMOD_EDU_MAX_IRQ_VEC];
	int bar_nr;
	void *__iomem bar[PCI_KMOD_EDU_BAR_NUM];
};

#endif
