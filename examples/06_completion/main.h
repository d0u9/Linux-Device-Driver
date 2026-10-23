/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/cdev.h>
#include <linux/completion.h>

#define MODULE_NAME	"completion"

struct completion_dev {
	struct cdev cdev;
	struct completion completion;
};

#endif
