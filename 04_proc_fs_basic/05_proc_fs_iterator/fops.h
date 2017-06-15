#ifndef _FOPS_H
#define _FOPS_H

extern int proc_open(struct inode *, struct file *);
extern ssize_t proc_write(struct file *, const char __user *, size_t, loff_t *);

#endif
