// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>

#include <asm/barrier.h>
#include <linux/jiffies.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/timekeeping.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "main.h"
#include "fops.h"

static unsigned int delay = HZ;	/* the default delay, expressed in jiffies */
module_param(delay, uint, 0444);
MODULE_PARM_DESC(delay, "Delay of the jit* files, in jiffies (default HZ)");

static unsigned int tdelay = 10;	/* timer interval, in jiffies */

int jit_currentime(struct seq_file *m, void *p)
{
	struct timespec64 tv1, tv2;
	unsigned long j1;
	u64 j2;

	pr_debug("%s() is invoked\n", __func__);

	/* get them four */
	j1 = jiffies;
	j2 = get_jiffies_64();

	/*
	 * LDD3 used do_gettimeofday(&struct timeval) and
	 * current_kernel_time() returning struct timespec. Both structures
	 * keep the seconds in a long, which is 32 bits on 32-bit machines and
	 * overflows in January 2038, so both functions were removed from the
	 * kernel. The replacements use struct timespec64, whose tv_sec is a
	 * 64-bit time64_t on every architecture:
	 *
	 *	do_gettimeofday()	-> ktime_get_real_ts64()
	 *	current_kernel_time()	-> ktime_get_coarse_real_ts64()
	 */
	ktime_get_real_ts64(&tv1);
	ktime_get_coarse_real_ts64(&tv2);

	/* tv_sec is 64-bit (time64_t); never truncate it to int (y2038) */
	seq_printf(m, "0x%08lx 0x%016llx %10lld.%06ld\n%41lld.%09ld\n",
		   j1, j2,
		   (long long)tv1.tv_sec, tv1.tv_nsec / NSEC_PER_USEC,
		   (long long)tv2.tv_sec, tv2.tv_nsec);
	return 0;
}

int jit_fn(struct seq_file *m, void *p)
{
	unsigned long j0, j1; /* jiffies */
	wait_queue_head_t wait;

	pr_debug("%s() is invoked\n", __func__);

	init_waitqueue_head(&wait);

	j0 = jiffies;
	j1 = j0 + delay;

	switch ((long)m->private) {
	case JIT_BUSY:
		/* burns the CPU; only acceptable for very short delays */
		while (time_before(jiffies, j1))
			cpu_relax();
		break;
	case JIT_SCHED:
		/* stays runnable, just lets others run first */
		while (time_before(jiffies, j1))
			schedule();
		break;
	case JIT_QUEUE:
		/* nobody wakes this queue, so it always times out */
		wait_event_interruptible_timeout(wait, 0, delay);
		break;
	case JIT_SCHEDTO:
		/* the two steps of schedule_timeout_interruptible(delay) */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(delay);
		break;
	default:
		pr_debug("unknown option\n");
	}

	seq_printf(m, "%9lu %9lu\n", j0, j1);

	return 0;
}

#define JIT_ASYNC_LOOPS	5
#define JIT_BUF_SIZE	PAGE_SIZE

/* This data structure is used as "data" for the timer and BH work functions */
struct jit_data {
	struct timer_list timer;
	struct work_struct work;
	struct workqueue_struct *wq;	/* system_bh_wq or system_bh_highpri_wq */
	wait_queue_head_t wait;
	unsigned long prevjiffies;
	char *buf;
	size_t len;
	int loops;
};

/* Append one line describing the current context to data->buf. */
static void jit_record(struct jit_data *data, unsigned long j)
{
	/*
	 * raw_smp_processor_id(): in process context we may be preempted and
	 * migrated right after reading the CPU number, which is fine for a
	 * value that is only printed.
	 */
	data->len += scnprintf(data->buf + data->len,
			       JIT_BUF_SIZE - data->len,
			       "%9lu  %3lu  %7i  %6i  %3i  %s\n",
			       j, j - data->prevjiffies,
			       in_serving_softirq() ? 1 : 0, current->pid,
			       raw_smp_processor_id(), current->comm);
	data->prevjiffies = j;
}

/* Allocate everything per read to allow concurrency. */
static struct jit_data *jit_data_alloc(void)
{
	struct jit_data *data;

	data = kzalloc_obj(*data);
	if (!data)
		return NULL;

	data->buf = kzalloc(JIT_BUF_SIZE, GFP_KERNEL);
	if (!data->buf) {
		kfree(data);
		return NULL;
	}

	init_waitqueue_head(&data->wait);
	data->loops = JIT_ASYNC_LOOPS;
	data->prevjiffies = jiffies;
	data->len = scnprintf(data->buf, JIT_BUF_SIZE,
			      "     time  delta  softirq     pid  cpu  command\n");
	jit_record(data, data->prevjiffies);

	return data;
}

static void jit_data_free(struct jit_data *data)
{
	kfree(data->buf);
	kfree(data);
}

/* Wait until the callbacks have run JIT_ASYNC_LOOPS times, then print. */
static int jit_wait_and_show(struct seq_file *m, struct jit_data *data)
{
	/*
	 * Acquire pairs with smp_store_release() in the callbacks: once loops
	 * reads 0, all lines they appended to data->buf are visible here too.
	 */
	if (wait_event_interruptible(data->wait, !smp_load_acquire(&data->loops)))
		return -ERESTARTSYS;

	seq_puts(m, data->buf);
	return 0;
}

static void jit_timer_fn(struct timer_list *t)
{
	struct jit_data *data = timer_container_of(data, t, timer);

	pr_debug("%s() is invoked\n", __func__);

	jit_record(data, jiffies);

	if (data->loops > 1) {
		data->loops--;
		mod_timer(&data->timer, data->timer.expires + tdelay);
	} else {
		/* release: publish data->buf before loops reads 0 */
		smp_store_release(&data->loops, 0);
		wake_up_interruptible(&data->wait);
	}
}

int jit_timer(struct seq_file *m, void *p)
{
	struct jit_data *data;
	int retval;

	pr_debug("%s() is invoked\n", __func__);

	data = jit_data_alloc();
	if (!data)
		return -ENOMEM;

	timer_setup(&data->timer, jit_timer_fn, 0);
	mod_timer(&data->timer, data->prevjiffies + tdelay);

	retval = jit_wait_and_show(m, data);

	/*
	 * Make sure the timer is neither pending nor running before freeing
	 * data: after a signal it is still armed, and even after the last
	 * round jit_timer_fn() may still be returning from wake_up().
	 */
	timer_shutdown_sync(&data->timer);
	jit_data_free(data);

	return retval;
}

/*
 * BH work items run in softirq context, like the tasklets this example used
 * to show. system_bh_highpri_wq takes the place of tasklet_hi_schedule().
 */
static void jit_bh_fn(struct work_struct *work)
{
	struct jit_data *data = container_of(work, struct jit_data, work);

	pr_debug("%s() is invoked\n", __func__);

	jit_record(data, jiffies);

	if (data->loops > 1) {
		data->loops--;
		queue_work(data->wq, &data->work);
	} else {
		/* release: publish data->buf before loops reads 0 */
		smp_store_release(&data->loops, 0);
		wake_up_interruptible(&data->wait);
	}
}

int jit_bh(struct seq_file *m, void *p)
{
	struct jit_data *data;
	int retval;

	pr_debug("%s() is invoked\n", __func__);

	data = jit_data_alloc();
	if (!data)
		return -ENOMEM;

	INIT_WORK(&data->work, jit_bh_fn);
	data->wq = (long)m->private == JIT_BH_HIGHPRI ?
		   system_bh_highpri_wq : system_bh_wq;
	queue_work(data->wq, &data->work);

	retval = jit_wait_and_show(m, data);

	/* as in jit_timer(): stop the work, including self re-queueing */
	disable_work_sync(&data->work);
	jit_data_free(data);

	return retval;
}
