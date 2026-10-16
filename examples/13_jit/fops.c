#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/time.h>
#include <linux/slab.h>
#include <linux/sched.h>
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

int jit_currentime(struct seq_file *m, void *p)
{
	struct timespec64 tv1, tv2;
	unsigned long j1;
	u64 j2;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	/* get them four */
	j1 = jiffies;
	j2 = get_jiffies_64();
	ktime_get_real_ts64(&tv1);
	ktime_get_coarse_real_ts64(&tv2);

	/* print */
	seq_printf(m ,"0x%08lx 0x%016Lx %10i.%06i\n"
		   "%41i.%09i\n",
		   j1, j2,
		   (int) tv1.tv_sec, (int) tv1.tv_nsec,
		   (int) tv2.tv_sec, (int) tv2.tv_nsec);
	return 0;
}

int jit_fn(struct seq_file *m, void *p)
{
	unsigned long j0, j1; /* jiffies */
	wait_queue_head_t wait;
	extern int delay;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	init_waitqueue_head(&wait);

	j0 = jiffies;
	j1 = j0 + delay;

	switch((long)(m->private)) {
	case JIT_BUSY:
		while (time_before(jiffies, j1))
			cpu_relax();
		break;
	case JIT_SCHED:
		while (time_before(jiffies, j1))
			schedule();
		break;
	case JIT_QUEUE:
		wait_event_interruptible_timeout(wait, 0, delay);
		break;
	case JIT_SCHEDTO:
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(delay);
		break;
	default:
		pr_debug("Known option\n");
	}

	seq_printf(m, "%9li %9li\n", j0, j1);

	return 0;
}

/* This data structure used as "data" for the timer and tasklet functions */
struct jit_data {
	struct timer_list timer;
	struct tasklet_struct tlet;
	int hi; /* tasklet or tasklet_hi */
	wait_queue_head_t wait;
	unsigned long prevjiffies;
	unsigned char *buf;
	int loops;
};

#define JIT_ASYNC_LOOPS 5

void jit_timer_fn(struct timer_list *t)
{
	struct jit_data *data = from_timer(data, t, timer);
	unsigned long j = jiffies;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	data->buf += sprintf(data->buf, "%9li  %3li     %i    %6i   %i   %s\n",
			     j, j - data->prevjiffies, in_interrupt() ? 1 : 0,
			     current->pid, smp_processor_id(), current->comm);

	if (--data->loops) {
		data->timer.expires += tdelay;
		data->prevjiffies = j;
		add_timer(&data->timer);
	} else {
		wake_up_interruptible(&data->wait);
	}
}

/* the /proc function: allocate everything to allow concurrency */
int jit_timer(struct seq_file *m, void *p)
{
	extern int tdelay;
	struct jit_data *data;
	char *buf = NULL, *buf2 = NULL;
	unsigned long j = jiffies;
	int retval = 0;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (!(data = kmalloc(sizeof(struct jit_data), GFP_KERNEL)))
		return -ENOMEM;

	if (!(buf = kmalloc(PAGE_SIZE, GFP_KERNEL))) {
		retval = -ENOMEM;
		goto alloc_buf_error;
	}
	memset(buf, 0, PAGE_SIZE);
	buf2 = buf;

	timer_setup(&data->timer, jit_timer_fn, 0);
	init_waitqueue_head(&data->wait);

	buf2 += sprintf(buf2, "   time   delta  inirq    pid   cpu command\n");
	buf2 += sprintf(buf2, "%9li  %3li     %i    %6i   %i   %s\n",
			j, 0L, in_interrupt() ? 1 : 0,
			current->pid, smp_processor_id(), current->comm);

	/* fill the data for our timer function */
	data->prevjiffies = j;
	data->buf = buf2;
	data->loops = JIT_ASYNC_LOOPS;
	
	/* register the timer */
	data->timer.expires = j + tdelay; /* parameter */
	add_timer(&data->timer);

	wait_event_interruptible(data->wait, !data->loops);

	if (signal_pending(current)) {
		retval = -ERESTARTSYS;
		goto out;
	}

	seq_printf(m, "%s", buf);


out:
	kfree(buf);

alloc_buf_error:
	kfree(data);

	return retval;
}

void jit_tasklet_fn(unsigned long arg)
{
	struct jit_data *data = (struct jit_data *)arg;
	unsigned long j = jiffies;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	data->buf += sprintf(data->buf, "%9li  %3li     %i    %6i   %i   %s\n",
			     j, j - data->prevjiffies, in_interrupt() ? 1 : 0,
			     current->pid, smp_processor_id(), current->comm);

	if (--data->loops) {
		data->prevjiffies = j;
		if (data->hi)
			tasklet_hi_schedule(&data->tlet);
		else
			tasklet_schedule(&data->tlet);
	} else {
		wake_up_interruptible(&data->wait);
	}
}

/* the /proc function: allocate everything to allow concurrency */
int jit_tasklet(struct seq_file *m, void *p)
{
	struct jit_data *data;
	char *buf, *buf2;
	unsigned long j = jiffies;
	long hi = (long)(m->private);
	int retval = 0;

	pr_debug("%s() is invoked\n", __FUNCTION__);

	if (!(data = kmalloc(sizeof(struct jit_data), GFP_KERNEL)))
		return -ENOMEM;

	if (!(buf = kmalloc(PAGE_SIZE, GFP_KERNEL))) {
		retval = -ENOMEM;
		goto alloc_buf_error;
	}
	memset(buf, 0, PAGE_SIZE);
	buf2 = buf;

	init_waitqueue_head(&data->wait);

	/* write the first lines in the buffer */
	buf2 += sprintf(buf2, "   time   delta  inirq    pid   cpu command\n");
	buf2 += sprintf(buf2, "%9li  %3li     %i    %6i   %i   %s\n",
			j, 0L, in_interrupt() ? 1 : 0,
			current->pid, smp_processor_id(), current->comm);

	/* fill the data for our tasklet function */
	data->prevjiffies = j;
	data->buf = buf2;
	data->loops = JIT_ASYNC_LOOPS;

	/* register the tasklet */
	tasklet_init(&data->tlet, jit_tasklet_fn, (unsigned long)data);
	data->hi = hi;
	if (hi)
		tasklet_hi_schedule(&data->tlet);
	else
		tasklet_schedule(&data->tlet);

	/* wait for the buffer to fill */
	wait_event_interruptible(data->wait, !data->loops);

	if (signal_pending(current)) {
		goto out;
		retval = -ERESTARTSYS;
	}

	seq_printf(m, "%s", buf);

out:
	kfree(buf);

alloc_buf_error:
	kfree(data);

	return retval;
}

