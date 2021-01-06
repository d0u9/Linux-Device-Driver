#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static
int __init hello_init(void)
{
	printk(KERN_ALERT "Hello, world!\n");
	pr_debug("This is a debug message!\n");
	pr_err("This is a error message!\n");
	return 0;
}

static
void __exit hello_exit(void)
{
	printk(KERN_ALERT "Bye, World!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Hello World program");
