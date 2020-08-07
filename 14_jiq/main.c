#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>


#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/errno.h>  /* error codes */
#include <linux/workqueue.h>
#include <linux/preempt.h>
#include <linux/interrupt.h> /* tasklets */

#include "main.h"

static long delay = 0;
module_param(delay, long, 0);

static struct jiq_dev *jiq_dev = NULL;

static int jiq_print(struct jiq_dev *dev)
{
	int  len = dev->len;
	char *buf = dev->buf;
	unsigned long j = jiffies;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (len == 0)
		len = sprintf(buf,"    time  delta preempt   pid cpu command\n");

	len += sprintf(buf + len, "%9li  %4li     %3i %5i %3i %s\n",
		       j, j - dev->jiffies,
		       preempt_count(), current->pid, smp_processor_id(),
		       current->comm);

	if (len > BUF_LEN) {
		pr_debug("len = %d\n", len);
		wake_up_interruptible(&dev->jiq_wait);
		return 0;
	}

	dev->len = len;
	dev->jiffies = j;

	return 1;
}

static
void jiq_print_wq_delayd(struct work_struct *work)
{
	struct jiq_dev *dev = container_of(work, struct jiq_dev, jiq_work_delay.work);

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (!jiq_print(dev))
		return;

	schedule_delayed_work(&dev->jiq_work_delay, dev->delay);
}

static
void jiq_print_wq(struct work_struct *work)
{
	struct jiq_dev *dev = container_of(work, struct jiq_dev, jiq_work);

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (!jiq_print(dev))
		return;

	schedule_work(&dev->jiq_work);
}

static
int jiq_read_wq_delayed(struct seq_file *m, void *v)
{
	DEFINE_WAIT(wait);

	pr_debug("%s() is invoked\n", __FUNCTION__);
	
	jiq_dev->len = 0;
	memset(jiq_dev->buf, 0, BUF_LEN);
	jiq_dev->jiffies = jiffies;
	jiq_dev->delay = delay;

	prepare_to_wait(&jiq_dev->jiq_wait, &wait, TASK_INTERRUPTIBLE);
	schedule_delayed_work(&jiq_dev->jiq_work_delay, jiq_dev->delay);
	schedule();
	finish_wait(&jiq_dev->jiq_wait, &wait);

	seq_printf(m, "%s", jiq_dev->buf);

	return 0;
}

static
int jiq_read_wq(struct seq_file *m, void *v)
{
	DEFINE_WAIT(wait);

	pr_debug("%s() is invoked\n", __FUNCTION__);

	jiq_dev->len = 0;
	memset(jiq_dev->buf, 0, BUF_LEN);
	jiq_dev->jiffies = jiffies;
	jiq_dev->delay = 0;

	prepare_to_wait(&jiq_dev->jiq_wait, &wait, TASK_INTERRUPTIBLE);
	schedule_work(&jiq_dev->jiq_work);
	schedule();
	finish_wait(&jiq_dev->jiq_wait, &wait);

	seq_printf(m, "%s", jiq_dev->buf);

	return 0;
}

/*
 * Call jiq_print from a timer
 */
static
void jiq_timedout(struct timer_list *t)
{
	struct jiq_dev *dev = from_timer(dev, t, timer);

	pr_debug("%s() is invoked\n", __FUNCTION__);

	jiq_print(dev);            /* print a line */

	dev->timeout = 1;
	wake_up_interruptible(&dev->jiq_wait);  /* awake the process */
}

static
int jiq_read_run_timer(struct seq_file *m, void *v)
{
	pr_debug("%s() is invoked\n", __FUNCTION__);

	jiq_dev->len = 0;
	memset(jiq_dev->buf, 0, BUF_LEN);
	jiq_dev->jiffies  =jiffies;

	timer_setup(&jiq_dev->timer, jiq_timedout, 0);
	jiq_dev->timer.expires = jiffies + HZ;

	jiq_print(jiq_dev);

	add_timer(&jiq_dev->timer);
	wait_event_interruptible(jiq_dev->jiq_wait, jiq_dev->timeout);
	jiq_dev->timeout = 0;
	del_timer_sync(&jiq_dev->timer);

	seq_printf(m, "%s", jiq_dev->buf);

	return 0;
}

/*
 * Call jiq_print from a tasklet
 */
static
void jiq_print_tasklet(unsigned long ptr)
{
	struct jiq_dev *dev = (struct jiq_dev*) ptr;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (jiq_print(dev)) {
		tasklet_schedule(&dev->tlet);
		return;
	}

	dev->timeout = 1;
	wake_up_interruptible(&dev->jiq_wait);  /* awake the process */
}

static
int jiq_read_tasklet(struct seq_file *m, void *v)
{
	jiq_dev->len = 0;
	memset(jiq_dev->buf, 0, BUF_LEN);
	jiq_dev->jiffies  =jiffies;

	tasklet_init(&jiq_dev->tlet, jiq_print_tasklet, (unsigned long)jiq_dev);

	tasklet_schedule(&jiq_dev->tlet);
	wait_event_interruptible(jiq_dev->jiq_wait, jiq_dev->timeout);
	jiq_dev->timeout = 0;

	seq_printf(m, "%s", jiq_dev->buf);

	return 0;
}

static
int proc_open(struct inode *inode, struct file *filp)
{
	int (*show)(struct seq_file *m, void *v) = PDE_DATA(inode);
	return single_open(filp, show, NULL);
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

static
int m_init(void)
{
	jiq_dev = kmalloc(sizeof(struct jiq_dev), GFP_KERNEL);

	printk(KERN_WARNING MODULE_NAME " is loaded\n");

	INIT_WORK(&jiq_dev->jiq_work, jiq_print_wq);
	INIT_DELAYED_WORK(&jiq_dev->jiq_work_delay, jiq_print_wq_delayd);

	init_waitqueue_head(&jiq_dev->jiq_wait);

	proc_create_data("jiqwq", 0, NULL, &proc_ops, jiq_read_wq);
	proc_create_data("jiqwqdelay", 0, NULL, &proc_ops, jiq_read_wq_delayed);
	proc_create_data("jiqtimer", 0, NULL, &proc_ops, jiq_read_run_timer);
	proc_create_data("jiqtasklet", 0, NULL, &proc_ops, jiq_read_tasklet);

	return 0;
}

static
void m_exit(void)
{
	printk(KERN_WARNING MODULE_NAME " unloaded\n");

	remove_proc_entry("jiqwq", NULL);
	remove_proc_entry("jiqwqdelay", NULL);
	remove_proc_entry("jiqtimer", NULL);
	remove_proc_entry("jiqtasklet", NULL);

	kfree(jiq_dev);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Use tasklet in kernel");
