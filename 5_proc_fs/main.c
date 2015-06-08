#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "global.h"

struct proc_dir_entry *parent = NULL;
static int proc_open(struct inode *inode, struct file *filp);
static int proc_release(struct inode *inode, struct file *filp);

struct file_operations proc_ops = {
	.owner = THIS_MODULE,
	.open = proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = proc_release,
};

static int proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Hello World\n");
	return 0;
}

static int proc_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, proc_show, NULL);
	return 0;
}

static int proc_release(struct inode *inode, struct file *filp)
{
	return single_release(inode, filp);
}

static int __init start_module(void)
{
	PDEBUG("\n\tModule 'proc-fs' is loaded\n");

	parent = proc_mkdir(SUB_DIR_NAME, NULL);
	if (!proc_create(PROC_FS_NAME, 0, parent, &proc_ops))
		return -ENOMEM;

	return 0;
}

static void __exit end_module(void)
{
	PDEBUG("Module 'proc-fs' is unloaded\n");

	remove_proc_entry(PROC_FS_NAME, parent);
	remove_proc_entry(SUB_DIR_NAME, NULL);
}

module_init(start_module);
module_exit(end_module);

MODULE_AUTHOR("Douglas");
MODULE_LICENSE("GPL");
