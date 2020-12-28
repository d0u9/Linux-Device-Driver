#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include "main.h"

static struct tty_driver	*ldd_tty_driver;
static struct ldd_tty_dev	*ldd_dev_table;

static void ldd_tty_timer_func(struct timer_list *t)
{
	int i, rv, data_size = 1;
	struct ldd_tty_dev *ldd_dev = from_timer(ldd_dev, t, timer);
	struct tty_struct *tty;
	struct tty_port *port;
	char data[1] = { 'z' };

	if (!ldd_dev)
		return;

	tty = ldd_dev->tty;
	port = &ldd_dev->port;

	for (i = 0; i < data_size; ++i) {
		// tty_insert_flip_char() will check room, we needn't do it here.
		// if (!tty_buffer_request_room(port, 1))
			// tty_flip_buffer_push(port);
		rv = tty_insert_flip_char(port, data[0], TTY_NORMAL);
	}
	tty_flip_buffer_push(port);

	ldd_dev->timer.expires = jiffies + TIMER_INTERVAL;
	add_timer(&ldd_dev->timer);
}

static int ldd_tty_open(struct tty_struct *tty, struct file *filp)
{
	int rv = 0, idx;
	unsigned long flags;
	struct ldd_tty_dev *ldd_dev;

	idx = tty->index;
	ldd_dev = ldd_dev_table + idx;
	spin_lock_irqsave(&ldd_dev->spinlock, flags);
	
	
	if (ldd_dev->open_count == 0) {
		rv = tty_port_open(&ldd_dev->port, tty, filp);
		if (rv) {
			pr_debug("Open port failed!\n");
			goto out;
		}

		ldd_dev->index = idx;
		ldd_dev->tty = tty;

		timer_setup(&ldd_dev->timer, ldd_tty_timer_func, 0);
		ldd_dev->timer.expires = jiffies + TIMER_INTERVAL;
		add_timer(&ldd_dev->timer);
	}

	ldd_dev->open_count++;
	tty->driver_data = ldd_dev;

	pr_info("open %d .......\n", ldd_dev->open_count);
out:
	spin_unlock_irqrestore(&ldd_dev->spinlock, flags);
	return rv;
}

static void __ldd_tty_close(struct ldd_tty_dev *ldd_dev)
{
	if (!ldd_dev->open_count) {
		goto out;
	}

	ldd_dev->open_count--;
	if (ldd_dev->open_count <= 0) {
		ldd_dev->open_count = 0;
		ldd_dev->tty = NULL;
		del_timer(&ldd_dev->timer);
	}

out:
	pr_info("close %d .......\n", ldd_dev->open_count);
}

static void ldd_tty_close(struct tty_struct *tty, struct file *filp)
{
	unsigned long flags;
	struct ldd_tty_dev *ldd_dev = tty->driver_data;

	spin_lock_irqsave(&ldd_dev->spinlock, flags);
	__ldd_tty_close(ldd_dev);
	if (ldd_dev->open_count <= 0)
		tty_port_close(&ldd_dev->port, tty, filp);
	spin_unlock_irqrestore(&ldd_dev->spinlock, flags);
}

static void ldd_tty_flush(struct ldd_tty_dev *ldd_dev)
{
	if (ldd_dev->buffer_curr > 0) {
		ldd_dev->buffer[ldd_dev->buffer_curr] = '\0';
		pr_info("[LDDTTY] %s\n", ldd_dev->buffer);
		ldd_dev->buffer_curr = 0;
	}
}

static int ldd_tty_printk(struct ldd_tty_dev *ldd_dev,
			  const unsigned char *buf, int count)
{
	int i = ldd_dev->buffer_curr;

	if (buf == NULL) {
		ldd_tty_flush(ldd_dev);
		return i;
	}

	for (i = 0; i < count; i++) {
		if (ldd_dev->buffer_curr >= LDD_LINE_LONG) {
			ldd_dev->buffer[ldd_dev->buffer_curr++] = '\\';
			ldd_tty_flush(ldd_dev);
		}

		switch (buf[i]) {
		case '\r':
			ldd_tty_flush(ldd_dev);
			if ((i + 1) < count && ldd_dev->buffer[i + 1] == '\n')
				i++;
			break;
		case '\n':
			ldd_tty_flush(ldd_dev);
			break;
		default:
			ldd_dev->buffer[ldd_dev->buffer_curr++] = buf[i];
			break;
		}
	}

	return count;
}

/**
 * write function can be called from both interrupt context and user context.
 *
 * If you really want to sleep, make sure to check first whether the driver is
 * in interrupt context by calling in_interrupt().
 */
static int ldd_tty_write(struct tty_struct *tty,
		     const unsigned char *buf, int count)
{
	int rv;
	unsigned long flags;
	struct ldd_tty_dev *ldd_dev = tty->driver_data;

	if (!ldd_dev)
		return -ENODEV;

	spin_lock_irqsave(&ldd_dev->spinlock, flags);
	rv = ldd_tty_printk(ldd_dev, buf, count);
	spin_unlock_irqrestore(&ldd_dev->spinlock, flags);
	return rv;
}

static int ldd_tty_write_room(struct tty_struct *tty)
{
	int rv;
	unsigned long flags;
	struct ldd_tty_dev *ldd_dev = tty->driver_data;

	if (!ldd_dev)
		return -ENODEV;

	spin_lock_irqsave(&ldd_dev->spinlock, flags);
	rv = LDD_TTY_MAX_ROOM - ldd_dev->buffer_curr;
	spin_unlock_irqrestore(&ldd_dev->spinlock, flags);
	return rv;
}

#define INTEREST_IFLAGS		(IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK)

static void ldd_tty_set_termios(struct tty_struct *tty,
				struct ktermios *old_termios)
{
	unsigned int cflag;
	cflag = tty->termios.c_cflag;

	pr_info("set_termios...\n");

	/* check that they really want us to change something */
	if (old_termios && 
	    (cflag == old_termios->c_cflag) &&
	    ( (tty->termios.c_iflag & INTEREST_IFLAGS) == 
	      (old_termios->c_iflag & INTEREST_IFLAGS) )
	   ) {
		pr_info(" - nothing to change...\n");
		return;
	}

	/* get the byte size */
	switch (cflag & CSIZE) {
	case CS5:
		pr_debug(" - data bits = 5\n");
		break;
	case CS6:
		pr_debug(" - data bits = 6\n");
		break;
	case CS7:
		pr_debug(" - data bits = 7\n");
		break;
	default:
	case CS8:
		pr_debug(" - data bits = 8\n");
		break;
	}

	/* determine the parity */
	if (cflag & PARENB)
		if (cflag & PARODD)
			pr_debug(" - parity = odd\n");
		else
			pr_debug(" - parity = even\n");
	else
		pr_debug(" - parity = none\n");

	/* figure out the stop bits requested */
	if (cflag & CSTOPB)
		pr_debug(" - stop bits = 2\n");
	else
		pr_debug(" - stop bits = 1\n");

	/* figure out the hardware flow control settings */
	if (cflag & CRTSCTS)
		pr_debug(" - RTS/CTS is enabled\n");
	else
		pr_debug(" - RTS/CTS is disabled\n");

	/* determine software flow control */
	/* if we are implementing XON/XOFF, set the start and
	 * stop character in the device */
	if (I_IXOFF(tty) || I_IXON(tty)) {
		unsigned char stop_char  = STOP_CHAR(tty);
		unsigned char start_char = START_CHAR(tty);

		/* if we are implementing INBOUND XON/XOFF */
		if (I_IXOFF(tty))
			pr_debug(" - INBOUND XON/XOFF is enabled, "
				 "XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			pr_debug(" - INBOUND XON/XOFF is disabled");

		/* if we are implementing OUTBOUND XON/XOFF */
		if (I_IXON(tty))
			pr_debug(" - OUTBOUND XON/XOFF is enabled, "
				 "XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			pr_debug(" - OUTBOUND XON/XOFF is disabled");
	}

	/* get the baud rate wanted */
	pr_debug(" - baud rate = %d", tty_get_baud_rate(tty));
}

#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define MCR_LOOP	0x04
#define MSR_CTS		0x08
#define MSR_CD		0x10
#define MSR_RI		0x20
#define MSR_DSR		0x40

static int ldd_tty_tiocmget(struct tty_struct *tty)
{
	struct ldd_tty_dev *ldd_dev = tty->driver_data;

	unsigned int result = 0;
	unsigned int msr = ldd_dev->msr;
	unsigned int mcr = ldd_dev->mcr;

	result = ((mcr & MCR_DTR)  ? TIOCM_DTR  : 0) |	/* DTR is set */
		 ((mcr & MCR_RTS)  ? TIOCM_RTS  : 0) |	/* RTS is set */
		 ((mcr & MCR_LOOP) ? TIOCM_LOOP : 0) |	/* LOOP is set */
		 ((msr & MSR_CTS)  ? TIOCM_CTS  : 0) |	/* CTS is set */
		 ((msr & MSR_CD)   ? TIOCM_CAR  : 0) |	/* Carrier detect is set*/
		 ((msr & MSR_RI)   ? TIOCM_RI   : 0) |	/* Ring Indicator is set */
		 ((msr & MSR_DSR)  ? TIOCM_DSR  : 0);	/* DSR is set */

	return result;
}

static int ldd_tty_tiocmset(struct tty_struct *tty, unsigned int set,
			    unsigned int clear)
{
	struct ldd_tty_dev *ldd_dev = tty->driver_data;
	unsigned int mcr = ldd_dev->mcr;

	if (set & TIOCM_RTS)
		mcr |= MCR_RTS;
	if (set & TIOCM_DTR)
		mcr |= MCR_RTS;

	if (clear & TIOCM_RTS)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_DTR)
		mcr &= ~MCR_RTS;

	/* set the new MCR value in the device */
	ldd_dev->mcr = mcr;
	return 0;
}

static int ldd_tty_proc_show(struct seq_file *m, void *v)
{
	int i;
	unsigned long flags;
	struct ldd_tty_dev *ldd_dev;

	seq_printf(m, "lddttyserinfo:1.0 driver:%s\n", "1.0.0");
	for (i = 0; i < LDD_TTY_MINOR_NR; ++i) {
		ldd_dev = ldd_dev_table + i;
		spin_lock_irqsave(&ldd_dev->spinlock, flags);
		if (ldd_dev->open_count == 0) {
			spin_unlock_irqrestore(&ldd_dev->spinlock, flags);
			continue;
		}

		spin_unlock_irqrestore(&ldd_dev->spinlock, flags);
		seq_printf(m, "%d\n", i);
	}

	return 0;
}

static int ldd_tty_ioctl(struct tty_struct *tty, unsigned int cmd,
			 unsigned long arg)
{
	return -ENOIOCTLCMD;
}

static const struct tty_operations tty_ops = {
	.open		= ldd_tty_open,
	.close		= ldd_tty_close,
	.write		= ldd_tty_write,
	.write_room	= ldd_tty_write_room,
	.set_termios	= ldd_tty_set_termios,
	.tiocmget	= ldd_tty_tiocmget,
	.tiocmset	= ldd_tty_tiocmset,
	.proc_show	= ldd_tty_proc_show,
	.ioctl		= ldd_tty_ioctl,
};

static const struct tty_port_operations null_ops = { };

static
int __init m_init(void)
{
	int rv = -ENOMEM, i;

	/*
	 * alloc_tty_driver is deprecated
	 */
	// ldd_tty_driver = alloc_tty_driver(LDD_TTY_MINOR_NR);
	ldd_tty_driver = tty_alloc_driver(LDD_TTY_MINOR_NR,
					  TTY_DRIVER_RESET_TERMIOS |
					  TTY_DRIVER_REAL_RAW);
	if (IS_ERR(ldd_tty_driver))
		return PTR_ERR(ldd_tty_driver);

	ldd_dev_table = kcalloc(LDD_TTY_MINOR_NR,
				sizeof(struct ldd_tty_dev), GFP_KERNEL);
	if (!ldd_dev_table) {
		put_tty_driver(ldd_tty_driver);
		return -ENOMEM;
	}

	for (i = 0; i < LDD_TTY_MINOR_NR; ++i) {
		ldd_dev_table[i].open_count = 0;
		spin_lock_init(&ldd_dev_table[i].spinlock);
		tty_port_init(&ldd_dev_table[i].port);
		ldd_dev_table[i].port.ops = &null_ops;
		tty_port_link_device(&ldd_dev_table[i].port, ldd_tty_driver, i);
	}

	ldd_tty_driver->driver_name = MODULE_NAME;
	ldd_tty_driver->name = MODULE_NAME;
	ldd_tty_driver->major = LDD_TTY_MAJOR;
	ldd_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	ldd_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	ldd_tty_driver->init_termios = tty_std_termios;
	ldd_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;

	tty_set_operations(ldd_tty_driver, &tty_ops);

	rv = tty_register_driver(ldd_tty_driver);
	if (rv < 0) {
		put_tty_driver(ldd_tty_driver);
		goto err;
	}

	pr_debug("load2 .......");
	return 0;

err:
	put_tty_driver(ldd_tty_driver);
	for (i = 0; i < LDD_TTY_MINOR_NR; ++i) {
		tty_port_destroy(&ldd_dev_table[i].port);
	}
	return rv;
}

static void destroy_ldd_dev(struct ldd_tty_dev *ldd_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&ldd_dev->spinlock, flags);
	while (ldd_dev->open_count) {
		__ldd_tty_close(ldd_dev);
	}
	spin_unlock_irqrestore(&ldd_dev->spinlock, flags);
}

static
void __exit m_exit(void)
{
	int i;

	tty_unregister_driver(ldd_tty_driver);

	for (i = 0; i < LDD_TTY_MINOR_NR; ++i) {
		destroy_ldd_dev(ldd_dev_table + i);
		tty_port_destroy(&ldd_dev_table[i].port);
		del_timer(&ldd_dev_table[i].timer);
	}

	kfree(ldd_dev_table);
	put_tty_driver(ldd_tty_driver);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("A fake tty device");
