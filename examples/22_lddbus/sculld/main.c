#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>

#include "sculld.h"
#include "lddbus.h"

static struct sculld_dev *sculld_devices;
static int devno_major;

static struct ldd_driver sculld_driver = {
	.version = "$Driver version: 1.21 $",
	.module = THIS_MODULE,
	.driver = {
		.name = "sculld",
	},
};

static ssize_t devno_show(struct device *ddev, struct device_attribute *attr,
			  char *buf)
{
	struct sculld_dev *dev = dev_get_drvdata(ddev);
	return print_dev_t(buf, dev->cdev.dev);
}
static DEVICE_ATTR_RO(devno);

static struct file_operations sculld_fops = {
	.owner = THIS_MODULE,
};

static
int __init m_init(void)
{
	int ret, i;
	struct sculld_dev *dev;
	dev_t devno;

	ret = alloc_chrdev_region(&devno, 0, SCULLD_DEV_NR, MODULE_NAME);
	if (ret < 0)
		return ret;

	ret = register_ldd_driver(&sculld_driver);
	if (ret) {
		goto driver_fail;
		return ret;
	}
	devno_major = MAJOR(devno);

	sculld_devices = kzalloc(SCULLD_DEV_NR * sizeof(struct sculld_dev), GFP_KERNEL);
	if (!sculld_devices) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	for (i = 0; i < SCULLD_DEV_NR; i++) {
		dev = sculld_devices + i;
		snprintf(dev->devname, DEVNAME_LEN, "sculld%d", i);
		dev->ldev.name = dev->devname;
		dev->ldev.driver = &sculld_driver;
		dev_set_drvdata(&dev->ldev.dev, dev);

		devno = MKDEV(devno_major, i);
		dev->cdev.owner = THIS_MODULE;
		cdev_init(&dev->cdev, &sculld_fops);
		ret = cdev_add(&dev->cdev, devno, 1);
		device_create(ldd_class, NULL, devno, NULL, MODULE_NAME"%d", i);
		if (ret) {
			pr_err("Error %d adding scull%d\n", ret, i);
			continue;
		}

		register_ldd_device(&dev->ldev);
		device_create_file(&(dev->ldev.dev), &dev_attr_devno);
	}

	return 0;

driver_fail:
	unregister_chrdev_region(MKDEV(devno_major, 0), SCULLD_DEV_NR);
fail_malloc:
	unregister_ldd_driver(&sculld_driver);
	return ret;
}

static
void __exit m_exit(void)
{
	int i;
	for (i = 0; i < SCULLD_DEV_NR; i++) {
		device_remove_file(&sculld_devices[i].ldev.dev, &dev_attr_devno);
		unregister_ldd_device(&sculld_devices[i].ldev);
	}

	kfree(sculld_devices);
	unregister_ldd_driver(&sculld_driver);
	unregister_chrdev_region(MKDEV(devno_major, 0), SCULLD_DEV_NR);
}


module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("scull bus");
