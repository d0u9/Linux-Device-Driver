/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SCULLD_H
#define _SCULLD_H

#include <linux/cdev.h>
#include <linux/device.h>

#include "lddbus.h"

#define MODULE_NAME		"sculld"
#define SCULLD_DEV_NR		2
#define DEVNAME_LEN		20

struct sculld_dev {
	char devname[DEVNAME_LEN];
	struct ldd_device ldev;		/* on the LDD bus */
	struct cdev cdev;
};

#endif
