#ifndef _FOPS_H
#define _FOPS_H

extern ssize_t proc_write(struct file *, const char __user *, size_t, loff_t *);
extern int proc_seq_open(struct inode *, struct file *);

#endif
