#ifndef _FOPS_H
#define _FOPS_H

extern int pipe_open(struct inode *inode, struct file *filp);
extern ssize_t pipe_read(struct file *filp, char __user * buff, size_t count,
			 loff_t * f_pos);
extern ssize_t pipe_write(struct file *filp, const char __user * buff,
			  size_t count, loff_t * f_pos);

#endif
