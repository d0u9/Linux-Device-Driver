#include <linux/module.h>

static int __init m_init(void)
{
	printk(KERN_ALERT "Hello, world!\n");
	return 0;
}

static void __exit m_exit(void)
{
	printk(KERN_ALERT "Bye, world!\n");
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Douglas Su");
MODULE_DESCRIPTION("Hello World program");
