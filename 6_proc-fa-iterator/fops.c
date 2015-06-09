#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>

#include "global.h"
#include "fops.h"

static void *proc_seq_start(struct seq_file *, loff_t *);
static void *proc_seq_next(struct seq_file *, void *, loff_t *);
static void proc_seq_stop(struct seq_file *, void *);
static int proc_seq_show(struct seq_file *, void *);

static struct seq_operations proc_seq_ops = {
	.start = proc_seq_start,
	.next = proc_seq_next,
	.stop = proc_seq_stop,
	.show = proc_seq_show,
};

/*
 * WARNING: For the purpose of simplifying codes, we have no locks here to
 * pretect list head from concurrent access. So, obviously, it is unsafe.
 */
ssize_t proc_write(struct file *filp, const char __user *buff, size_t count,
	loff_t *f_pos)
{
	struct store_node *cur_node = NULL;
	PDEBUG("Write function is invoked\n");

	cur_node = (struct store_node *) kmalloc(sizeof(*cur_node), GFP_KERNEL);
	if (!cur_node)
		return -ENOMEM;

	if (!(cur_node->buf = kmalloc(count + 1, GFP_KERNEL))) {
		kfree(cur_node);
		return -ENOMEM;
	}
	memset(cur_node->buf, 0, count + 1);

	if (copy_from_user(cur_node->buf, buff, count)) {
		kfree(cur_node->buf);
		kfree(cur_node);
		return -EFAULT;
	}

	INIT_LIST_HEAD(&cur_node->list);
	list_add_tail(&cur_node->list, &store_list_head.list);
	atomic_inc(&store_list_head.n);

	PDEBUG("write %zd bytes, data is: '%s'\n", count, cur_node->buf);
	PDEBUG("Write function has finished\n");
	return count;
}

int proc_seq_open(struct inode *inode, struct file *filp)
{
	return seq_open(filp, &proc_seq_ops);
}

static void *proc_seq_start(struct seq_file *s_file, loff_t *pos)
{
	PDEBUG("seq file start\n");
	if (*pos >= atomic_read(&store_list_head.n))
		return NULL;
	if (list_empty(&store_list_head.list))
		return NULL;
	return list_first_entry(&store_list_head.list, struct store_node, list);
}

static void *proc_seq_next(struct seq_file *s_file, void *v, loff_t *pos)
{
	struct store_node *tmp = NULL;
	PDEBUG("seq file next\n");
	PDEBUG("operate at pos: %lld\n", *pos);
	(*pos)++;
	tmp = list_next_entry((struct store_node *)v, list);
	if (&tmp->list == &store_list_head.list) {
		PDEBUG("seq next will return NULL\n");
		return NULL;
	}

	PDEBUG("seq file now is returning %p\n", tmp);
	return tmp;
}

static void proc_seq_stop(struct seq_file *s_file, void *v)
{
	PDEBUG("seq stop\n");
}

static int proc_seq_show(struct seq_file *s_file, void *v)
{
	PDEBUG("seq show: %p -> %s\n", v, ((struct store_node *)v)->buf);
	seq_printf(s_file, "%p -> %s\n", v, ((struct store_node *)v)->buf);
	return 0;
}
