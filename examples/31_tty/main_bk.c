#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>

#include "main.h"

static struct tty_driver	*ldd_tty_driver;
static struct tty_port		ldd_tty_port[LDD_TTY_MINOR_NR];
static struct ldd_tty_dev	*ldd_dev_table[LDD_TTY_MINOR_NR];

static int ldd_tty_open(struct tty_struct *tty, struct file *filp)
{
	int ret, index;
	struct ldd_tty_dev *ldd_dev;

	index = tty->index;
	ldd_dev = ldd_dev_table[index];
	if (!ldd_dev) {
		ret = tty_port_open(ldd_tty_port + index, tty, filp);
		if (ret)
			return ret;

		ldd_dev = kzalloc(sizeof(struct ldd_tty_dev), GFP_KERNEL);
		if (!ldd_dev)
			return -ENOMEM;

		ldd_dev->index = index;
		mutex_init(&ldd_dev->mutex);
		ldd_dev->open_count = 0;

		ldd_dev_table[index] = ldd_dev;

	}

	mutex_lock(&ldd_dev->mutex);
	tty->driver_data = ldd_dev;
	ldd_dev->tty = tty;
	ldd_dev->open_count++;

	mutex_unlock(&ldd_dev->mutex);

	pr_info("open .......");

	return 0;
}

static void ldd_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct ldd_tty_dev *ldd_dev = tty->driver_data;

	mutex_lock(&ldd_dev->mutex);
	if (!ldd_dev->open_count) {
		goto out;
	}

	ldd_dev->open_count--;
	if (ldd_dev->open_count <= 0) {
		// tty_port_close(ldd_tty_port + i, tty, filp);
	}

out:
	pr_info("close .......");
	mutex_unlock(&ldd_dev->mutex);

}

static int ldd_tty_write(struct tty_struct *tty,
		     const unsigned char *buf, int count)
{

	pr_info("=---- write\n");
	return 0;
}

static int ldd_tty_write_room(struct tty_struct *tty)
{
	return LDD_TTY_MAX_ROOM;
}

static const struct tty_operations tty_ops = {
	.open		= ldd_tty_open,
	.close		= ldd_tty_close,
	.write		= ldd_tty_write,
	.write_room	= ldd_tty_write_room,
};

static const struct tty_port_operations null_ops = { };

static
int __init m_init(void)
{
	int rv, i;

	/*
	 * alloc_tty_driver is deprecated
	 */
	// ldd_tty_driver = alloc_tty_driver(LDD_TTY_MINOR_NR);
	ldd_tty_driver = tty_alloc_driver(LDD_TTY_MINOR_NR,
					  TTY_DRIVER_RESET_TERMIOS |
					  TTY_DRIVER_REAL_RAW);
	if (IS_ERR(ldd_tty_driver))
		return PTR_ERR(ldd_tty_driver);

#if 0
	ldd_tty_driver->driver_name = MODULE_NAME;
	ldd_tty_driver->name = MODULE_NAME;
	ldd_tty_driver->major = LDD_TTY_MAJOR;
	ldd_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	ldd_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	ldd_tty_driver->init_termios = tty_std_termios;
	ldd_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;

#else
	ldd_tty_driver->driver_name = MODULE_NAME;
	ldd_tty_driver->name = MODULE_NAME;
	ldd_tty_driver->major = TTYAUX_MAJOR;
	ldd_tty_driver->minor_start = 3;
	ldd_tty_driver->type = TTY_DRIVER_TYPE_CONSOLE;
	ldd_tty_driver->init_termios = tty_std_termios;
	ldd_tty_driver->init_termios.c_oflag = OPOST | OCRNL | ONOCR | ONLRET;
#endif

	tty_set_operations(ldd_tty_driver, &tty_ops);

	for (i = 0; i < LDD_TTY_MINOR_NR; ++i) {
		tty_port_init(ldd_tty_port + i);
		ldd_tty_port[i].ops = &null_ops;
		tty_port_link_device(ldd_tty_port + i, ldd_tty_driver, i);
	}

	rv = tty_register_driver(ldd_tty_driver);
	if (rv < 0) {
		put_tty_driver(ldd_tty_driver);
		return rv;
	}

	pr_debug("load2 .......");
	return 0;
}

static
void __exit m_exit(void)
{
	int i;

	tty_unregister_driver(ldd_tty_driver);
	put_tty_driver(ldd_tty_driver);

	for (i = 0; i < LDD_TTY_MINOR_NR; ++i)
		tty_port_destroy(ldd_tty_port + i);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("A fake tty device");
