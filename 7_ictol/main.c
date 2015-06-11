#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "global.h"

static int __init ioctl_init(void)
{
	PDEBUG("\n\tModule is loaded!\n");
	return 0;
}

static void __exit ioctl_exit(void)
{
	PDEBUG("Module is unloaded\n");
}

module_init(ioctl_init);
module_exit(ioctl_exit);

MODULE_AUTHOR("Douglas");
MODULE_LICENSE("GPL");
