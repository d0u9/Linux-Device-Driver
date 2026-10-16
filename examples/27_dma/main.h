#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME			"dma"

#define PCI_KMOD_EDU_BAR_NUM		6
#define PCI_KMOD_EDU_MAX_IRQ_VEC	8
#define DMA_SIZE			(PAGE_SIZE >> 1)

struct irq_dev {
	dev_t devno;
	struct cdev cdev;
	struct pci_dev *pcidev;
	int irq_nr;
	int irqs[PCI_KMOD_EDU_MAX_IRQ_VEC];
	int bar_nr;
	dma_addr_t dma_addr; // TODO: name to xxx_bus
	void *dma_buf;
	void *__iomem bar[PCI_KMOD_EDU_BAR_NUM];
};

#endif
