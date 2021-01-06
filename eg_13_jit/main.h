#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME	"jit"
#define PROC_FILE_NR		8

/* use these as data pointers, to implement four files in one function */
enum jit_files {
	JIT_BUSY,
	JIT_SCHED,
	JIT_QUEUE,
	JIT_SCHEDTO
};

struct opt {
	int (*show)(struct seq_file *m, void *p);
	void *args;
};

extern int delay;
extern int tdelay;

#endif

