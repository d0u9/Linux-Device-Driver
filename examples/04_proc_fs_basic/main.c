// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "main.h"

static struct proc_dir_entry *parent;

static int proc_show(struct seq_file *m, void *v)
{
	long c = m->private ? (long)m->private : 1;

	for (long i = 0; i < c; ++i)
		seq_puts(m, "Hello World!\n");

	return 0;
}

static int proc_open(struct inode *inode, struct file *filp)
{
	/*
	 * The data passed to proc_create_data() can be retrieved from the
	 * inode by pde_data(). Here we hand it to single_open(), which stores
	 * it in the private field of struct seq_file.
	 */
	return single_open(filp, proc_show, pde_data(inode));
}

static int proc_release(struct inode *inode, struct file *filp)
{
	return single_release(inode, filp);
}

static const struct proc_ops proc_ops = {
	.proc_open    = proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = proc_release,
};

/*
 * Since 4.18, a file that only needs a show() callback can skip proc_ops,
 * open() and release() altogether:
 *
 *	proc_create_single_data(PROC_FS_NAME_MUL, 0444, parent, proc_show,
 *				(void *)PRINT_NR);
 *
 * proc_show() then finds the data in m->private, just as it does here.
 */

static int __init m_init(void)
{
	pr_info("loaded\n");

	parent = proc_mkdir(SUB_DIR_NAME, NULL);
	if (!parent)
		return -ENOMEM;

	if (!proc_create(PROC_FS_NAME, 0444, parent, &proc_ops))
		goto err_remove_dir;

	/* pass a variable to open() */
	if (!proc_create_data(PROC_FS_NAME_MUL, 0444, parent, &proc_ops,
			      (void *)PRINT_NR))
		goto err_remove_file;

	return 0;

err_remove_file:
	remove_proc_entry(PROC_FS_NAME, parent);
err_remove_dir:
	remove_proc_entry(SUB_DIR_NAME, NULL);
	return -ENOMEM;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	/*
	 * Each entry is removed one by one here, children before the
	 * directory. proc_remove(parent) or remove_proc_subtree(SUB_DIR_NAME,
	 * NULL) removes the whole directory in one call; the next example
	 * uses that.
	 */
	remove_proc_entry(PROC_FS_NAME_MUL, parent);
	remove_proc_entry(PROC_FS_NAME, parent);
	remove_proc_entry(SUB_DIR_NAME, NULL);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A very basic example of /proc fs manipulation");
