/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FOPS_H
#define _FOPS_H

#include <linux/fs.h>

int ioctl_open(struct inode *inode, struct file *filp);
ssize_t ioctl_read(struct file *filp, char __user *buff, size_t count,
		   loff_t *f_pos);
long ioctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#endif
