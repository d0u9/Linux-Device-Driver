#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"short_irq"
#define SHORT_NR_PORTS		1

struct short_dev {
	spinlock_t lock;
	int cnt;
};

#endif
