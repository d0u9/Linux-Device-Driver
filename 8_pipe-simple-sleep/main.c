#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#include "global.h"

static int pipe_open(struct inode *, struct file *);

static struct buff_t *buff[PIPE_DEV_NR];
static int pipe_major = 0, pipe_minor = 0;

static struct file_operations fops  = {
	.owner = THIS_MODULE,
	.open = pipe_open,
	/*.write = pipe_write;*/
};


static int __init pipe_init(void)
{
	dev_t devno;
	int err = 0, i = 0;

	PDEBUG("Module loaded!\n");

	err = alloc_chrdev_region(&devno, pipe_minor, PIPE_DEV_NR, "pipe");
	if (err < 0) {
		PDEBUG("Can't get devno major\n");
		return err;
	} else {
		pipe_major = MAJOR(devno);
	}

	for (i = 0; i < PIPE_DEV_NR; ++i) {
		buff[i] = kmalloc(sizeof(struct buff_t), GFP_KERNEL);
		if (!buff[i]) {
			PDEBUG("kmalloc error when i = %d\n", i);
			continue;
		}
		memset(buff[i], 0, sizeof(struct buff_t));

		mutex_init(&buff[i]->mutex_r);
		mutex_init(&buff[i]->mutex_w);
		cdev_init(&buff[i]->dev, &fops);
		buff[i]->dev.owner = THIS_MODULE;

		devno = MKDEV(pipe_major, pipe_minor + i);
		err = cdev_add(&buff[i]->dev, devno, 1);
		if (err) {
			PDEBUG("add dev error at i = %d\n", i);
			kfree(buff[i]);
		}
	}


	return 0;
}

static void __exit pipe_exit(void)
{
	dev_t devno;
	int i = 0;

	PDEBUG("Module un-loaded!\n");

	for (i = 0; i < PIPE_DEV_NR; ++i) {
		cdev_del(&buff[i]->dev);
		kfree(buff[i]);
	}

	devno = MKDEV(pipe_major, pipe_minor);
	unregister_chrdev_region(devno, PIPE_DEV_NR);
}

module_init(pipe_init);
module_exit(pipe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Douglas");

static int pipe_open(struct inode *inode, struct file *file)
{
	PDEBUG("`open` is invoked!\n");

	return 0;
}
