#ifndef _MAIN_H
#define _MAIN_H

#include <linux/cdev.h>
#include <linux/fs.h>
#include "lddbus.h"

#define MODULE_NAME		"sculld"
#define SCULLD_DEV_NR		1
#define DEVNAME_LEN		20

struct sculld_dev {
	struct cdev cdev;
	char devname[DEVNAME_LEN];
	struct ldd_device ldev;
};

#endif
