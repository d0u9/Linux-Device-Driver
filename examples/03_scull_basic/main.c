#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/list.h>

#include "main.h"
#include "fops.h"

static int scull_major = 0, scull_minor = 0;

static struct scull_dev *scull_dev[SCULL_NR_DEVS];

static struct file_operations scull_fops = {
	.owner   = THIS_MODULE,
	.open    = scull_open,
	.read    = scull_read,
	.write   = scull_write,
	.release = scull_release,
};

static
void __init init_scull_dev(struct scull_dev *dev)
{
	dev->block_counter = 0;

	INIT_LIST_HEAD(&dev->block_list);
	mutex_init(&dev->mutex);

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
}

static
int __init m_init(void)
{
	int err = 0;
	dev_t devno;

	printk(KERN_WARNING MODULE_NAME " is loaded\n");

	//Alloc device number
	err = alloc_chrdev_region(&devno, scull_minor, SCULL_NR_DEVS, MODULE_NAME);
	if (err < 0) {
		pr_debug("Cant't get major");
		return err;
	}
	scull_major = MAJOR(devno);

	for (int i = 0; i < SCULL_NR_DEVS; ++i) {
		scull_dev[i] = kmalloc(sizeof(struct scull_dev), GFP_KERNEL);
		if (!scull_dev[i]) {
			pr_debug("Error(%d): kmalloc failed on scull%d\n", err, i);
			continue;
		}

		init_scull_dev(scull_dev[i]);
	
		/*
		 * The cdev_add() function will make this char device usable
		 * in userspace. If you havn't ready to populate this device
		 * to its users, DO NOT call cdev_add()
		 */
		devno = MKDEV(scull_major, scull_minor + i);
		err = cdev_add(&scull_dev[i]->cdev, devno, 1);
		if (err) {
			pr_debug("Error(%d): Adding %s%d error\n", err, MODULE_NAME, i);
			kfree(scull_dev[i]);
			scull_dev[i] = NULL;
		}
	}

	// TODO: unregister chrdev_region here if fail
	return 0;
}

static
void __exit m_exit(void)
{
	dev_t devno;
	printk(KERN_WARNING MODULE_NAME " unloaded\n");

	for (int i = 0; i < SCULL_NR_DEVS; ++i) {
		cdev_del(&scull_dev[i]->cdev);
		scull_trim(scull_dev[i]);
		kfree(scull_dev[i]);
	}

	devno = MKDEV(scull_major, scull_minor);
	unregister_chrdev_region(devno, SCULL_NR_DEVS);
}


module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("A simple memory based storage device aims to demonstrate "
		   "basic concepts of char device");
