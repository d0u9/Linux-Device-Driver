#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/atomic.h>

#include "global.h"
#include "fops.h"

static struct echo_dev *echo_devs = NULL;
static int echo_major = ECHO_MAJOR;
static int echo_minor = ECHO_MINOR;
static int echo_buf_size = ECHO_BUF_SIZE;

struct file_operations echo_ops = {
	.owner = THIS_MODULE,
	.open = echo_open,
	.write = echo_write,
	.read = echo_read,
};

module_param(echo_buf_size, int, S_IRUGO);

static int __init module_start(void)
{
	int i, err = 0;
	dev_t devno = 0;

	PDEBUG("Module is loaded\n");
	
	/*
	 * alloc device number
	 */
	err = alloc_chrdev_region(&devno, echo_minor, ECHO_NR_DEVS,
		"echo");
	if (err < 0) {
		printk(KERN_WARNING "Can't alloc device number!");
		return err;
	} else {
		echo_major = MAJOR(devno);
	}

	echo_devs = (struct echo_dev *)
		    kmalloc(ECHO_NR_DEVS * sizeof(*echo_devs), GFP_KERNEL);
	for (i = 0; i < ECHO_NR_DEVS; ++i) {
		devno = MKDEV(echo_major, echo_minor + i);
		atomic_set(&(echo_devs + i)->opt_counter, 0);
		(echo_devs + i)->data_len = 0;
		(echo_devs + i)->data_max_len = echo_buf_size;
		(echo_devs + i)->data = (char *)
		    kmalloc(echo_buf_size * sizeof(char), GFP_KERNEL);
		memset((echo_devs + i)->data, 0, echo_buf_size);
		mutex_init(&(echo_devs + i)->mutex);
		cdev_init(&(echo_devs + i)->cdev, &echo_ops);
		(echo_devs + i)->cdev.owner = THIS_MODULE;
		(echo_devs + i)->cdev.ops = &echo_ops;
		err = cdev_add(&(echo_devs + i)->cdev, devno, 1);
		if (err) {
			printk(KERN_NOTICE "Error %d adding echo%d", err, i);
		}
	}

	return 0;
}

static void __exit module_end(void)
{
	int i, err = 0;
	dev_t devno = MKDEV(echo_major, echo_minor);
	PDEBUG("Module unloaded\n");

	if (echo_devs) {
		for (i = 0; i < ECHO_NR_DEVS; ++i) {
			cdev_del(&(echo_devs + i)->cdev);
		}
	}
	unregister_chrdev_region(devno, ECHO_NR_DEVS);
}

module_init(module_start);
module_exit(module_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Doug");

