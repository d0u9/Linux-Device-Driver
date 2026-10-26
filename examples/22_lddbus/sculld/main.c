// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#include "sculld.h"

static struct sculld_dev *sculld_devices;
static int devno_major;

static struct ldd_driver sculld_driver = {
	.version = "1.21",
	.driver = {
		.name  = "sculld",
		.owner = THIS_MODULE,
	},
};

/* /sys/bus/ldd/devices/sculldN/devno: the char device number */
static ssize_t devno_show(struct device *ddev, struct device_attribute *attr,
			  char *buf)
{
	struct ldd_device *ldev = to_ldd_device(ddev);
	struct sculld_dev *dev = container_of(ldev, struct sculld_dev, ldev);

	return sysfs_emit(buf, "%u:%u\n", MAJOR(dev->cdev.dev),
			  MINOR(dev->cdev.dev));
}
static DEVICE_ATTR_RO(devno);

static struct attribute *sculld_dev_attrs[] = {
	&dev_attr_devno.attr,
	NULL
};
ATTRIBUTE_GROUPS(sculld_dev);

/* No file operations: this example is about where the device shows up. */
static const struct file_operations sculld_fops = {
	.owner = THIS_MODULE,
};

/*
 * Put device i on the LDD bus, add its char device and create /dev/sculldN.
 * The node's parent is the bus device, so /sys/class/ldd/sculldN links to
 * it under /sys/devices/ldd0/.
 */
static int sculld_setup(struct sculld_dev *dev, int i)
{
	dev_t devno = MKDEV(devno_major, i);
	struct device *node;
	int err;

	snprintf(dev->devname, sizeof(dev->devname), "sculld%d", i);
	dev->ldev.name = dev->devname;
	dev->ldev.dev.groups = sculld_dev_groups;
	err = register_ldd_device(&dev->ldev);
	if (err)
		return err;

	cdev_init(&dev->cdev, &sculld_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		goto err_ldev;

	node = device_create(ldd_class, &dev->ldev.dev, devno, NULL, "%s",
			     dev->devname);
	if (IS_ERR(node)) {
		err = PTR_ERR(node);
		goto err_cdev;
	}

	return 0;

err_cdev:
	cdev_del(&dev->cdev);
err_ldev:
	unregister_ldd_device(&dev->ldev);
	return err;
}

static void sculld_cleanup(struct sculld_dev *dev)
{
	device_destroy(ldd_class, dev->cdev.dev);
	cdev_del(&dev->cdev);
	unregister_ldd_device(&dev->ldev);
}

static int __init m_init(void)
{
	dev_t devno;
	int err, i;

	err = alloc_chrdev_region(&devno, 0, SCULLD_DEV_NR, MODULE_NAME);
	if (err)
		return err;
	devno_major = MAJOR(devno);

	err = register_ldd_driver(&sculld_driver);
	if (err)
		goto err_region;

	sculld_devices = kzalloc_objs(*sculld_devices, SCULLD_DEV_NR);
	if (!sculld_devices) {
		err = -ENOMEM;
		goto err_driver;
	}

	for (i = 0; i < SCULLD_DEV_NR; i++) {
		err = sculld_setup(&sculld_devices[i], i);
		if (err)
			goto err_devices;
	}

	return 0;

err_devices:
	while (i--)
		sculld_cleanup(&sculld_devices[i]);
	kfree(sculld_devices);
err_driver:
	unregister_ldd_driver(&sculld_driver);
err_region:
	unregister_chrdev_region(MKDEV(devno_major, 0), SCULLD_DEV_NR);
	return err;
}

static void __exit m_exit(void)
{
	int i;

	for (i = 0; i < SCULLD_DEV_NR; i++)
		sculld_cleanup(&sculld_devices[i]);
	kfree(sculld_devices);
	unregister_ldd_driver(&sculld_driver);
	unregister_chrdev_region(MKDEV(devno_major, 0), SCULLD_DEV_NR);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("scull devices on the LDD bus");
