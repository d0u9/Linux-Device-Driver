#include <time.h>
#include <stdlib.h>

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/usb.h"
#include "desc.h"

#define TYPE_USB_KMOD_EDU	"usb-kmod-edu"
#define USB_KMOD_EDU(obj) \
	OBJECT_CHECK(USBKmodEduState, (obj), TYPE_USB_KMOD_EDU)

#define USB_MANUFACTURER	0x0001
#define USB_PRODUCT		0x0002
#define USB_SERIALNUMBER	0x0003
#define CAPACITY		0x100000	// 1M bytes

#define BULK_PACKET_SIZE	64
#define INT_PACKET_SIZE		8

typedef struct USBKmodEduState {
	USBDevice parent_obj;

	int interval;		// millisecond
} USBKmodEduState;

enum {
    STR_MANUFACTURER = 1,
    STR_PRODUCT,
    STR_SERIALNUMBER,
};

static const USBDescStrings desc_strings = {
    [STR_MANUFACTURER]     = "QEMU",
    [STR_PRODUCT]          = "Kernel Module Deucational Device",
    [STR_SERIALNUMBER]     = "1",
};


static const USBDescIface desc_iface_kmod_edu = {
    .bInterfaceNumber              = 0,
    .bNumEndpoints                 = 3,
    .bInterfaceClass               = USB_CLASS_VENDOR_SPEC, //USB_CLASS_HID,
    .bInterfaceSubClass            = 0xff, /* boot */
    .bInterfaceProtocol            = 0xff,
    .eps = (USBDescEndpoint[]) {
        {
            .bEndpointAddress      = USB_DIR_IN | 0x01,
            .bmAttributes          = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize        = BULK_PACKET_SIZE,
        },
        {
            .bEndpointAddress      = USB_DIR_OUT | 0x02,
            .bmAttributes          = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize        = BULK_PACKET_SIZE,
        },
        {
            .bEndpointAddress      = USB_DIR_IN | 0x03,
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = INT_PACKET_SIZE,
            .bInterval             = 0x0a,
        },
    },
};

static const USBDescDevice desc_device_kmod_edu = {
    .bcdUSB                        = 0x0110,
    .bMaxPacketSize0               = 8,
    .bNumConfigurations            = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces        = 1,
            .bConfigurationValue   = 1,
            .bmAttributes          = USB_CFG_ATT_ONE,
            .bMaxPower             = 50,
            .nif = 1,
            .ifs = &desc_iface_kmod_edu,
        },
    },
};

static const USBDesc desc_kmod_edu = {
    .id = {
        .idVendor          = 0x1234,
        .idProduct         = 0x7863,
        .bcdDevice         = 0x4210,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device_kmod_edu,
    // .high = &desc_device_kmod_edu,
    // .super = &desc_device_kmod_edu,
    .str  = desc_strings,
};

static void usb_kmod_edu_unrealize(USBDevice *dev)
{
	USBKmodEduState *us = USB_KMOD_EDU(dev);
	us->interval = 500;
}

static void usb_kmod_edu_realize(USBDevice *dev, Error **errp)
{
	usb_desc_create_serial(dev);
	usb_desc_init(dev);
}

static void usb_kmod_edu_handle_reset(USBDevice *dev)
{
}

static void usb_kmod_edu_handle_control(USBDevice *dev, USBPacket *p,
               int request, int value, int index, int length, uint8_t *data)
{
	int ret;

	ret = usb_desc_handle_control(dev, p, request, value,
				      index, length, data);
	if (ret >= 0) {
		return;
	}

	return;
}

static void usb_kmod_edu_handle_data(USBDevice *dev, USBPacket *p)
{
	static time_t t;
	static uint32_t intv = 1;
	time_t cur_t;
	int i;
	struct tm *tm;
	struct iovec *iov;
	uint32_t int_data[2] = { 0, 0 };	// int_data[0]: flag, int_data[1]: intv
	char buf[64] = { 0 };

	switch (p->pid) {
	case USB_TOKEN_OUT:
		for (i = 0; i < p->iov.niov; i++) {
			iov = p->iov.iov + i;
			intv = *((uint32_t*)(iov->iov_base));
		}
		p->actual_length = p->iov.size;
		break;
	case USB_TOKEN_IN:
		if (p->ep->nr == 1) {
			// bulk in ep
			tm = localtime(&t);
			i = strftime(buf, 64, "%F %T", tm);
			i = MIN(i + 1, p->iov.size);
			usb_packet_copy(p, buf, i);
		} else if (p->ep->nr == 3) {
			// interrupt in ep
			cur_t = time(NULL);
			if (cur_t >= t + intv) {
				t = cur_t;
				int_data[0] = 1;
			} else {
				int_data[0] = 0;
			}

			int_data[1] = intv;

			usb_packet_copy(p, &int_data, sizeof(int_data));
		}
		break;

	default:
		p->status = USB_RET_STALL;
		break;
    }
}

static void usb_kmod_edu_class_initfn(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);
	USBDeviceClass *k = USB_DEVICE_CLASS(klass);

	k->product_desc	  = "QEMU virtual devcie for kernel module study";
	k->usb_desc       = &desc_kmod_edu;
	k->realize        = usb_kmod_edu_realize;
	k->handle_reset	  = usb_kmod_edu_handle_reset;
	k->handle_control = usb_kmod_edu_handle_control;
	k->handle_data    = usb_kmod_edu_handle_data;
	k->unrealize      = usb_kmod_edu_unrealize;
	set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo usb_kmod_edu_info = {
	.name          = TYPE_USB_KMOD_EDU,
	.parent        = TYPE_USB_DEVICE,
	.instance_size = sizeof(USBKmodEduState),
	.class_init    = usb_kmod_edu_class_initfn,
};

static void usb_kmod_edu_register_types(void)
{
	type_register_static(&usb_kmod_edu_info);
	usb_legacy_register(TYPE_USB_KMOD_EDU, "kmod_edu", NULL);
}

type_init(usb_kmod_edu_register_types)
