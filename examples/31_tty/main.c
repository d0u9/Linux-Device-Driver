// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include "main.h"

static struct tty_driver	*ldd_tty_driver;
static struct ldd_tty_dev	*ldd_dev_table;

/*
 * The "receive interrupt": a character arrives from the line every
 * TIMER_INTERVAL. It goes into the port's flip buffer, and the push hands
 * it to the line discipline, which delivers it to read().
 */
static void ldd_tty_timer_func(struct timer_list *t)
{
	struct ldd_tty_dev *ldd_dev = timer_container_of(ldd_dev, t, timer);
	struct tty_port *port = &ldd_dev->port;

	/* tty_insert_flip_char() makes room itself, or drops the char */
	tty_insert_flip_char(port, 'z', TTY_NORMAL);
	tty_flip_buffer_push(port);

	mod_timer(&ldd_dev->timer, jiffies + TIMER_INTERVAL);
}

static void ldd_tty_flush(struct ldd_tty_dev *ldd_dev)
{
	if (ldd_dev->buffer_curr > 0) {
		ldd_dev->buffer[ldd_dev->buffer_curr] = '\0';
		pr_info("[%s%td] %s\n", MODULE_NAME, ldd_dev - ldd_dev_table,
			ldd_dev->buffer);
		ldd_dev->buffer_curr = 0;
	}
}

/*
 * tty_port_open() calls activate on the first open and tty_port_close()
 * calls shutdown on the last close, so the port counts the opens and the
 * "hardware" runs only while someone has the device open. Both are called
 * with the port mutex held and may sleep.
 */
static int ldd_port_activate(struct tty_port *port, struct tty_struct *tty)
{
	struct ldd_tty_dev *ldd_dev = container_of(port, struct ldd_tty_dev, port);

	mod_timer(&ldd_dev->timer, jiffies + TIMER_INTERVAL);
	return 0;
}

static void ldd_port_shutdown(struct tty_port *port)
{
	struct ldd_tty_dev *ldd_dev = container_of(port, struct ldd_tty_dev, port);
	unsigned long flags;

	/* waits for a running timer function, which may re-arm itself */
	timer_delete_sync(&ldd_dev->timer);

	spin_lock_irqsave(&ldd_dev->lock, flags);
	ldd_tty_flush(ldd_dev);
	spin_unlock_irqrestore(&ldd_dev->lock, flags);
}

static const struct tty_port_operations ldd_port_ops = {
	.activate	= ldd_port_activate,
	.shutdown	= ldd_port_shutdown,
};

static int ldd_tty_open(struct tty_struct *tty, struct file *filp)
{
	struct ldd_tty_dev *ldd_dev = ldd_dev_table + tty->index;

	tty->driver_data = ldd_dev;
	return tty_port_open(&ldd_dev->port, tty, filp);
}

static void ldd_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct ldd_tty_dev *ldd_dev = tty->driver_data;

	tty_port_close(&ldd_dev->port, tty, filp);
}

static void ldd_tty_hangup(struct tty_struct *tty)
{
	struct ldd_tty_dev *ldd_dev = tty->driver_data;

	tty_port_hangup(&ldd_dev->port);
}

/* Collect the written characters into lines and print each with printk. */
static int ldd_tty_printk(struct ldd_tty_dev *ldd_dev,
			  const unsigned char *buf, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (ldd_dev->buffer_curr >= LDD_LINE_LONG) {
			ldd_dev->buffer[ldd_dev->buffer_curr++] = '\\';
			ldd_tty_flush(ldd_dev);
		}

		switch (buf[i]) {
		case '\r':
			ldd_tty_flush(ldd_dev);
			if ((i + 1) < count && buf[i + 1] == '\n')
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

/*
 * write can be called both from process context and, for echoes, from
 * where the line discipline receives characters, so it must not sleep.
 */
static ssize_t ldd_tty_write(struct tty_struct *tty,
			     const u8 *buf, size_t count)
{
	struct ldd_tty_dev *ldd_dev = tty->driver_data;
	unsigned long flags;
	ssize_t rv;

	spin_lock_irqsave(&ldd_dev->lock, flags);
	rv = ldd_tty_printk(ldd_dev, buf, count);
	spin_unlock_irqrestore(&ldd_dev->lock, flags);
	return rv;
}

static unsigned int ldd_tty_write_room(struct tty_struct *tty)
{
	struct ldd_tty_dev *ldd_dev = tty->driver_data;
	unsigned long flags;
	unsigned int rv;

	spin_lock_irqsave(&ldd_dev->lock, flags);
	rv = LDD_TTY_MAX_ROOM - ldd_dev->buffer_curr;
	spin_unlock_irqrestore(&ldd_dev->lock, flags);
	return rv;
}

#define INTEREST_IFLAGS		(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK)

static void ldd_tty_set_termios(struct tty_struct *tty,
				const struct ktermios *old_termios)
{
	unsigned int cflag = tty->termios.c_cflag;

	pr_info("set_termios...\n");

	/* check that they really want us to change something */
	if (old_termios && cflag == old_termios->c_cflag &&
	    (tty->termios.c_iflag & INTEREST_IFLAGS) ==
	    (old_termios->c_iflag & INTEREST_IFLAGS)) {
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
	if (cflag & PARENB) {
		if (cflag & PARODD)
			pr_debug(" - parity = odd\n");
		else
			pr_debug(" - parity = even\n");
	} else {
		pr_debug(" - parity = none\n");
	}

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

	/*
	 * determine software flow control: if we are implementing XON/XOFF,
	 * set the start and stop character in the device
	 */
	if (I_IXOFF(tty) || I_IXON(tty)) {
		unsigned char stop_char  = STOP_CHAR(tty);
		unsigned char start_char = START_CHAR(tty);

		/* if we are implementing INBOUND XON/XOFF */
		if (I_IXOFF(tty))
			pr_debug(" - INBOUND XON/XOFF is enabled, XON = %2x, XOFF = %2x\n",
				 start_char, stop_char);
		else
			pr_debug(" - INBOUND XON/XOFF is disabled\n");

		/* if we are implementing OUTBOUND XON/XOFF */
		if (I_IXON(tty))
			pr_debug(" - OUTBOUND XON/XOFF is enabled, XON = %2x, XOFF = %2x\n",
				 start_char, stop_char);
		else
			pr_debug(" - OUTBOUND XON/XOFF is disabled\n");
	}

	/* get the baud rate wanted */
	pr_debug(" - baud rate = %d\n", tty_get_baud_rate(tty));
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
	unsigned int msr, mcr;
	unsigned long flags;

	spin_lock_irqsave(&ldd_dev->lock, flags);
	msr = ldd_dev->msr;
	mcr = ldd_dev->mcr;
	spin_unlock_irqrestore(&ldd_dev->lock, flags);

	return ((mcr & MCR_DTR)  ? TIOCM_DTR  : 0) |	/* DTR is set */
	       ((mcr & MCR_RTS)  ? TIOCM_RTS  : 0) |	/* RTS is set */
	       ((mcr & MCR_LOOP) ? TIOCM_LOOP : 0) |	/* LOOP is set */
	       ((msr & MSR_CTS)  ? TIOCM_CTS  : 0) |	/* CTS is set */
	       ((msr & MSR_CD)   ? TIOCM_CAR  : 0) |	/* Carrier detect is set */
	       ((msr & MSR_RI)   ? TIOCM_RI   : 0) |	/* Ring Indicator is set */
	       ((msr & MSR_DSR)  ? TIOCM_DSR  : 0);	/* DSR is set */
}

static int ldd_tty_tiocmset(struct tty_struct *tty, unsigned int set,
			    unsigned int clear)
{
	struct ldd_tty_dev *ldd_dev = tty->driver_data;
	unsigned long flags;

	spin_lock_irqsave(&ldd_dev->lock, flags);
	if (set & TIOCM_RTS)
		ldd_dev->mcr |= MCR_RTS;
	if (set & TIOCM_DTR)
		ldd_dev->mcr |= MCR_DTR;

	if (clear & TIOCM_RTS)
		ldd_dev->mcr &= ~MCR_RTS;
	if (clear & TIOCM_DTR)
		ldd_dev->mcr &= ~MCR_DTR;
	spin_unlock_irqrestore(&ldd_dev->lock, flags);
	return 0;
}

/* /proc/tty/driver/lddtty: the lines that are open */
static int ldd_tty_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "lddttyserinfo:1.0 driver:%s\n", "1.0.0");
	for (i = 0; i < LDD_TTY_MINOR_NR; ++i)
		if (tty_port_initialized(&ldd_dev_table[i].port))
			seq_printf(m, "%d\n", i);

	return 0;
}

static const struct tty_operations tty_ops = {
	.open		= ldd_tty_open,
	.close		= ldd_tty_close,
	.hangup		= ldd_tty_hangup,
	.write		= ldd_tty_write,
	.write_room	= ldd_tty_write_room,
	.set_termios	= ldd_tty_set_termios,
	.tiocmget	= ldd_tty_tiocmget,
	.tiocmset	= ldd_tty_tiocmset,
	.proc_show	= ldd_tty_proc_show,
};

static int __init m_init(void)
{
	struct ldd_tty_dev *ldd_dev;
	int rv, i;

	ldd_tty_driver = tty_alloc_driver(LDD_TTY_MINOR_NR,
					  TTY_DRIVER_RESET_TERMIOS |
					  TTY_DRIVER_REAL_RAW);
	if (IS_ERR(ldd_tty_driver))
		return PTR_ERR(ldd_tty_driver);

	ldd_dev_table = kcalloc(LDD_TTY_MINOR_NR, sizeof(*ldd_dev_table),
				GFP_KERNEL);
	if (!ldd_dev_table) {
		rv = -ENOMEM;
		goto err_driver;
	}

	for (i = 0; i < LDD_TTY_MINOR_NR; ++i) {
		ldd_dev = ldd_dev_table + i;
		spin_lock_init(&ldd_dev->lock);
		timer_setup(&ldd_dev->timer, ldd_tty_timer_func, 0);
		tty_port_init(&ldd_dev->port);
		ldd_dev->port.ops = &ldd_port_ops;
		/* line i uses this port; tty_port_install() in .install is the same */
		tty_port_link_device(&ldd_dev->port, ldd_tty_driver, i);
	}

	ldd_tty_driver->driver_name = MODULE_NAME;
	ldd_tty_driver->name = MODULE_NAME;
	ldd_tty_driver->major = 0;	/* dynamic */
	ldd_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	ldd_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	ldd_tty_driver->init_termios = tty_std_termios;
	ldd_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;

	tty_set_operations(ldd_tty_driver, &tty_ops);

	/* registers the major and creates /dev/lddtty0..3 */
	rv = tty_register_driver(ldd_tty_driver);
	if (rv)
		goto err_ports;

	return 0;

err_ports:
	for (i = 0; i < LDD_TTY_MINOR_NR; ++i)
		tty_port_destroy(&ldd_dev_table[i].port);
	kfree(ldd_dev_table);
err_driver:
	tty_driver_kref_put(ldd_tty_driver);
	return rv;
}

static void __exit m_exit(void)
{
	int i;

	/* no line is open: each open holds a reference on the module */
	tty_unregister_driver(ldd_tty_driver);

	for (i = 0; i < LDD_TTY_MINOR_NR; ++i)
		tty_port_destroy(&ldd_dev_table[i].port);

	kfree(ldd_dev_table);
	tty_driver_kref_put(ldd_tty_driver);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("A fake tty device");
