/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FOPS_H
#define _FOPS_H

#include <linux/fs.h>

int seeking_open(struct inode *inode, struct file *filp);
ssize_t seeking_read(struct file *filp, char __user *buff, size_t count,
		     loff_t *f_pos);
loff_t seeking_llseek(struct file *filp, loff_t off, int whence);

#endif
