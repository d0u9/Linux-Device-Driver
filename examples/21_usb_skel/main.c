// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kstrtox.h>
#include <linux/poll.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/usb.h>

#include "main.h"

/* The devices this driver handles, matched by vendor and product ID. */
static const struct usb_device_id skel_table[] = {
	{ USB_DEVICE(LDD_USB_VENDOR_ID, LDD_USB_PRODUCT_ID) },
	{ }
};
MODULE_DEVICE_TABLE(usb, skel_table);

static struct usb_driver skel_driver;

/* Called when the last reference goes: disconnected and every file closed. */
static void skel_delete(struct kref *kref)
{
	struct usb_skel *dev = container_of(kref, struct usb_skel, kref);

	if (dev->tick_buffer)
		usb_free_coherent(dev->udev, LDD_USB_TICK_LEN,
				  dev->tick_buffer, dev->tick_urb->transfer_dma);
	usb_free_urb(dev->tick_urb);	/* NULL is fine */
	kfree(dev->bulk_in_buffer);
	usb_put_dev(dev->udev);		/* last: the frees above use udev */
	kfree(dev);
}

/*
 * Completion of the interrupt URB, in interrupt context. ldd-usb NAKs until
 * the next tick, so this runs once per tick. Count it, wake up readers and
 * submit the URB again to wait for the next one.
 */
static void skel_tick_complete(struct urb *urb)
{
	struct usb_skel *dev = urb->context;
	int err;

	switch (urb->status) {
	case 0:
		break;
	case -ECONNRESET:	/* usb_kill_urb() */
	case -ENOENT:
	case -ESHUTDOWN:	/* device gone */
		return;
	default:		/* transient error: try again */
		dev_dbg(&dev->interface->dev, "tick status %d\n",
			urb->status);
		goto resubmit;
	}

	if (urb->actual_length == LDD_USB_TICK_LEN &&
	    le32_to_cpu(dev->tick_buffer[0]) == 1) {
		dev_dbg(&dev->interface->dev, "tick, interval %u s\n",
			le32_to_cpu(dev->tick_buffer[1]));
		atomic_inc(&dev->ticks);
		wake_up_interruptible(&dev->tick_wait);
	}

resubmit:
	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err && err != -EPERM)	/* -EPERM: being killed */
		dev_err(&dev->interface->dev, "cannot resubmit tick: %d\n",
			err);
}

static int skel_open(struct inode *inode, struct file *filp)
{
	struct usb_interface *interface;
	struct skel_file *file;
	struct usb_skel *dev;
	int err = 0;

	/*
	 * usb_register_dev() gave this interface its minor. The USB core
	 * holds a lock while calling open() that usb_deregister_dev() also
	 * takes, so the interface data cannot be freed under us here.
	 */
	interface = usb_find_interface(&skel_driver, iminor(inode));
	if (!interface)
		return -ENODEV;
	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;

	file = kzalloc_obj(*file);
	if (!file)
		return -ENOMEM;

	mutex_lock(&dev->io_mutex);
	if (dev->disconnected) {
		err = -ENODEV;
	} else if (dev->open_count++ == 0) {
		/* first open: start polling the interrupt endpoint */
		err = usb_submit_urb(dev->tick_urb, GFP_KERNEL);
		if (err)
			dev->open_count--;
	}
	mutex_unlock(&dev->io_mutex);

	if (err) {
		kfree(file);
		return err;
	}

	kref_get(&dev->kref);		/* the file keeps dev alive */
	file->dev = dev;
	file->seen = atomic_read(&dev->ticks);
	filp->private_data = file;

	return stream_open(inode, filp);
}

static int skel_release(struct inode *inode, struct file *filp)
{
	struct skel_file *file = filp->private_data;
	struct usb_skel *dev = file->dev;

	mutex_lock(&dev->io_mutex);
	/* last close: stop polling; after disconnect it is stopped already */
	if (--dev->open_count == 0 && !dev->disconnected)
		usb_kill_urb(dev->tick_urb);
	mutex_unlock(&dev->io_mutex);

	kref_put(&dev->kref, skel_delete);
	kfree(file);
	return 0;
}

/*
 * Wait for a tick this file has not seen yet, then read the time of the
 * last tick from the bulk IN endpoint and return it as one line.
 */
static ssize_t skel_read(struct file *filp, char __user *buf, size_t count,
			 loff_t *f_pos)
{
	struct skel_file *file = filp->private_data;
	struct usb_skel *dev = file->dev;
	int ticks, actual, err;
	size_t len;

	if (filp->f_flags & O_NONBLOCK) {
		if (atomic_read(&dev->ticks) == file->seen)
			return -EAGAIN;
	} else if (wait_event_interruptible(dev->tick_wait,
				atomic_read(&dev->ticks) != file->seen ||
				READ_ONCE(dev->disconnected))) {
		return -ERESTARTSYS;
	}
	ticks = atomic_read(&dev->ticks);

	if (mutex_lock_interruptible(&dev->io_mutex))
		return -ERESTARTSYS;

	if (dev->disconnected) {
		err = -ENODEV;
		goto out;
	}

	/* synchronous: sleeps until the transfer is done or times out */
	err = usb_bulk_msg(dev->udev,
			   usb_rcvbulkpipe(dev->udev, dev->bulk_in_addr),
			   dev->bulk_in_buffer, dev->bulk_in_size, &actual,
			   SKEL_TIMEOUT_MS);
	if (err)
		goto out;

	/* "YYYY-MM-DD HH:MM:SS\0" -> "YYYY-MM-DD HH:MM:SS\n" */
	len = strnlen(dev->bulk_in_buffer, actual);
	if (len == dev->bulk_in_size)
		len--;
	dev->bulk_in_buffer[len++] = '\n';
	len = min(len, count);

	if (copy_to_user(buf, dev->bulk_in_buffer, len)) {
		err = -EFAULT;
		goto out;
	}
	file->seen = ticks;
	err = len;
out:
	mutex_unlock(&dev->io_mutex);
	return err;
}

/* Completion of a write URB: free its buffer, report errors. */
static void skel_write_complete(struct urb *urb)
{
	if (urb->status && urb->status != -ENOENT &&
	    urb->status != -ECONNRESET && urb->status != -ESHUTDOWN)
		dev_err(&urb->dev->dev, "interval write failed: %d\n",
			urb->status);

	kfree(urb->transfer_buffer);
}

/*
 * Set the interval: the text written, e.g. "5", is sent as a 32-bit
 * little-endian number to the bulk OUT endpoint, asynchronously. write()
 * returns as soon as the URB is queued.
 */
static ssize_t skel_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *f_pos)
{
	struct skel_file *file = filp->private_data;
	struct usb_skel *dev = file->dev;
	unsigned int interval;
	__le32 *data;
	struct urb *urb;
	int err;

	err = kstrtouint_from_user(buf, count, 0, &interval);
	if (err)
		return err;

	/* URB buffers must be DMA-able: kmalloc(), never the stack */
	data = kmalloc_obj(*data);
	if (!data)
		return -ENOMEM;
	*data = cpu_to_le32(interval);

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		err = -ENOMEM;
		goto err_free_data;
	}
	usb_fill_bulk_urb(urb, dev->udev,
			  usb_sndbulkpipe(dev->udev, dev->bulk_out_addr),
			  data, sizeof(*data), skel_write_complete, dev);

	mutex_lock(&dev->io_mutex);
	if (dev->disconnected)
		err = -ENODEV;
	else
		err = usb_submit_urb(urb, GFP_KERNEL);
	mutex_unlock(&dev->io_mutex);

	/*
	 * Drop our reference. A submitted URB stays alive until it completes;
	 * skel_write_complete() frees the data then.
	 */
	usb_free_urb(urb);
	if (err)
		goto err_free_data;

	return count;

err_free_data:
	kfree(data);
	return err;
}

static __poll_t skel_poll(struct file *filp, poll_table *wait)
{
	struct skel_file *file = filp->private_data;
	struct usb_skel *dev = file->dev;
	__poll_t mask = 0;

	poll_wait(filp, &dev->tick_wait, wait);

	/* only look, never change state: poll may be called many times */
	if (atomic_read(&dev->ticks) != file->seen)
		mask |= EPOLLIN | EPOLLRDNORM;
	if (READ_ONCE(dev->disconnected))
		mask |= EPOLLHUP | EPOLLERR;

	return mask;
}

static const struct file_operations skel_fops = {
	.owner   = THIS_MODULE,
	.open    = skel_open,
	.release = skel_release,
	.read    = skel_read,
	.write   = skel_write,
	.poll    = skel_poll,
};

/*
 * usb_register_dev() creates /dev/skel0, skel1, ... with these fops. Older
 * drivers wrote "usb/skel%d" here; only the part after the last '/' is the
 * device name now, and a node under /dev/usb/ needs a .devnode callback.
 */
static struct usb_class_driver skel_class = {
	.name       = "skel%d",
	.fops       = &skel_fops,
	.minor_base = SKEL_MINOR_BASE,
};

static int skel_probe(struct usb_interface *interface,
		      const struct usb_device_id *id)
{
	struct usb_endpoint_descriptor *bulk_in, *bulk_out, *int_in;
	struct usb_skel *dev;
	int err;

	dev = kzalloc_obj(*dev);
	if (!dev)
		return -ENOMEM;

	kref_init(&dev->kref);
	mutex_init(&dev->io_mutex);
	init_waitqueue_head(&dev->tick_wait);
	atomic_set(&dev->ticks, 0);
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* find the first bulk IN, bulk OUT and interrupt IN endpoints */
	err = usb_find_common_endpoints(interface->cur_altsetting,
					&bulk_in, &bulk_out, &int_in, NULL);
	if (err) {
		dev_err(&interface->dev, "missing bulk or interrupt endpoint\n");
		goto err_put;
	}

	dev->bulk_in_addr = bulk_in->bEndpointAddress;
	dev->bulk_in_size = usb_endpoint_maxp(bulk_in);
	dev->bulk_in_buffer = kmalloc(dev->bulk_in_size, GFP_KERNEL);
	dev->bulk_out_addr = bulk_out->bEndpointAddress;

	/*
	 * The interrupt URB is submitted again and again, so its buffer is
	 * allocated once as coherent DMA memory and the URB is told not to
	 * map it each time (URB_NO_TRANSFER_DMA_MAP). One-off buffers, like
	 * the bulk ones, are plain kmalloc() and the core maps them.
	 */
	dev->tick_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (dev->tick_urb)
		dev->tick_buffer = usb_alloc_coherent(dev->udev,
						      LDD_USB_TICK_LEN,
						      GFP_KERNEL,
						      &dev->tick_urb->transfer_dma);
	if (!dev->bulk_in_buffer || !dev->tick_urb || !dev->tick_buffer) {
		err = -ENOMEM;
		goto err_put;
	}

	usb_fill_int_urb(dev->tick_urb, dev->udev,
			 usb_rcvintpipe(dev->udev, int_in->bEndpointAddress),
			 dev->tick_buffer, LDD_USB_TICK_LEN,
			 skel_tick_complete, dev, int_in->bInterval);
	dev->tick_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	usb_set_intfdata(interface, dev);

	err = usb_register_dev(interface, &skel_class);
	if (err) {
		dev_err(&interface->dev, "cannot get a minor: %d\n", err);
		usb_set_intfdata(interface, NULL);
		goto err_put;
	}

	dev_info(&interface->dev, "attached as /dev/skel%d\n",
		 interface->minor - SKEL_MINOR_BASE);
	return 0;

err_put:
	kref_put(&dev->kref, skel_delete);
	return err;
}

/*
 * The device was unplugged, or the driver is going away. Open files may
 * stay open: mark the device gone so their read() and write() fail, stop
 * the interrupt URB, and let the last close free everything.
 */
static void skel_disconnect(struct usb_interface *interface)
{
	struct usb_skel *dev = usb_get_intfdata(interface);
	int minor = interface->minor;

	usb_deregister_dev(interface, &skel_class);	/* no new opens */

	mutex_lock(&dev->io_mutex);
	dev->disconnected = true;
	mutex_unlock(&dev->io_mutex);

	usb_kill_urb(dev->tick_urb);
	wake_up_interruptible(&dev->tick_wait);		/* readers see it */

	usb_set_intfdata(interface, NULL);
	kref_put(&dev->kref, skel_delete);

	dev_info(&interface->dev, "skel%d disconnected\n",
		 minor - SKEL_MINOR_BASE);
}

static struct usb_driver skel_driver = {
	.name       = MODULE_NAME,
	.id_table   = skel_table,
	.probe      = skel_probe,
	.disconnect = skel_disconnect,
};

/* module_usb_driver(skel_driver) generates exactly these two functions */
static int __init m_init(void)
{
	return usb_register(&skel_driver);
}

static void __exit m_exit(void)
{
	usb_deregister(&skel_driver);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("USB driver skeleton for the ldd-usb device");
