#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"short_port_remap"
#define SHORT_NR_PORTS		1

struct short_dev {
	int block_counter;		//record how many blocks now in the list
	struct mutex mutex;
	struct cdev cdev;
	struct list_head block_list;	//list of storage blocks
};

#endif
