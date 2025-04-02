#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"seeking"
#define BUFF_SIZE		PAGE_SIZE
#define SEEKING_DEV_NR		1
#define HEX_DICT		"0123456789ABCDEF"
#define HEX_DICT_LEN		(ARRAY_SIZE(HEX_DICT) - 1)

struct seeking_dev {
	struct cdev cdev;
	struct mutex mutex;
	char buff[HEX_DICT_LEN];
};

#endif
