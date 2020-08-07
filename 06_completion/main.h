#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME	"completion"

struct completion_dev {
	struct cdev cdev;
	struct completion completion;
};

#endif
