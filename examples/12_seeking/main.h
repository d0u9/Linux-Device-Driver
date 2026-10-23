/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/cdev.h>
#include <linux/mutex.h>

#define MODULE_NAME		"seeking"
#define SEEKING_DEV_NR		1
#define HEX_DICT		"0123456789ABCDEF"
#define HEX_DICT_LEN		(sizeof(HEX_DICT) - 1)

struct seeking_dev {
	struct cdev cdev;
	struct mutex mutex;
	char buff[HEX_DICT_LEN];
};

#endif
