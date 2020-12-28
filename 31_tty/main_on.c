#include <linux/device.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/module.h>

#include "main.h"

static struct tty_driver *ldd_tty_driver;
static struct tty_port ldd_tty_port[LDD_TTY_MINOR_NR];

static int ldd_tty_open(struct tty_struct *tty, struct file *filp)
{
	int idx;
	// tty->driver_data = &tpk_port;

	idx = tty->index;

	pr_info("open, index= %d\n", idx);
	return tty_port_open(ldd_tty_port + idx, tty, filp);
}

static void ldd_tty_close(struct tty_struct *tty, struct file *filp)
{
	// struct ttyprintk_port *tpkp = tty->driver_data;
	// unsigned long flags;
	int idx;
	idx = tty->index;

	pr_info("close, index = %d...\n", idx);

	tty_port_close(ldd_tty_port + idx, tty, filp);
}

/*
 * TTY operations write function.
 */
static int ldd_tty_write(struct tty_struct *tty,
		const unsigned char *buf, int count)
{
	// struct ttyprintk_port *tpkp = tty->driver_data;
	// unsigned long flags;


	/* exclusive use of tpk_printk within this tty */
	pr_info("write ... \n");

	return count;
}

/*
 * TTY operations write_room function.
 */
static int ldd_tty_write_room(struct tty_struct *tty)
{
	return LDD_TTY_MAX_ROOM;
}

static int ldd_tty_ioctl(struct tty_struct *tty,
			unsigned int cmd, unsigned long arg)
{
	// struct ttyprintk_port *tpkp = tty->driver_data;

	// if (!tpkp)
		// return -EINVAL;

	// switch (cmd) {
	// [> Stop TIOCCONS <]
	// case TIOCCONS:
		// return -EOPNOTSUPP;
	// default:
		// return -ENOIOCTLCMD;
	// }
	return 0;
}

static const struct tty_operations ttyprintk_ops = {
	.open		= ldd_tty_open,
	.close		= ldd_tty_close,
	.write		= ldd_tty_write,
	.write_room	= ldd_tty_write_room,
	.ioctl		= ldd_tty_ioctl,
};

static const struct tty_port_operations null_ops = { };

static
int __init m_init(void)
{
	int i, ret = -ENOMEM;

	ldd_tty_driver = tty_alloc_driver(LDD_TTY_MINOR_NR,
			TTY_DRIVER_RESET_TERMIOS |
			TTY_DRIVER_REAL_RAW);

	if (IS_ERR(ldd_tty_driver)) {
		pr_info("-----------x\n");
		return PTR_ERR(ldd_tty_driver);
	}

	for (i = 0; i < LDD_TTY_MINOR_NR; ++i) {
		tty_port_init(ldd_tty_port + i);
		ldd_tty_port[i].ops = &null_ops;
		tty_port_link_device(ldd_tty_port + i, ldd_tty_driver, 0);
	}

	ldd_tty_driver->driver_name = MODULE_NAME;
	ldd_tty_driver->name = MODULE_NAME;
	ldd_tty_driver->major = LDD_TTY_MAJOR;
	ldd_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	ldd_tty_driver->init_termios = tty_std_termios;
	ldd_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	tty_set_operations(ldd_tty_driver, &ttyprintk_ops);

	ret = tty_register_driver(ldd_tty_driver);
	if (ret < 0) {
		printk(KERN_ERR "Couldn't register ttyprintk driver\n");
		goto error;
	}

	return 0;

error:
	put_tty_driver(ldd_tty_driver);
	for (i = 0; i < LDD_TTY_MINOR_NR; ++i)
		tty_port_destroy(ldd_tty_port + i);
	return ret;
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
