// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sysfs.h>

#include "lddbus.h"

static const char *version = "1.9";

struct class *ldd_class;
EXPORT_SYMBOL(ldd_class);

/*
 * The root device, /sys/devices/ldd0, parent of every device on the bus.
 * root_device_register("ldd0") allocates and registers such a device in
 * one call; it is written out here to show what a device needs.
 */
static void ldd_root_release(struct device *dev)
{
	/* static, nothing to free */
}

static ssize_t dev_info_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	return sysfs_emit(buf, "%s\n", dev_name(dev));
}
static DEVICE_ATTR_RO(dev_info);

static struct attribute *ldd_root_attrs[] = {
	&dev_attr_dev_info.attr,
	NULL
};
ATTRIBUTE_GROUPS(ldd_root);

static struct device ldd_root = {
	.release = ldd_root_release,
	.groups  = ldd_root_groups,
};

/*
 * The bus. Its attributes are given as groups, which the driver core
 * creates before announcing the bus, device or driver to user space.
 * Creating them afterwards with bus_create_file(), device_create_file()
 * or driver_create_file() races with udev, which may look for them first.
 */
static ssize_t bus_version_show(const struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%s\n", version);
}
static BUS_ATTR_RO(bus_version);

static struct attribute *ldd_bus_attrs[] = {
	&bus_attr_bus_version.attr,
	NULL
};
ATTRIBUTE_GROUPS(ldd_bus);

/* /sys/bus/ldd/drivers/<name>/version, for every driver on the bus */
static ssize_t version_show(struct device_driver *driver, char *buf)
{
	struct ldd_driver *ldriver = to_ldd_driver(driver);

	return sysfs_emit(buf, "%s\n", ldriver->version);
}
static DRIVER_ATTR_RO(version);

static struct attribute *ldd_drv_attrs[] = {
	&driver_attr_version.attr,
	NULL
};
ATTRIBUTE_GROUPS(ldd_drv);

/* A driver handles a device whose name starts with the driver's name. */
static int ldd_match(struct device *dev, const struct device_driver *driver)
{
	return !strncmp(dev_name(dev), driver->name, strlen(driver->name));
}

static int ldd_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	return add_uevent_var(env, "LDDBUS_VERSION=%s", version);
}

const struct bus_type ldd_bus_type = {
	.name       = "ldd",
	.match      = ldd_match,
	.uevent     = ldd_uevent,
	.bus_groups = ldd_bus_groups,
	.drv_groups = ldd_drv_groups,
};
EXPORT_SYMBOL(ldd_bus_type);

/*
 * Devices. The ldd_device is embedded in the caller's structure, which the
 * caller frees after unregister_ldd_device(), so release has nothing to do.
 */
static void ldd_dev_release(struct device *dev)
{
}

int register_ldd_device(struct ldd_device *ldddev)
{
	int err;

	ldddev->dev.bus = &ldd_bus_type;
	ldddev->dev.parent = &ldd_root;
	ldddev->dev.release = ldd_dev_release;
	err = dev_set_name(&ldddev->dev, "%s", ldddev->name);
	if (err)
		return err;

	err = device_register(&ldddev->dev);
	if (err)
		put_device(&ldddev->dev);	/* needed even when it fails */
	return err;
}
EXPORT_SYMBOL(register_ldd_device);

void unregister_ldd_device(struct ldd_device *ldddev)
{
	device_unregister(&ldddev->dev);
}
EXPORT_SYMBOL(unregister_ldd_device);

/* Drivers */
int register_ldd_driver(struct ldd_driver *driver)
{
	driver->driver.bus = &ldd_bus_type;
	return driver_register(&driver->driver);
}
EXPORT_SYMBOL(register_ldd_driver);

void unregister_ldd_driver(struct ldd_driver *driver)
{
	driver_unregister(&driver->driver);
}
EXPORT_SYMBOL(unregister_ldd_driver);

static int __init m_init(void)
{
	int err;

	err = bus_register(&ldd_bus_type);
	if (err)
		return err;

	ldd_class = class_create("ldd");
	if (IS_ERR(ldd_class)) {
		err = PTR_ERR(ldd_class);
		goto err_bus;
	}

	err = dev_set_name(&ldd_root, "ldd0");
	if (err)
		goto err_class;
	err = device_register(&ldd_root);
	if (err) {
		put_device(&ldd_root);
		goto err_class;
	}

	return 0;

err_class:
	class_destroy(ldd_class);
err_bus:
	bus_unregister(&ldd_bus_type);
	return err;
}

static void __exit m_exit(void)
{
	device_unregister(&ldd_root);
	class_destroy(ldd_class);
	bus_unregister(&ldd_bus_type);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("A virtual bus for the device model");
