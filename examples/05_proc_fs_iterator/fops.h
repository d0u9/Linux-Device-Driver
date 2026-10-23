/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FOPS_H
#define _FOPS_H

#include <linux/fs.h>

int pfi_open(struct inode *inode, struct file *filp);

#endif
