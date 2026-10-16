#ifndef _FOPS_H
#define _FOPS_H

extern int seeking_open(struct inode *inode, struct file *filp);
extern ssize_t seeking_read(struct file *filp, char __user * buff, size_t count,
			 loff_t * f_pos);
extern loff_t seeking_llseek(struct file *filp, loff_t off, int whence);

#endif
