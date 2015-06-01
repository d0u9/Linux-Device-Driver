#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#define MODULE_NAME "Module Parameters"

static char *name = MODULE_NAME;
static int howmany = 1;
module_param(howmany, int, S_IRUGO);
module_param(name, charp, S_IRUGO);

static int __init module_start(void)
{
	int i = 0;
	printk(KERN_WARNING MODULE_NAME "loaded\n");
	for (; i < howmany; ++i) {
		printk(KERN_WARNING "name = %s, time = %d\n", name, i);
	}
	return 0;
}

static void __exit module_end(void)
{
	printk(KERN_WARNING MODULE_NAME " unloaded\n");
}

module_init(module_start);
module_exit(module_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Douglas Su");
MODULE_DESCRIPTION("Module parameters test program");

