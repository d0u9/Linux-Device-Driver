/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME	"jit"

/* use these as data pointers, to implement four files in one function */
enum jit_files {
	JIT_BUSY,
	JIT_SCHED,
	JIT_QUEUE,
	JIT_SCHEDTO,
};

/* data pointers of jit_bh(): which BH workqueue to use */
enum jit_bh_prio {
	JIT_BH_NORMAL,		/* was a tasklet */
	JIT_BH_HIGHPRI,		/* was a high priority tasklet */
};

#endif
