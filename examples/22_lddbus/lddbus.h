/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LDDBUS_H
#define _LDDBUS_H

#include <linux/device.h>

/* The virtual LDD bus, and the class its devices' /dev nodes go in. */
extern const struct bus_type ldd_bus_type;
extern struct class *ldd_class;

/* A driver for devices on the LDD bus. */
struct ldd_driver {
	char *version;
	struct device_driver driver;
};

#define to_ldd_driver(drv) container_of(drv, struct ldd_driver, driver)

/* A device "plugged" into the LDD bus. */
struct ldd_device {
	char *name;
	struct device dev;
};

#define to_ldd_device(d) container_of(d, struct ldd_device, dev)

int register_ldd_device(struct ldd_device *ldddev);
void unregister_ldd_device(struct ldd_device *ldddev);
int register_ldd_driver(struct ldd_driver *driver);
void unregister_ldd_driver(struct ldd_driver *driver);

#endif
