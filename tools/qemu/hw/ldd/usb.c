#include "qemu/osdep.h"

#include "qemu/bswap.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qom/object.h"
#include "hw/core/qdev-properties.h"
#include "hw/usb/usb.h"
#include "migration/vmstate.h"
#include "hw/usb/desc.h"

/*
 * ldd-usb: a USB device that ticks every few seconds. A bulk OUT endpoint
 * sets the interval, an interrupt IN endpoint reports each tick and a bulk
 * IN endpoint returns the time of the last one. See tools/qemu/ldd-usb.md.
 */

#define LDD_USB_VENDOR_ID       0x1234
#define LDD_USB_PRODUCT_ID      0x7863

/* Endpoint numbers */
#define LDD_USB_EP_TIME         1   /* bulk IN: time of the last tick */
#define LDD_USB_EP_INTERVAL     2   /* bulk OUT: new interval */
#define LDD_USB_EP_TICK         3   /* interrupt IN: one report per tick */

#define LDD_USB_BULK_PACKET     64
#define LDD_USB_INT_PACKET      8
#define LDD_USB_INTERVAL_MAX    3600    /* seconds */
#define LDD_USB_TIME_LEN        20      /* "YYYY-MM-DD HH:MM:SS" and a NUL */

#define TYPE_LDD_USB "ldd-usb"
OBJECT_DECLARE_SIMPLE_TYPE(LDDUSBState, LDD_USB)

struct LDDUSBState {
    USBDevice parent_obj;

    USBEndpoint *tick_ep;
    QEMUTimer *timer;

    uint32_t interval;          /* seconds, set by the driver */
    bool tick_pending;          /* a tick not yet reported on the tick EP */
    char time[LDD_USB_TIME_LEN];

    /* Properties */
    uint32_t default_interval;
};

enum {
    STR_MANUFACTURER = 1,
    STR_PRODUCT,
    STR_SERIALNUMBER,
};

static const USBDescStrings desc_strings = {
    [STR_MANUFACTURER] = "QEMU",
    [STR_PRODUCT]      = "LDD USB ticker",
    [STR_SERIALNUMBER] = "1",
};

static const USBDescIface desc_iface = {
    .bInterfaceNumber   = 0,
    .bNumEndpoints      = 3,
    .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
    .bInterfaceSubClass = 0xff,
    .bInterfaceProtocol = 0xff,
    .eps = (USBDescEndpoint[]) {
        {
            .bEndpointAddress = USB_DIR_IN | LDD_USB_EP_TIME,
            .bmAttributes     = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize   = LDD_USB_BULK_PACKET,
        },
        {
            .bEndpointAddress = USB_DIR_OUT | LDD_USB_EP_INTERVAL,
            .bmAttributes     = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize   = LDD_USB_BULK_PACKET,
        },
        {
            .bEndpointAddress = USB_DIR_IN | LDD_USB_EP_TICK,
            .bmAttributes     = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize   = LDD_USB_INT_PACKET,
            .bInterval        = 10,     /* ms at full speed */
        },
    },
};

static const USBDescDevice desc_device = {
    .bcdUSB             = 0x0110,
    .bMaxPacketSize0    = 8,
    .bNumConfigurations = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces      = 1,
            .bConfigurationValue = 1,
            .bmAttributes        = USB_CFG_ATT_ONE,
            .bMaxPower           = 50,
            .nif                 = 1,
            .ifs                 = &desc_iface,
        },
    },
};

static const USBDesc desc_ldd_usb = {
    .id = {
        .idVendor      = LDD_USB_VENDOR_ID,
        .idProduct     = LDD_USB_PRODUCT_ID,
        .bcdDevice     = 0x0100,
        .iManufacturer = STR_MANUFACTURER,
        .iProduct      = STR_PRODUCT,
        .iSerialNumber = STR_SERIALNUMBER,
    },
    .full = &desc_device,
    .str  = desc_strings,
};

/* Remember the host's local time, returned on the time EP. */
static void ldd_usb_stamp_time(LDDUSBState *s)
{
    time_t now = time(NULL);
    struct tm tm;

    localtime_r(&now, &tm);
    strftime(s->time, sizeof(s->time), "%Y-%m-%d %H:%M:%S", &tm);
}

static void ldd_usb_arm_timer(LDDUSBState *s)
{
    timer_mod(s->timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) +
                        (int64_t)s->interval * 1000);
}

static void ldd_usb_tick(void *opaque)
{
    LDDUSBState *s = opaque;

    ldd_usb_stamp_time(s);
    s->tick_pending = true;
    ldd_usb_arm_timer(s);

    /* The host controller retries a NAKed interrupt transfer on wakeup. */
    usb_wakeup(s->tick_ep, 0);
}

static void ldd_usb_handle_reset(USBDevice *dev)
{
    LDDUSBState *s = LDD_USB(dev);

    s->interval = s->default_interval;
    s->tick_pending = false;
    ldd_usb_stamp_time(s);
    ldd_usb_arm_timer(s);
}

static void ldd_usb_handle_control(USBDevice *dev, USBPacket *p, int request,
                                   int value, int index, int length,
                                   uint8_t *data)
{
    /* Only the standard requests, answered from the descriptors. */
    if (usb_desc_handle_control(dev, p, request, value, index, length,
                                data) < 0) {
        p->status = USB_RET_STALL;
    }
}

/* Bulk OUT: a 32-bit little-endian interval in seconds. */
static void ldd_usb_set_interval(LDDUSBState *s, USBPacket *p)
{
    uint32_t le;

    if (p->iov.size != sizeof(le)) {
        p->status = USB_RET_STALL;
        return;
    }

    usb_packet_copy(p, &le, sizeof(le));
    s->interval = MIN(MAX(le32_to_cpu(le), 1), LDD_USB_INTERVAL_MAX);
    s->tick_pending = false;
    ldd_usb_arm_timer(s);
}

/* Interrupt IN: { le32 1, le32 interval } once per tick, NAK in between. */
static void ldd_usb_report_tick(LDDUSBState *s, USBPacket *p)
{
    uint32_t report[2];

    if (!s->tick_pending) {
        p->status = USB_RET_NAK;
        return;
    }

    report[0] = cpu_to_le32(1);
    report[1] = cpu_to_le32(s->interval);
    usb_packet_copy(p, report, MIN(sizeof(report), p->iov.size));
    s->tick_pending = false;
}

static void ldd_usb_handle_data(USBDevice *dev, USBPacket *p)
{
    LDDUSBState *s = LDD_USB(dev);

    switch (p->pid) {
    case USB_TOKEN_OUT:
        if (p->ep->nr == LDD_USB_EP_INTERVAL) {
            ldd_usb_set_interval(s, p);
            return;
        }
        break;
    case USB_TOKEN_IN:
        if (p->ep->nr == LDD_USB_EP_TIME) {
            usb_packet_copy(p, s->time, MIN(sizeof(s->time), p->iov.size));
            return;
        }
        if (p->ep->nr == LDD_USB_EP_TICK) {
            ldd_usb_report_tick(s, p);
            return;
        }
        break;
    }

    p->status = USB_RET_STALL;
}

static void ldd_usb_realize(USBDevice *dev, Error **errp)
{
    LDDUSBState *s = LDD_USB(dev);

    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->tick_ep = usb_ep_get(dev, USB_TOKEN_IN, LDD_USB_EP_TICK);
    s->timer = timer_new_ms(QEMU_CLOCK_VIRTUAL, ldd_usb_tick, s);
}

static void ldd_usb_unrealize(USBDevice *dev)
{
    LDDUSBState *s = LDD_USB(dev);

    g_clear_pointer(&s->timer, timer_free);
}

static const Property ldd_usb_properties[] = {
    DEFINE_PROP_UINT32("interval", LDDUSBState, default_interval, 1),
};

static const VMStateDescription vmstate_ldd_usb = {
    .name = TYPE_LDD_USB,
    .unmigratable = 1,
};

static void ldd_usb_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "LDD USB ticker";
    uc->usb_desc       = &desc_ldd_usb;
    uc->realize        = ldd_usb_realize;
    uc->unrealize      = ldd_usb_unrealize;
    uc->handle_reset   = ldd_usb_handle_reset;
    uc->handle_control = ldd_usb_handle_control;
    uc->handle_data    = ldd_usb_handle_data;
    dc->desc = "LDD USB ticker for driver examples";
    dc->vmsd = &vmstate_ldd_usb;
    device_class_set_props(dc, ldd_usb_properties);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo ldd_usb_info = {
    .name          = TYPE_LDD_USB,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(LDDUSBState),
    .class_init    = ldd_usb_class_init,
};

static void ldd_usb_register_types(void)
{
    type_register_static(&ldd_usb_info);
}

type_init(ldd_usb_register_types)
