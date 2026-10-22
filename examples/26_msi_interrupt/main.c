#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/delay.h>

#include "main.h"

#define PCI_VENDOR_ID_QEMU		0x1234
#define PCI_KMOD_EDU_VENDOR_ID		PCI_VENDOR_ID_QEMU
#define PCI_KMOD_EDU_DEVICE_ID		0x7863

static struct pci_device_id ids[] = {
	{ PCI_DEVICE(PCI_KMOD_EDU_VENDOR_ID, PCI_KMOD_EDU_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, ids);

static int dev_open(struct inode *inode, struct file *filp)
{
	struct irq_dev *irq_dev;
	irq_dev = container_of(inode->i_cdev, struct irq_dev, cdev);

	iowrite32(1, irq_dev->bar[0] + 0x60);

	return 0;
}

static int dev_release(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);
	return 0;
}

static struct file_operations fops = {
	.owner   = THIS_MODULE,
	.open    = dev_open,
	.release = dev_release,
};

static int create_chrdev(struct irq_dev *irq_dev)
{
	int rv;

	rv = alloc_chrdev_region(&irq_dev->devno, 0, 1, MODULE_NAME);
	if (rv < 0) {
		pr_err("cannot get major number!\n");
		return rv;
	}

	cdev_init(&irq_dev->cdev, &fops);
	irq_dev->cdev.owner = THIS_MODULE;

	rv = cdev_add(&irq_dev->cdev, irq_dev->devno, 1);
	if (rv) {
		pr_debug("Error(%d): Adding %s error\n", rv, MODULE_NAME);
		unregister_chrdev_region(irq_dev->devno, 1);
		return rv;
	}



	return 0;
}

static void destroy_chrdev(struct irq_dev *irq_dev)
{
	cdev_del(&irq_dev->cdev);
	unregister_chrdev_region(irq_dev->devno, 1);
}

static irqreturn_t irq_service(int irq, void *dev_id)
{
	pr_debug("irq triggered: irq = %d\n", irq);

	return IRQ_HANDLED;
}

static void irq_free(struct irq_dev *irq_dev)
{
	int i;
	for (i = 0; i < irq_dev->irq_nr; i++) {
		if (irq_dev->irqs[i])
			free_irq(irq_dev->irqs[i], irq_dev->irqs + i);
	}
	pci_free_irq_vectors(irq_dev->pcidev);
}

static int irq_alloc(struct irq_dev *irq_dev)
{
	int i, irq, rv, nvec = PCI_KMOD_EDU_MAX_IRQ_VEC;

	nvec = pci_alloc_irq_vectors(irq_dev->pcidev,
				     1, nvec, PCI_IRQ_ALL_TYPES);
	if (nvec < 0) {
		pr_err("pci_alloc_irq_vectors failed!\n");
		return nvec;
	}
	pr_debug("irq vec number = %d\n", nvec);
	irq_dev->irq_nr = nvec;

	for (i = 0; i < nvec; i++) {
		irq = pci_irq_vector(irq_dev->pcidev, i);
		rv = request_irq(irq, irq_service, 0, MODULE_NAME, irq_dev->irqs + i);
		pr_debug("request_irq(%d) == %d\n", irq, rv);
		if (rv < 0)
			goto fail;
		irq_dev->irqs[i] = irq;
	}

	return 0;
fail:
	irq_free(irq_dev);
	return rv;
}

static void unmap_bars(struct irq_dev *irq_dev)
{
	int i;
	for (i = 0; i < irq_dev->bar_nr; i++) {
		pci_iounmap(irq_dev->pcidev, irq_dev->bar[i]);
	}
}

static int map_single_bar(struct irq_dev *irq_dev, int idx)
{
	resource_size_t bar_start;
	resource_size_t bar_len;
	void *__iomem bar_addr;

	bar_start = pci_resource_start(irq_dev->pcidev, idx);
	bar_len = pci_resource_len(irq_dev->pcidev, idx);

	if (bar_len == 0) {
		return bar_len;
	}

	bar_addr = pci_iomap(irq_dev->pcidev, idx, bar_len);
	if (!bar_addr)
		return -ENOMEM;

	pr_info("BAR%d at 0x%llx mapped at 0x%px, length=%llu(/%llx)\n", idx,
		(u64)bar_start, bar_addr, (u64)bar_len, (u64)bar_len);

	irq_dev->bar[irq_dev->bar_nr++] = bar_addr;

	return (int)bar_len;
}

static int map_bars(struct irq_dev *irq_dev)
{
	int i, rv;
	int bar_len;

	for (i = 0; i < PCI_KMOD_EDU_BAR_NUM; i++) {
		bar_len = map_single_bar(irq_dev, i);
		if (bar_len == 0) {
			continue;
		} else if (bar_len < 0) {
			rv = bar_len;
			goto fail;
		}
	}

	return 0;

fail:
	unmap_bars(irq_dev);
	return rv;
}

static int probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int rv;
	u8 irq;
	struct irq_dev *irq_dev;

	pr_debug("New pci device probing!\n");

	if (pci_enable_device(dev)) {
		dev_err(&dev->dev, "can't enable PCI device\n");
		return -ENODEV;
	}
	pci_set_master(dev);

	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
	pr_debug("IRQ: %d\n", irq);
	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &irq);
	pr_debug("IRQ: %d\n", irq);

	irq_dev = kzalloc(sizeof(struct irq_dev), GFP_KERNEL);
	if (!irq_dev) {
		pr_err("Allocate memory for 'struct irq_dev' failed!\n");
		rv = -ENOMEM;
		goto alloc_err;
	}
	irq_dev->pcidev = dev;
	dev_set_drvdata(&dev->dev, irq_dev);

	rv = pci_request_regions(dev, MODULE_NAME);
	if (rv) {
		dev_err(&dev->dev, "can't request regions\n");
		goto request_regions_err;
	}

	rv = map_bars(irq_dev);
	if (rv < 0) {
		dev_err(&dev->dev, "can't map bars\n");
		goto map_bars_err;
	}

	rv = dma_set_mask_and_coherent(&dev->dev, DMA_BIT_MASK(64));
	if (rv) {
		dev_err(&dev->dev, "can't set DMA mask\n");
		goto dma_mask_err;
	}

	rv = irq_alloc(irq_dev);
	if (rv < 0) {
		dev_err(&dev->dev, "can't alloc irq\n");
		goto irq_err;
	}

	rv = create_chrdev(irq_dev);
	if (rv < 0) {
		dev_err(&dev->dev, "can't create char dev\n");
		goto create_chrdev_err;
	}

	return 0;

create_chrdev_err:
	irq_free(irq_dev);
irq_err:
dma_mask_err:
	unmap_bars(irq_dev);
map_bars_err:
	pci_release_regions(dev);
request_regions_err:
	dev_set_drvdata(&dev->dev, NULL);
	kfree(irq_dev);
alloc_err:
	pci_disable_device(dev);
	return rv;
}

static void remove(struct pci_dev *dev)
{
	struct irq_dev *irq_dev;
	irq_dev = dev_get_drvdata(&dev->dev);

	destroy_chrdev(irq_dev);
	irq_free(irq_dev);
	unmap_bars(irq_dev);
	pci_release_regions(dev);
	dev_set_drvdata(&dev->dev, NULL);
	kfree(irq_dev);
	pci_disable_device(dev);
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
