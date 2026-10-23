// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <asm/barrier.h>
#include <linux/jiffies.h>
#include <linux/preempt.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/workqueue.h>

#include "main.h"

static unsigned int delay;
module_param(delay, uint, 0444);
MODULE_PARM_DESC(delay, "Delay between runs of jiqwqdelay, in jiffies");

static struct jiq_dev *jiq_dev;

/* Mark the buffer complete and wake the reader. */
static void jiq_done(struct jiq_dev *dev)
{
	/* release: publish dev->buf before done reads 1 */
	smp_store_release(&dev->done, 1);
	wake_up_interruptible(&dev->jiq_wait);
}

/*
 * Append one line describing the current context. Returns false, after
 * waking the reader, once the buffer has no room for another line.
 */
static bool jiq_print(struct jiq_dev *dev)
{
	unsigned long j = jiffies;

	pr_debug("%s() is invoked\n", __func__);

	if (dev->len == 0)
		dev->len = scnprintf(dev->buf, BUF_LEN,
				     "    time  delta preempt   pid cpu command\n");

	/* raw_: the CPU number is only printed, see 13_jit */
	dev->len += scnprintf(dev->buf + dev->len, BUF_LEN - dev->len,
			      "%9lu  %4lu     %3i %5i %3i %s\n",
			      j, j - dev->jiffies,
			      preempt_count(), current->pid,
			      raw_smp_processor_id(), current->comm);
	dev->jiffies = j;

	if (BUF_LEN - dev->len < LINE_MAX_LEN) {
		jiq_done(dev);
		return false;
	}

	return true;
}

/* Reset the buffer before a new read. Called with dev->lock held. */
static void jiq_reset(struct jiq_dev *dev)
{
	dev->len = 0;
	dev->jiffies = jiffies;
	WRITE_ONCE(dev->done, 0);
}

/* Wait until the buffer is complete, then print it. */
static int jiq_wait_and_show(struct seq_file *m, struct jiq_dev *dev)
{
	/* acquire pairs with smp_store_release() in jiq_done() */
	if (wait_event_interruptible(dev->jiq_wait, smp_load_acquire(&dev->done)))
		return -ERESTARTSYS;

	seq_puts(m, dev->buf);
	return 0;
}

/*
 * Call jiq_print() from the shared workqueue. schedule_work() queues on
 * system_percpu_wq, formerly called system_wq.
 */
static void jiq_print_wq(struct work_struct *work)
{
	struct jiq_dev *dev = container_of(work, struct jiq_dev, jiq_work);

	if (jiq_print(dev))
		schedule_work(&dev->jiq_work);
}

static int jiq_read_wq(struct seq_file *m, void *v)
{
	struct jiq_dev *dev = jiq_dev;
	DEFINE_WAIT(wait);
	int retval = 0;

	pr_debug("%s() is invoked\n", __func__);

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	jiq_reset(dev);
	schedule_work(&dev->jiq_work);

	/*
	 * Sleep by hand, as in 09_pipe_advanced_sleep, instead of using
	 * wait_event_interruptible() like the other files.
	 *
	 * prepare_to_wait() comes before the test, so a wakeup between the
	 * test and schedule() is not lost: it sets us back to TASK_RUNNING.
	 *
	 * schedule() returning does not mean the work is done. A signal, a
	 * wake_up() meant for another file sharing jiq_wait, or a spurious
	 * wakeup also end it, hence the loop that tests done and
	 * signal_pending() every time.
	 */
	for (;;) {
		prepare_to_wait(&dev->jiq_wait, &wait, TASK_INTERRUPTIBLE);
		/* acquire pairs with smp_store_release() in jiq_done() */
		if (smp_load_acquire(&dev->done))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}
	finish_wait(&dev->jiq_wait, &wait);

	/* after a signal the work is still re-queueing itself; stop it */
	cancel_work_sync(&dev->jiq_work);

	if (!retval)
		seq_puts(m, dev->buf);

	mutex_unlock(&dev->lock);
	return retval;
}

/* Call jiq_print() from a delayed work item, dev->delay jiffies apart. */
static void jiq_print_wq_delayed(struct work_struct *work)
{
	struct jiq_dev *dev = container_of(work, struct jiq_dev,
					   jiq_work_delayed.work);

	if (jiq_print(dev))
		schedule_delayed_work(&dev->jiq_work_delayed, dev->delay);
}

static int jiq_read_wq_delayed(struct seq_file *m, void *v)
{
	struct jiq_dev *dev = jiq_dev;
	int retval;

	pr_debug("%s() is invoked\n", __func__);

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	jiq_reset(dev);
	dev->delay = delay;
	schedule_delayed_work(&dev->jiq_work_delayed, dev->delay);

	retval = jiq_wait_and_show(m, dev);
	cancel_delayed_work_sync(&dev->jiq_work_delayed);

	mutex_unlock(&dev->lock);
	return retval;
}

/* Call jiq_print() once from a timer, one second later. */
static void jiq_timedout(struct timer_list *t)
{
	struct jiq_dev *dev = timer_container_of(dev, t, timer);

	jiq_print(dev);
	jiq_done(dev);
}

static int jiq_read_run_timer(struct seq_file *m, void *v)
{
	struct jiq_dev *dev = jiq_dev;
	int retval;

	pr_debug("%s() is invoked\n", __func__);

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	jiq_reset(dev);
	jiq_print(dev);		/* the first line, from process context */
	mod_timer(&dev->timer, jiffies + HZ);

	retval = jiq_wait_and_show(m, dev);
	timer_delete_sync(&dev->timer);

	mutex_unlock(&dev->lock);
	return retval;
}

/*
 * Call jiq_print() from a BH work item, i.e. in softirq context. This file
 * used a tasklet (tasklet_schedule()) as in LDD3; tasklets are deprecated
 * and BH workqueues replace them since Linux 6.9.
 */
static void jiq_print_bh(struct work_struct *work)
{
	struct jiq_dev *dev = container_of(work, struct jiq_dev, bh_work);

	if (jiq_print(dev))
		queue_work(system_bh_wq, &dev->bh_work);
}

static int jiq_read_bh(struct seq_file *m, void *v)
{
	struct jiq_dev *dev = jiq_dev;
	int retval;

	pr_debug("%s() is invoked\n", __func__);

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	jiq_reset(dev);
	queue_work(system_bh_wq, &dev->bh_work);

	retval = jiq_wait_and_show(m, dev);
	cancel_work_sync(&dev->bh_work);

	mutex_unlock(&dev->lock);
	return retval;
}

static const struct {
	const char *name;
	int (*show)(struct seq_file *m, void *v);
} jiq_files[] = {
	{ "jiqwq",      jiq_read_wq },
	{ "jiqwqdelay", jiq_read_wq_delayed },
	{ "jiqtimer",   jiq_read_run_timer },
	/* named after the tasklet it used to be */
	{ "jiqtasklet", jiq_read_bh },
};

static void remove_files(int count)
{
	while (count--)
		remove_proc_entry(jiq_files[count].name, NULL);
}

static int __init m_init(void)
{
	int i;

	pr_info("loaded\n");

	jiq_dev = kzalloc_obj(*jiq_dev);
	if (!jiq_dev)
		return -ENOMEM;

	mutex_init(&jiq_dev->lock);
	init_waitqueue_head(&jiq_dev->jiq_wait);
	INIT_WORK(&jiq_dev->jiq_work, jiq_print_wq);
	INIT_DELAYED_WORK(&jiq_dev->jiq_work_delayed, jiq_print_wq_delayed);
	timer_setup(&jiq_dev->timer, jiq_timedout, 0);
	INIT_WORK(&jiq_dev->bh_work, jiq_print_bh);

	for (i = 0; i < ARRAY_SIZE(jiq_files); i++) {
		if (!proc_create_single(jiq_files[i].name, 0444, NULL,
					jiq_files[i].show)) {
			remove_files(i);
			kfree(jiq_dev);
			return -ENOMEM;
		}
	}

	return 0;
}

static void __exit m_exit(void)
{
	pr_info("unloaded\n");

	/* no reader is left once the files are gone */
	remove_files(ARRAY_SIZE(jiq_files));

	/* every read stops its work before returning; be sure anyway */
	cancel_work_sync(&jiq_dev->jiq_work);
	cancel_delayed_work_sync(&jiq_dev->jiq_work_delayed);
	timer_shutdown_sync(&jiq_dev->timer);
	cancel_work_sync(&jiq_dev->bh_work);

	kfree(jiq_dev);
}

module_init(m_init);
module_exit(m_exit);

MODULE_AUTHOR("d0u9");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Deferring work with workqueues, timers and BH work");
