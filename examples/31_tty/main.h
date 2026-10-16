#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME			"lddtty"

#define LDD_TTY_MAJOR			240
#define LDD_TTY_MINOR_NR		4

#define LDD_LINE_LONG			80	// PRINT 80 chars per line
#define LDD_TTY_MAX_ROOM		PAGE_SIZE

#define TIMER_INTERVAL			(HZ >> 8)

struct ldd_tty_dev {
	int		index;
	int		open_count;
	spinlock_t		spinlock;

	struct tty_struct	*tty;
	struct tty_port		port;

	int		msr;
	int		mcr;

	int buffer_curr;
	char buffer[LDD_TTY_MAX_ROOM];

	struct timer_list	timer;
};

#endif
