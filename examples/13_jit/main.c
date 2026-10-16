#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/slab.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>

#include <asm/hardirq.h>

#include "main.h"
#include "fops.h"
int delay = HZ; /* the default delay, expressed in jiffies */
int tdelay = 10;

module_param(delay, int, 0);

static struct opt *opts[PROC_FILE_NR] = { NULL };

static
int proc_open(struct inode *inode, struct file *filp)
{
	struct opt *opt = PDE_DATA(inode);
	return single_open(filp, opt->show, opt->args);
}

static
int proc_release(struct inode *inode, struct file *filp)
{
	return single_release(inode, filp);
}

static struct proc_ops proc_ops = {
	.proc_open    = proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = proc_release,
};


static inline
struct opt *new_opt(int (*show)(struct seq_file *m, void *p), void *args)
{
	struct opt *opt = kmalloc(sizeof(struct opt), GFP_KERNEL);
	opt->show = show;
	opt->args = args;

	return opt;
}

static
int __init m_init(void)
{
	struct opt *opt = NULL;
	int i = 0;

	printk(KERN_WARNING MODULE_NAME " is loaded\n");

	opt = opts[i++] = new_opt(jit_currentime, NULL);
	proc_create_data("currentime", 0, NULL, &proc_ops, opt);

	opt = opts[i++] = new_opt(jit_fn, (void*)JIT_BUSY);
	proc_create_data("jitbusy",   0, NULL, &proc_ops, opt);
	opt = opts[i++] = new_opt(jit_fn, (void*)JIT_SCHED);
	proc_create_data("jitsched",  0, NULL, &proc_ops, opt);
	opt = opts[i++] = new_opt(jit_fn, (void*)JIT_QUEUE);
	proc_create_data("jitqueue",  0, NULL, &proc_ops, opt);
	opt = opts[i++] = new_opt(jit_fn, (void*)JIT_SCHEDTO);
	proc_create_data("jitschedto",0, NULL, &proc_ops, opt);

	opt = opts[i++] = new_opt(jit_timer, NULL);
	proc_create_data("jitimer", 0, NULL, &proc_ops, opt);

	opt = opts[i++] = new_opt(jit_tasklet, NULL);
	proc_create_data("jitasklet", 0, NULL, &proc_ops, opt);

	opt = opts[i++] = new_opt(jit_tasklet, (void*)1);
	proc_create_data("jitasklethi", 0, NULL, &proc_ops, opt);

	return 0;
}

static
void __exit m_exit(void)
{
	printk(KERN_WARNING MODULE_NAME " unloaded\n");

	remove_proc_entry("currentime", NULL);
	remove_proc_entry("jitbusy", NULL);
	remove_proc_entry("jitsched", NULL);
	remove_proc_entry("jitqueue", NULL);
	remove_proc_entry("jitschedto", NULL);
	remove_proc_entry("jitimer", NULL);
	remove_proc_entry("jitasklet", NULL);
	remove_proc_entry("jitasklethi", NULL);

	for (int i = 0; i < PROC_FILE_NR; ++i)
		kfree(opts[i]);

#if 0
	remove_proc_entry("jitasklethi", NULL);
#endif
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Delay methods in Linux kernel.");

