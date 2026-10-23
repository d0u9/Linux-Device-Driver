/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FOPS_H
#define _FOPS_H

#include <linux/fs.h>
#include <linux/poll.h>

int poll_open(struct inode *inode, struct file *filp);
ssize_t poll_read(struct file *filp, char __user *buff, size_t count,
		  loff_t *f_pos);
ssize_t poll_write(struct file *filp, const char __user *buff, size_t count,
		   loff_t *f_pos);
__poll_t poll_poll(struct file *filp, poll_table *wait);

#endif
