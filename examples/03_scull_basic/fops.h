/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FOPS_H
#define _FOPS_H

#include <linux/fs.h>

#include "main.h"

int scull_open(struct inode *inode, struct file *filp);
int scull_release(struct inode *inode, struct file *filp);
ssize_t scull_read(struct file *filp, char __user *buff, size_t count,
		   loff_t *f_pos);
ssize_t scull_write(struct file *filp, const char __user *buff, size_t count,
		    loff_t *f_pos);
void scull_trim(struct scull_dev *dev);

#endif
