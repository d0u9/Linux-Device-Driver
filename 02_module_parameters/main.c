#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

static char *whom = "Mom";
static int howmany = 1;

module_param(howmany, int,   S_IRUGO);
module_param(whom,    charp, S_IRUGO);

static
int __init module_start(void)
{
	printk(KERN_WARNING "parameters test module is loaded\n");

	for (int i = 0; i < howmany; ++i) {
		printk(KERN_WARNING "#%d Hello, %s\n", i, whom);
	}
	return 0;
}

static
void __exit module_end(void)
{
	printk(KERN_WARNING "parameters test module is unloaded\n");
}

module_init(module_start);
module_exit(module_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Module parameters test program");

