#ifndef _FOPS_H
#define _FOPS_H

extern int ioctl_open(struct inode *inode, struct file *filp);
extern ssize_t ioctl_read(struct file *filp, char __user *buff, size_t count,
			  loff_t *f_pos);
extern long ioctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);


#endif
