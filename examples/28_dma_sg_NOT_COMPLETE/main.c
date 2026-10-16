#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/delay.h>

#include "main.h"
#include "ioctl_cmd.h"

#define PCI_VENDOR_ID_QEMU		0x1234
#define PCI_KMOD_EDU_VENDOR_ID		PCI_VENDOR_ID_QEMU
// #define PCI_KMOD_EDU_DEVICE_ID		0x7863
#define PCI_KMOD_EDU_DEVICE_ID		0x11e8

static struct pci_device_id ids[] = {
	{ PCI_DEVICE(PCI_KMOD_EDU_VENDOR_ID, PCI_KMOD_EDU_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, ids);

static
int dev_open(struct inode *inode, struct file *filp)
{
	filp->private_data = container_of(inode->i_cdev, struct irq_dev, cdev);

	return 0;
}

static
struct scatterlist *create_sg_list(int nents)
{
	int i;
	struct scatterlist *sg;
	struct page *page;

	sg = kmalloc_array(nents, sizeof(struct scatterlist), GFP_KERNEL);
	if (!sg) {
		pr_err("Allocate memory for scatterlist failed!\n");
		return NULL;
	}

	sg_init_table(sg, nents);
	for (i = 0; i < nents; i++) {
		page = alloc_pages(GFP_KERNEL, i);
		sg_set_page(sg + i, page, PAGE_SIZE * (i + 1), 0);
	}

	return sg;
}

static
void free_sg_list(int nents, struct scatterlist *sg)
{
	int i;
	struct page *page;
	for (i = 0; i < nents; i++) {
		page = sg_page(sg + i);
		__free_pages(page, i);
	}

	kfree(sg);
}

static
long dma_to(struct irq_dev *irq_dev, void __user *arg, bool to_dev)
{
	int i, rv, count;
	struct ioc_arg ioc_arg;
	struct scatterlist *sg;

	pr_debug("dma_to_dev invoked!\n");

	rv = copy_from_user(&ioc_arg, arg, sizeof(struct ioc_arg));
	if (rv) {
		pr_err("copy from user faild!\n");
		return -EFAULT;
	}

	if (ioc_arg.len > DMA_SIZE) {
		pr_err("Data size overflow!\n");
		return -EFAULT;
	}

	sg = create_sg_list(3);
	count = dma_map_sg(irq_dev->pci_dev->dev, sg, 3,
			   to_dev ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
	pr_debug("---- actual sg len: %d\n", count);
	
	if (to_dev) {
		// src
		iowrite32(cpu_to_le32(irq_dev->dma_addr), irq_dev->bar[0] + 0x80);
		// dst
		iowrite32(0x40000, irq_dev->bar[0] + 0x88);
		iowrite32(cpu_to_le32(DMA_SIZE), irq_dev->bar[0] + 0x90);
		iowrite32(0x04 | 0x01, irq_dev->bar[0] + 0x98);
	} else {
		// src
		iowrite32(0x40000, irq_dev->bar[0] + 0x80);
		// dst
		iowrite32(cpu_to_le32(irq_dev->dma_addr), irq_dev->bar[0] + 0x88);
		iowrite32(cpu_to_le32(DMA_SIZE), irq_dev->bar[0] + 0x90);
		iowrite32(0x04 | 0x02 | 0x01, irq_dev->bar[0] + 0x98);
	}

	return 0;
}

static
long dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rv = 0;
	struct irq_dev *irq_dev = filp->private_data;

	pr_debug("dev_ioctl invoked()!\n");

	if (_IOC_TYPE(cmd) != IOC_MAGIC) {
		pr_err("ioctl() command error!\n");
		return -ENOTTY;
	}

	rv = access_ok(arg, _IOC_SIZE(cmd));
	if (!rv)
		return -EFAULT;

	switch (cmd) {
	case IOC_DMA_TO_DEV:
		rv = dma_to(irq_dev, (void *__user)arg, true);
		break;
	case IOC_DMA_TO_RAM:
		rv = dma_to(irq_dev, (void *__user)arg, false);
		break;
	default:
		pr_err("invalid ioctl() command!\n");
		return -ENOTTY;
	}

	return rv;
}

int dev_release(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);
	return 0;
}

static struct file_operations fops = {
	.owner   = THIS_MODULE,
	.open    = dev_open,
	.unlocked_ioctl = dev_ioctl,
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
	unregister_chrdev_region(irq_dev->devno, 1);
}

irqreturn_t irq_service(int irq, void *dev_id)
{
	struct irq_dev *irq_dev = (struct irq_dev*)(dev_id);
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

	for (i = 0; i < nvec; i++) {
		irq = pci_irq_vector(irq_dev->pcidev, i);
		rv = request_irq(irq, irq_service, 0, MODULE_NAME, irq_dev->irqs + i);
		pr_debug("request_irq(%d) == %d\n", irq, rv);
		if (rv < 0)
			goto fail;
		irq_dev->irqs[i] = irq;
	}

	irq_dev->irq_nr = nvec;

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
			rv = -EINVAL;
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
		return -ENOMEM;
	}
	irq_dev->pcidev = dev;

	rv = map_bars(irq_dev);
	if (rv < 0) {
		dev_err(&dev->dev, "can't map bars\n");
		goto map_bars_err;
	}

	rv = pci_request_regions(dev, MODULE_NAME);
	pr_err("ERROR pci_request_region: %d\n", rv);

	rv = pci_set_dma_mask(dev, DMA_BIT_MASK(28));
	pr_err("ERROR pci_set_dma_mask: %d\n", rv);

	rv = pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(28));
	pr_err("ERROR  pci_set_consistent_dma_mask: %d\n", rv);


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

	dev_set_drvdata(&dev->dev, irq_dev);

	return 0;

create_chrdev_err:
	unmap_bars(irq_dev);
map_bars_err:
	irq_free(irq_dev);
irq_err:
	kfree(irq_dev);
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
	kfree(irq_dev);

	dev_set_drvdata(&dev->dev, NULL);
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
