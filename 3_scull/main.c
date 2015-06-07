#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <asm/atomic.h>

#include "main.h"
#include "fops.h"

#define MODULE_NAME "scull"

static struct scull_dev *scull_dev[SCULL_NR_DEVS];
static int scull_minor = 0, scull_major = 0;
static struct file_operations scull_fops = {
	.owner = THIS_MODULE,
	.open =  scull_open,
	.release = scull_release,
	.read = scull_read,
	.write = scull_write,
};

static void init_scull_dev(struct scull_dev *dev, int index)
{
	atomic_set(&dev->open_counter, 0);

	INIT_LIST_HEAD(&dev->list);
	dev->list_entry_counter = 0;
	mutex_init(&dev->mutex);

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
}

static int __init module_start(void)
{
	dev_t devno;
	int err = 0, i = 0;
	printk(KERN_WARNING "\n" MODULE_NAME " loaded\n");

	//Alloc device number
	err = alloc_chrdev_region(&devno, scull_minor, SCULL_NR_DEVS, "scull");
	if (err < 0) {
		PDEBUG("Cant't get major");
		return err;
	} else {
		scull_major = MAJOR(devno);
	}

	for (i = 0; i < SCULL_NR_DEVS; ++i) {
		scull_dev[i] = (struct scull_dev *)
			kmalloc(sizeof(struct scull_dev), GFP_KERNEL);
		init_scull_dev(scull_dev[i], i);
	
		/*
		 * `cdev_add` will start up this module and other users can
		 * recognise this module and start using it. So if you havn't
		 * ready here, don't call this function.
		 */
		devno = MKDEV(scull_major, scull_minor + i);
		err = cdev_add(&scull_dev[i]->cdev, devno, 1);
		if (err) {
			PDEBUG("Error %d adding scull%d\n", err, i);
			kfree(scull_dev[i]);
		}
	}
	return 0;
}

static void __exit module_end(void)
{
	int i = 0;
	dev_t devno;

	printk(KERN_WARNING MODULE_NAME " unloaded\n");

	for (i = 0; i < SCULL_NR_DEVS; ++i) {
		cdev_del(&scull_dev[i]->cdev);
		scull_trim(scull_dev[i]);
		kfree(scull_dev[i]);
	}

	devno = MKDEV(scull_major, scull_minor);
	unregister_chrdev_region(devno, SCULL_NR_DEVS);
}


module_init(module_start);
module_exit(module_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Douglas Su");
MODULE_DESCRIPTION("A simple memory base I/O device driver");
