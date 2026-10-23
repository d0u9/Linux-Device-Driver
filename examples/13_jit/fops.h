/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FOPS_H
#define _FOPS_H

#include <linux/seq_file.h>

int jit_currentime(struct seq_file *m, void *p);
int jit_fn(struct seq_file *m, void *p);
int jit_timer(struct seq_file *m, void *p);
int jit_bh(struct seq_file *m, void *p);

#endif
