/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FOPS_H
#define _FOPS_H

#include <linux/fs.h>

int pipe_open(struct inode *inode, struct file *filp);
ssize_t pipe_read(struct file *filp, char __user *buff, size_t count,
		  loff_t *f_pos);
ssize_t pipe_write(struct file *filp, const char __user *buff, size_t count,
		   loff_t *f_pos);

#endif
