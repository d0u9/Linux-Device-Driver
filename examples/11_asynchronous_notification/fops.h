/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FOPS_H
#define _FOPS_H

#include <linux/fs.h>

int async_notify_open(struct inode *inode, struct file *filp);
int async_notify_release(struct inode *inode, struct file *filp);
ssize_t async_notify_read(struct file *filp, char __user *buff, size_t count,
			  loff_t *f_pos);
ssize_t async_notify_write(struct file *filp, const char __user *buff,
			   size_t count, loff_t *f_pos);
int async_notify_fasync(int fd, struct file *filp, int mode);

#endif
