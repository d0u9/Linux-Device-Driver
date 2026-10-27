/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/tty_port.h>

#define MODULE_NAME		"lddtty"
#define LDD_TTY_MINOR_NR	4

#define LDD_LINE_LONG		80	/* print at most 80 chars per line */
#define LDD_TTY_MAX_ROOM	PAGE_SIZE

#define TIMER_INTERVAL		HZ	/* one input character a second */

struct ldd_tty_dev {
	struct tty_port		port;
	spinlock_t		lock;	/* protects mcr, msr and buffer */

	int			msr;
	int			mcr;

	int			buffer_curr;
	char			buffer[LDD_TTY_MAX_ROOM];

	struct timer_list	timer;
};

#endif
