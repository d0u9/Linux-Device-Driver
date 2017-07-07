#ifndef _FOPS_H
#define _FOPS_H

#include <linux/poll.h>

extern int poll_open(struct inode *inode, struct file *filp);
extern ssize_t poll_read(struct file *filp, char __user *buff,
				 size_t count, loff_t *f_pos);
extern ssize_t poll_write(struct file *filp, const char __user *buff,
				  size_t count, loff_t *f_pos);
extern unsigned int poll_poll(struct file *filp, poll_table *wait);

#endif
