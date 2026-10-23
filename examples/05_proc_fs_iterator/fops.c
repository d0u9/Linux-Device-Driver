// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/seq_file.h>

#include "main.h"
#include "fops.h"

static const char * const data[] = {
	"Day 1: God creates the heavens and the earth.",
	"Day 2: God creates the sky.",
	"Day 3: God creates dry land and all plant life both large and small.",
	"Day 4: God creates all the stars and heavenly bodies.",
	"Day 5: God creates all life that lives in the water.",
	"Day 6: God creates all the creatures that live on dry land.",
	"Day 7: God rests.",
};

/* Return the element at *pos, or NULL when the iteration is over. */
static void *data_at(loff_t *pos)
{
	if (*pos >= ARRAY_SIZE(data)) {
		pr_debug("position requested exceeds the maximum length\n");
		return NULL;
	}

	return (void *)data[*pos];
}

static void *pfi_seq_start(struct seq_file *s_file, loff_t *pos)
{
	pr_debug("%s() is invoked, pos=%lld\n", __func__, *pos);
	return data_at(pos);
}

static void *pfi_seq_next(struct seq_file *s_file, void *v, loff_t *pos)
{
	pr_debug("%s() is invoked, pos=%lld\n", __func__, *pos);
	(*pos)++;
	return data_at(pos);
}

static void pfi_seq_stop(struct seq_file *s_file, void *v)
{
	pr_debug("%s() is invoked\n", __func__);
}

static int pfi_seq_show(struct seq_file *s_file, void *v)
{
	pr_debug("%s() is invoked\n", __func__);

	seq_printf(s_file, "%p: %s\n", v, (char *)v);
	return 0;
}

static const struct seq_operations pfi_seq_ops = {
	.start = pfi_seq_start,
	.next  = pfi_seq_next,
	.stop  = pfi_seq_stop,
	.show  = pfi_seq_show,
};

int pfi_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __func__);
	return seq_open(filp, &pfi_seq_ops);
}
