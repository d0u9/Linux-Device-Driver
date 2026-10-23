// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "main.h"
#include "fops.h"

static const struct {
	const char *name;
	int (*show)(struct seq_file *m, void *p);
	long arg;
} jit_files[] = {
	{ "currentime",  jit_currentime, 0 },
	{ "jitbusy",     jit_fn,         JIT_BUSY },
	{ "jitsched",    jit_fn,         JIT_SCHED },
	{ "jitqueue",    jit_fn,         JIT_QUEUE },
	{ "jitschedto",  jit_fn,         JIT_SCHEDTO },
	{ "jitimer",     jit_timer,      0 },
	/* named after the tasklets they replace, see the README */
	{ "jitasklet",   jit_bh,         JIT_BH_NORMAL },
	{ "jitasklethi", jit_bh,         JIT_BH_HIGHPRI },
};

static void remove_files(int count)
{
	while (count--)
		remove_proc_entry(jit_files[count].name, NULL);
}

static int __init m_init(void)
{
	int i;

	pr_info("loaded\n");

	for (i = 0; i < ARRAY_SIZE(jit_files); i++) {
		/*
		 * proc_create_single_data() wires up single_open() itself;
		 * arg reaches the show function as m->private.
		 */
		if (!proc_create_single_data(jit_files[i].name, 0444, NULL,
					     jit_files[i].show,
					     (void *)jit_files[i].arg)) {
			remove_files(i);
			return -ENOMEM;
		}
	}

	return 0;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	remove_files(ARRAY_SIZE(jit_files));
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Delay methods in Linux kernel.");
