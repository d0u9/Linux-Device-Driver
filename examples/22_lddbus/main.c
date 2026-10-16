#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>

#include "main.h"
#include "lddbus.h"

static char *Version = "$Revision: 1.9 $";

struct class *ldd_class;
EXPORT_SYMBOL(ldd_class);

/*
 * BUS type
 */
static int ldd_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	if (add_uevent_var(env, "LDDBUS_VERSION=%s", Version))
		return -ENOMEM;

	return 0;
}

static ssize_t bus_version_show(struct bus_type *bus, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", Version);
}
static BUS_ATTR_RO(bus_version);

static void ldd_bus_release(struct device *dev)
{
	pr_debug("lddbus release\n");
}

struct device ldd_bus = {
	.release = ldd_bus_release,
};

/*
 * BUS Device
 */
static int ldd_match(struct device *dev, struct device_driver *driver)
{
	return !strncmp(dev_name(dev), driver->name, strlen(driver->name));
}

static ssize_t dev_info_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", dev_name(dev));
}
static DEVICE_ATTR_RO(dev_info);

struct bus_type ldd_bus_type = {
	.name	= "ldd",
	.match	= ldd_match,
	.uevent = ldd_uevent,
};


/*
 * Device
 */
static void ldd_dev_release(struct device *dev)
{

}

int register_ldd_device(struct ldd_device *ldddev)
{
	ldddev->dev.bus = &ldd_bus_type;
	ldddev->dev.parent = &ldd_bus;
	ldddev->dev.release = ldd_dev_release;
	dev_set_name(&ldddev->dev, ldddev->name);
	return device_register(&ldddev->dev);
}
EXPORT_SYMBOL(register_ldd_device);

void unregister_ldd_device(struct ldd_device *ldddev)
{
	device_unregister(&ldddev->dev);
}
EXPORT_SYMBOL(unregister_ldd_device);

/*
 * Driver
 */
static ssize_t show_version(struct device_driver *driver, char *buf)
{
	struct ldd_driver *ldriver = to_ldd_driver(driver);
	return snprintf(buf, PAGE_SIZE, "%s\n", ldriver->version);
}

int register_ldd_driver(struct ldd_driver *driver)
{
	int ret;

	driver->driver.bus = &ldd_bus_type;
	ret = driver_register(&driver->driver);
	if (ret)
		return ret;

	driver->version_attr.attr.name = "version";
	driver->version_attr.attr.mode = S_IRUGO;
	driver->version_attr.show = show_version;
	driver->version_attr.store = NULL;
	return driver_create_file(&driver->driver, &driver->version_attr);
}
EXPORT_SYMBOL(register_ldd_driver);

void unregister_ldd_driver(struct ldd_driver *driver)
{
	driver_remove_file(&driver->driver, &driver->version_attr);
	driver_unregister(&driver->driver);
}
EXPORT_SYMBOL(unregister_ldd_driver);


/*
 * Init and remove
 */
static
int __init m_init(void)
{
	int ret;

	ret = bus_register(&ldd_bus_type);
	if (ret) {
		pr_err("Unable to register ldd bus, failure was %d\n", ret);
		return ret;
	}

	ldd_class = class_create(THIS_MODULE, "ldd");
	if (!ldd_class) {
		pr_err("Unable to create ldd class\n");
		goto fail1;
	}

	if (bus_create_file(&ldd_bus_type, &bus_attr_bus_version))
		pr_err("Unable to create version attribute\n");

	dev_set_name(&ldd_bus, "ldd0");
	ret = device_register(&ldd_bus);
	if (ret) {
		pr_err("Unable to register ldd0, failure was %d\n", ret);
		goto fail2;
	}

	if (device_create_file(&ldd_bus, &dev_attr_dev_info))
		pr_err("Unable to create dev info attribute\n");

	return 0;
fail2:
	class_destroy(ldd_class);

fail1:
	bus_unregister(&ldd_bus_type);
	return ret;
}

static
void __exit m_exit(void)
{
	device_remove_file(&ldd_bus, &dev_attr_dev_info);
	device_unregister(&ldd_bus);
	// TODO: check before free
	bus_remove_file(&ldd_bus_type, &bus_attr_bus_version);
	class_destroy(ldd_class);
	bus_unregister(&ldd_bus_type);
}


module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("BUS");
