#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/slab.h>

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
	kfree(dev->bulk_in_buffer);
	kfree(dev->int_in_buffer);
	kfree(dev);
}

static struct usb_driver skel_driver;

static int skel_open(struct inode *inode, struct file *file)
{
	struct usb_skel *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);
	interface = usb_find_interface(&skel_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d",
		       __FUNCTION__, subminor);
		retval = -ENODEV;
		goto exit;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		retval = -ENODEV;
		goto exit;
	}

	kref_get(&dev->kref);

	file->private_data = dev;

exit:
	return 0;
}

static int skel_release(struct inode *inode, struct file *file)
{
	struct usb_skel *dev;

	dev = (struct usb_skel *)file->private_data;
	if (dev == NULL)
		return -ENODEV;

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
	char *buf = NULL;

	dev = (struct usb_skel *)file->private_data;

	if (count == 0)
		goto exit;

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		goto error;
	}

	buf = usb_alloc_coherent(dev->udev, count, GFP_KERNEL, &urb->transfer_dma);
	if (!buf) {
		retval = -ENOMEM;
		goto error;
	}

	if (copy_from_user(buf, user_buffer, count)) {
		retval = -EFAULT;
		goto error;
	}

	pr_debug("-------1\n");

	usb_fill_bulk_urb(urb, dev->udev,
			  usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
			  buf, count, skel_write_bulk_callback, dev);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	pr_debug("-------2\n");

	retval = usb_submit_urb(urb, GFP_KERNEL);
	if (retval) {
		pr_debug("-------q\n");
		pr_err("%s - failed submitting write urb, error %d\n", __FUNCTION__, retval);
		goto error;
	}

	pr_debug("-------3\n");

	usb_free_urb(urb);

	pr_debug("-------4\n");

exit:
	return count;

error:
	if (urb) {
		usb_free_coherent(dev->udev, count, buf, urb->transfer_dma);
		usb_free_urb(urb);
	}
	return retval;
}

unsigned int skel_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

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

static int skel_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
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
	dev->int_in_buffer = kmalloc(dev->int_in_size, GFP_KERNEL);
	if (!dev->int_in_buffer) {
		kfree(dev->bulk_in_buffer);
		retval = -ENOMEM;
		goto error;
	}

	init_waitqueue_head(&dev->wq);

	pr_debug("int in size: %ld\n", dev->int_in_size);

#if 0
	int i;
	size_t buffer_size;
	struct usb_host_interface *iface_desc = NULL;
	struct usb_endpoint_descriptor *endpoint = NULL;


	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		pr_debug("xxxx\n");
		endpoint = &iface_desc->endpoint[i].desc;
		pr_debug("==== %x\n", endpoint->bEndpointAddress);

		if (!dev->bulk_in_buffer &&
		    (endpoint->bEndpointAddress & USB_DIR_IN) &&
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
					== USB_ENDPOINT_XFER_BULK)) {
			/* we found a bulk in endpoint */
			buffer_size = endpoint->wMaxPacketSize;
			dev->bulk_in_size = buffer_size;
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			pr_debug("in %x\n", endpoint->bEndpointAddress);
			dev->bulk_in_buffer = kzalloc(buffer_size, GFP_KERNEL);
			if (!dev->bulk_in_buffer) {
				pr_err("Could not alloc bulk_in_buffer\n");
				goto error;
			}
		}

		if (!dev->bulk_out_endpointAddr &&
		    !(endpoint->bEndpointAddress & USB_DIR_IN) && 
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
					== USB_ENDPOINT_XFER_BULK)) {
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
			pr_debug("out %x\n", endpoint->bEndpointAddress);
		}
	}

	if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
		pr_err("Could not find both bulk-in and bulk-out endpoints\n");
		goto error;
	}
#endif

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
