#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

#include "main.h"
#include "fops.h"

static void *proc_seq_start(struct seq_file *, loff_t *);
static void *proc_seq_next(struct seq_file *, void *, loff_t *);
static void proc_seq_stop(struct seq_file *, void *);
static int proc_seq_show(struct seq_file *, void *);

static struct seq_operations proc_seq_ops = {
	.start = proc_seq_start,
	.next  = proc_seq_next,
	.stop  = proc_seq_stop,
	.show  = proc_seq_show,
};

static char *data[DATA_BLOCK_NUM] = {
	"Day 1: God creates the heavens and the earth.",
	"Day 2: God creates the sky.",
	"Day 3: God creates dry land and all plant life both large and small.",
	"Day 4: God creates all the stars and heavenly bodies.",
	"Day 5: God creates all life that lives in the water.",
	"Day 6: God creates all the creatures that live on dry land.",
	"Day 7: God rests."
};

int proc_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);
	return seq_open(filp, &proc_seq_ops);
}

static void *proc_seq_start(struct seq_file *s_file, loff_t *pos)
{
	pr_debug("%s() is invoked, pos=%lld\n", __FUNCTION__, *pos);

	if (*pos >= ARRAY_SIZE(data)) {
		pr_debug("position requested exceeds the maximum length\n");
		return NULL;
	}

	return *(data + *pos);
}

static void *proc_seq_next(struct seq_file *s_file, void *v, loff_t *pos)
{
	pr_debug("%s() is invoked, pos=%lld\n", __FUNCTION__, *pos);

	(*pos)++;
	if (*pos >= ARRAY_SIZE(data)) {
		pr_debug("position requested exceeds the maximum length\n");
		return NULL;
	}

	return *(data + *pos);
}

static void proc_seq_stop(struct seq_file *s_file, void *v)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);
}

static int proc_seq_show(struct seq_file *s_file, void *v)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);

	seq_printf(s_file, "%p: %s\n", v, (char *)v);
	return 0;
}

