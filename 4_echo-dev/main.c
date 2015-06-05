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

static inline void init_it(struct echo_dev *dev, int index)
{
	int err = 0;
	dev_t devno = MKDEV(echo_major, echo_minor + index);

	atomic_set(&dev->opt_counter, 0);
	dev->data_len = 0;
	dev->data_max_len = echo_buf_size;
	dev->data = (char *)kmalloc(echo_buf_size * sizeof(char), GFP_KERNEL);
	memset(dev->data, 0, echo_buf_size);
	
	mutex_init(&dev->mutex);
	cdev_init(&dev->cdev, &echo_ops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &echo_ops;
	if ((err = cdev_add(&dev->cdev, devno, 1)))
		printk(KERN_NOTICE "Error %d adding echo%d", err, index);
}

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
		init_it(echo_devs + i, i);
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
			kfree((echo_devs + i)->data);
		}
	}
	kfree(echo_devs);
	unregister_chrdev_region(devno, ECHO_NR_DEVS);
}

module_init(module_start);
module_exit(module_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Douglas");

