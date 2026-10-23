// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

static char *whom = "Mom";
static int howmany = 1;

module_param(howmany, int, 0444);
MODULE_PARM_DESC(howmany, "Number of times to print the greeting");
module_param(whom, charp, 0444);
MODULE_PARM_DESC(whom, "Name to greet");

static int __init m_init(void)
{
	pr_debug("parameters test module is loaded\n");

	for (int i = 0; i < howmany; ++i)
		pr_info("#%d Hello, %s\n", i, whom);

	return 0;
}

static void __exit m_exit(void)
{
	pr_debug("parameters test module is unloaded\n");
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Module parameters test program");
