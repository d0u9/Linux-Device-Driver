#ifndef _FOPS_H
#define _FOPS_H

extern int scull_open(struct inode *inode, struct file *filp);
extern int scull_release(struct inode *inode, struct file *filp);
extern ssize_t scull_read(struct file *filp, char __user *buff, size_t count,
			  loff_t *offp);
extern ssize_t scull_write(struct file *filp, const char __user *buff, size_t count,
			  loff_t *offp);
extern void scull_trim(struct scull_dev *dev);

#endif
