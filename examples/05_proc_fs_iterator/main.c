// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "main.h"
#include "fops.h"

static struct proc_dir_entry *entry;

static const struct proc_ops pfi_proc_ops = {
	.proc_open    = pfi_open,

	/*
	 * seq_read, seq_lseek and seq_release are provided by the kernel to
	 * drive the seq_file iterator.
	 */
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = seq_release,
};

/*
 * Since 4.18, proc_create_seq() does the seq_open()/seq_release() wiring
 * itself, so neither pfi_proc_ops nor pfi_open() is needed:
 *
 *	entry = proc_create_seq(PROC_FILE_NAME, 0444, NULL, &pfi_seq_ops);
 */

static int __init m_init(void)
{
	pr_info("loaded\n");

	entry = proc_create(PROC_FILE_NAME, 0444, NULL, &pfi_proc_ops);
	if (!entry)
		return -ENOMEM;

	return 0;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	/* proc_remove() takes the entry itself and removes any children too */
	proc_remove(entry);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Implement proc file interface via seq_file iterator");
