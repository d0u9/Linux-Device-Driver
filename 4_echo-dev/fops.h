#ifndef _FOPS_H
#define _FOPS_H

int echo_open(struct inode *inode, struct file *filp);
ssize_t echo_write(struct file *filp, const char __user *buf, size_t count,
		   loff_t *f_pos);
ssize_t echo_read(struct file *filp, char __user *buf, size_t count,
		  loff_t *f_pos);

#endif
