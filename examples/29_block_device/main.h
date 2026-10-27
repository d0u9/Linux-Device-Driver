/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/blk-mq.h>
#include <linux/blkdev.h>

#define MODULE_NAME		"memdisk"
#define MEMDISK_MINORS		16	/* the disk and 15 partitions */

struct memdisk {
	int major;
	u64 size;			/* in bytes */
	u8 *data;			/* the disk's contents */
	struct blk_mq_tag_set tag_set;	/* blk-mq mode only */
	struct gendisk *gd;
};

#endif
