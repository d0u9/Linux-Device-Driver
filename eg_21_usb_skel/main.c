#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/poll.h>

#include "main.h"

#define USB_VENDOR_ID		0x1234
#define USB_PRODUCT_ID		0x7863

#define to_skel_dev(d)	container_of(d, struct usb_skel, kref)

#define USB_SKEL_MINOR_BASE	192

static struct usb_device_id skel_table[] = {
	{ USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
	{ }
};
MODULE_DEVICE_TABLE(usb, skel_table);

static void skel_delete(struct kref *kref)
{
	struct usb_skel *dev = to_skel_dev(kref);

	usb_put_dev(dev->udev);

	if (dev->int_in_buffer)
		usb_free_coherent(dev->udev, 8, dev->int_in_buffer, dev->irq_urb->transfer_dma);

	if (dev->irq_urb)
		usb_free_urb(dev->irq_urb);

	if (dev->bulk_in_buffer)
		kfree(dev->bulk_in_buffer);

	kfree(dev);
}

static struct usb_driver skel_driver;

static int skel_open(struct inode *inode, struct file *file)
{
	struct usb_skel *dev;
	struct usb_interface *interface;
	int subminor;

	subminor = iminor(inode);
	interface = usb_find_interface(&skel_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d",
		       __FUNCTION__, subminor);
		return -ENODEV;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		return -ENODEV;
	}

	kref_get(&dev->kref);

	file->private_data = dev;

	if (usb_submit_urb(dev->irq_urb, GFP_KERNEL))
		return -EIO;

	return 0;
}

static int skel_release(struct inode *inode, struct file *file)
{
	struct usb_skel *dev;

	dev = (struct usb_skel *)file->private_data;
	if (dev == NULL)
		return -ENODEV;

	usb_kill_urb(dev->irq_urb);
	kref_put(&dev->kref, skel_delete);

	return 0;
}

static ssize_t skel_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	struct usb_skel *dev;
	int retval = 0;

	dev = (struct usb_skel*)file->private_data;

	retval = usb_bulk_msg(dev->udev,
			      usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
			      dev->bulk_in_buffer,
			      min(dev->bulk_in_size, count),
			      (int *)&count, HZ * 10);

	pr_debug("count = %ld\n", count);

	if (!retval) {
		if (copy_to_user(buffer, dev->bulk_in_buffer, count)) 
			retval = -EFAULT;
		else
			retval = count;
	}

	return count;
}

static void skel_write_bulk_callback(struct urb *urb)
{
	struct usb_skel *dev = urb->context;

	/* sync/async unlink faults aren't errors */
	if (urb->status && 
	    !(urb->status == -ENOENT || 
	      urb->status == -ECONNRESET ||
	      urb->status == -ESHUTDOWN)) {
		dev_dbg(&dev->interface->dev,
			"%s - nonzero write bulk status received: %d",
			__FUNCTION__, urb->status);
	}

	/* free up our allocated buffer */
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			urb->transfer_buffer, urb->transfer_dma);
}

static ssize_t skel_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct usb_skel *dev;
	int retval = 0;
	struct urb *urb = NULL;
	char *urb_buf = NULL, *buf = NULL;
	unsigned int val;

	dev = (struct usb_skel *)file->private_data;

	if (count == 0)
		goto exit;

	buf = kzalloc(count + 1, GFP_KERNEL);
	if (!buf) {
		retval = -ENOMEM;
		goto error;
	}

	if (copy_from_user(buf, user_buffer, count)) {
		retval = -EFAULT;
		goto error;
	}

	retval = kstrtouint(buf, 0, &val);
	if (retval < 0)
		goto error;

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		goto error;
	}

	urb_buf = usb_alloc_coherent(dev->udev, sizeof(val), GFP_KERNEL, &urb->transfer_dma);
	if (!urb_buf) {
		retval = -ENOMEM;
		goto error;
	}

	memcpy(urb_buf, &val, sizeof(val));

	usb_fill_bulk_urb(urb, dev->udev,
			  usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
			  &val, sizeof(val), skel_write_bulk_callback, dev);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	retval = usb_submit_urb(urb, GFP_KERNEL);
	if (retval) {
		pr_err("%s - failed submitting write urb, error %d\n", __FUNCTION__, retval);
		goto error;
	}

	usb_free_urb(urb);
	kfree(buf);
exit:
	return count;

error:
	if (urb) {
		usb_free_coherent(dev->udev, count, urb_buf, urb->transfer_dma);
		usb_free_urb(urb);
	}
	if (buf)
		kfree(buf);
	return retval;
}

unsigned int skel_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct usb_skel *dev = filp->private_data;

	poll_wait(filp, &dev->wq, wait);
	if (atomic_dec_and_test(&dev->can_rd)) {
		pr_debug("Now fd can be read\n");
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

static struct file_operations skel_fops = {
	.owner =	THIS_MODULE,
	.read =		skel_read,
	.write =	skel_write,
	.poll =		skel_poll,
	.open =		skel_open,
	.release =	skel_release,
};

static struct usb_class_driver skel_class = {
	.name	= "usb/skel%d",
	.fops	= &skel_fops,
	.minor_base = USB_SKEL_MINOR_BASE,
};

static void usb_irq(struct urb *urb)
{
	struct usb_skel *dev = urb->context;
	int status;
	int *data = (int*)(dev->int_in_buffer);


	switch (urb->status) {
	case 0:			/* success */
		break;
	case -ECONNRESET:	/* unlink */
	case -ENOENT:
	case -ESHUTDOWN:
		return;
	/* -EPIPE:  should clear the halt */
	default:		/* error */
		goto resubmit;
	}

	if (data[0] > 0) {
		// Data available
		atomic_set(&dev->can_rd, 1);
		wake_up_interruptible(&dev->wq);
	}
		
	pr_debug("interrupt: %x:%x\n", data[0], data[1]);

resubmit:
	status = usb_submit_urb(urb, GFP_ATOMIC);
	if (status)
		dev_err(&dev->udev->dev,
			"can't resubmit intr, %s-%s/input0, status %d\n",
			dev->udev->bus->bus_name,
			dev->udev->devpath, status);
}

static int skel_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	int pipe, maxp;
	struct usb_skel *dev = NULL;
	struct usb_endpoint_descriptor *bulk_in, *bulk_out, *int_in;
	int retval = -ENOMEM;

	pr_debug("probe usb\n");

	dev = kzalloc(sizeof(struct usb_skel), GFP_KERNEL);
	if (!dev) {
		pr_err("Out of memory");
		goto error;
	}
	kref_init(&dev->kref);

	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	retval = usb_find_common_endpoints(interface->cur_altsetting,
			&bulk_in, &bulk_out, &int_in, NULL);
	if (retval) {
		dev_err(&interface->dev,
			"Could not find both bulk-in and bulk-out endpoints\n");
		goto error;
	}

	dev->bulk_in_size = usb_endpoint_maxp(bulk_in);
	dev->bulk_in_endpointAddr = bulk_in->bEndpointAddress;
	dev->bulk_in_buffer = kmalloc(dev->bulk_in_size, GFP_KERNEL);
	if (!dev->bulk_in_buffer) {
		retval = -ENOMEM;
		goto error;
	}
	dev->bulk_out_endpointAddr = bulk_out->bEndpointAddress;

	dev->int_in_size = usb_endpoint_maxp(int_in);
	dev->int_in_endpointAddr = int_in->bEndpointAddress;

	dev->irq_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->irq_urb) {
		pr_err("alloc irq urb failed\n");
		retval = -ENOMEM;
		goto error;
	}

	pipe = usb_rcvintpipe(dev->udev, dev->int_in_endpointAddr);
	maxp = usb_maxpacket(dev->udev, pipe, usb_pipeout(pipe));

	dev->int_in_buffer = usb_alloc_coherent(dev->udev, 8, GFP_ATOMIC, &dev->irq_urb->transfer_dma);
	if (!dev->int_in_buffer) {
		retval = -ENOMEM;
		pr_err("alloc coherent failed\n");
		goto error;
	}

	usb_fill_int_urb(dev->irq_urb, dev->udev, pipe, dev->int_in_buffer,
			 (maxp > 8 ? 8 : maxp),
			 usb_irq, dev, int_in->bInterval);
	dev->irq_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	pr_debug("int in size: %ld\n", dev->int_in_size);

	atomic_set(&dev->can_rd, 0);
	init_waitqueue_head(&dev->wq);
	usb_set_intfdata(interface, dev);

	retval = usb_register_dev(interface, &skel_class);
	if (retval) {
		/* something prevented us from registering this driver */
		pr_err("Not able to get a minor for this device.");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	dev_info(&interface->dev, "USB Skeleton device now attached to USBSkel-%d", interface->minor);
	return 0;

error:
	if (dev)
		kref_put(&dev->kref, skel_delete);
	return retval;
}

static void skel_disconnect(struct usb_interface *interface)
{
	struct usb_skel *dev;
	int minor = interface->minor;

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	usb_deregister_dev(interface, &skel_class);

	kref_put(&dev->kref, skel_delete);
	dev_info(&interface->dev, "USB Skeleton #%d now disconnected\n", minor);
}

static struct usb_driver skel_driver = {
	.name	= "usb_skeleton",
	.id_table = skel_table,
	.probe	= skel_probe,
	.disconnect = skel_disconnect,
};

static
int __init m_init(void)
{
	int result;
	result = usb_register(&skel_driver);
	if (result)
		pr_err("usb_register failed. Error number %d\n", result);

	return result;
}

static
void __exit m_exit(void)
{
	usb_deregister(&skel_driver);
}


module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("USB Driver skel");
