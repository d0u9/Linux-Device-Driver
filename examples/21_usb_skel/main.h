#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"usb_skel"
#define SHORT_NR_PORTS		1

struct usb_skel {
	struct usb_device	*udev;
	struct usb_interface	*interface;
	size_t			bulk_in_size;
	__u8			bulk_in_endpointAddr;
	__u8			bulk_out_endpointAddr;
	unsigned char		*bulk_in_buffer;
	size_t			int_in_size;
	__u8			int_in_endpointAddr;
	unsigned char		*int_in_buffer;
	struct urb		*irq_urb;

	atomic_t can_rd;

	struct kref		kref;
	wait_queue_head_t	wq;
};

#endif
