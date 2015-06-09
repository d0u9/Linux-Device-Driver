#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/atomic.h>

#include "global.h"
#include "fops.h"


struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.write = proc_write,
	.open = proc_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

struct store_list_head_t store_list_head;

static int __init proc_fs_iterator_init(void)
{
	PDEBUG("\n\tModule is loaded!\n");
	
	atomic_set(&store_list_head.n, 0);
	INIT_LIST_HEAD(&store_list_head.list);

	if (!proc_create(PROC_FILE_NAME, 0666, NULL, &proc_fops))
		return -ENOMEM;

	return 0;
}

static void __exit proc_fs_iterator_exit(void)
{
	PDEBUG("Module is unloaded\n");
	remove_proc_entry(PROC_FILE_NAME, NULL);
}

module_init(proc_fs_iterator_init);
module_exit(proc_fs_iterator_exit);

MODULE_AUTHOR("Douglas");
MODULE_LICENSE("GPL");
