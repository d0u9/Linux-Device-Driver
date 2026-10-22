#include "qemu/osdep.h"
#include "hw/isa/isa.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "chardev/char-fe.h"
#include "qapi/error.h"
#include "sysemu/reset.h"
#include "hw/irq.h"

#define MMIO_BASE		0xFF000000
#define IO_BASE			0x200
#define PORT_WIDTH		0x10

#define TYPE_ISA_KMOD_EDU	"isa-kmod-edu"
#define ISA_KMOD_EDU(obj) \
	OBJECT_CHECK(ISAKmodEduState, (obj), TYPE_ISA_KMOD_EDU)

typedef struct ISAKmodEduState {
	ISADevice parent_obj;

	uint32_t iobase;
	uint32_t mmiobase;
	uint32_t isairq;
	bool irq_enabled;

	uint8_t data;

	CharBackend chr;
	qemu_irq irq;
	PortioList portio_list;
	MemoryRegion iomem;
} ISAKmodEduState;

static void isa_kmod_edu_reset(void *opaque)
{
}

static int isa_kmod_edu_can_receive(void *opaque)
{
	return 1;
}

static uint64_t isa_kmod_edu_mm_read(void *opaque, hwaddr addr, unsigned size)
{
	ISAKmodEduState *edu = opaque;
	uint8_t ret;

	switch (addr) {
	case 0: // DATA
		ret = edu->data;
		break;
	case 1: // CTRL
		ret = edu->irq_enabled;
		break;
	default:
		ret = 0;
	}

	return (uint32_t)ret;
}

static void isa_kmod_edu_mm_write(void *opaque, hwaddr addr,
				    uint64_t value, unsigned size)
{
	ISAKmodEduState *edu = opaque;
	uint8_t byte = (uint8_t)value;

	switch (addr) {
	case 0:  // DATA
		qemu_chr_fe_write_all(&edu->chr, &byte, 1);
		if (isgraph(byte))
			edu->data = byte;
		// send interrupt if user write a '\a' char.
		if (edu->irq_enabled && byte == '\n')
			qemu_irq_pulse(edu->irq);
		break;
	case 1:	 // CTRL
		if (value != 0) {
			edu->irq_enabled = true;
		} else {
			edu->irq_enabled = false;
		}
		break;
	default:
		break;
	}

}

static const MemoryRegionOps isa_kmod_edu_mm_ops = {
    .read = isa_kmod_edu_mm_read,
    .write = isa_kmod_edu_mm_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static uint32_t isa_kmod_edu_ioport_read(void *opaque, uint32_t addr)
{
	ISAKmodEduState *edu = opaque;
	return (uint64_t)isa_kmod_edu_mm_read(opaque, addr - edu->iobase, 1);
}

static void
isa_kmod_edu_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
	 ISAKmodEduState *edu = opaque;
	isa_kmod_edu_mm_write(opaque, addr - edu->iobase, val, 1);
}

static const MemoryRegionPortio isa_kmod_edu_portio_list[] = {
    { 0, PORT_WIDTH, 1,
      .read = isa_kmod_edu_ioport_read,
      .write = isa_kmod_edu_ioport_write },
    PORTIO_END_OF_LIST(),
};

static void isa_kmod_edu_realizefn(DeviceState *dev, Error **errp)
{
	static bool registered = false;
	ISADevice *isadev = ISA_DEVICE(dev);
	ISAKmodEduState *edu = ISA_KMOD_EDU(dev);
	MemoryRegion *isa_mm;

	if (registered) {
		error_setg(errp, "Only one kmod_edu device can be registered");
		return;
	}

	if (!qemu_chr_fe_backend_connected(&edu->chr)) {
		error_setg(errp, "Can't create kmod_edu device, empty char device");
		return;
	}
	
	if (edu->iobase == -1)
		edu->iobase = IO_BASE;

	if (edu->mmiobase == -1)
		edu->mmiobase = MMIO_BASE;

    edu->irq = isa_get_irq(isadev, edu->isairq);
	qemu_register_reset(isa_kmod_edu_reset, edu);

	isa_mm = isa_address_space(isadev);
	memory_region_init_io(&edu->iomem, NULL, &isa_kmod_edu_mm_ops, edu,
			      TYPE_ISA_KMOD_EDU, PORT_WIDTH);
	memory_region_add_subregion_overlap(isa_mm, edu->mmiobase, &edu->iomem, 3);

	qemu_chr_fe_set_handlers(&edu->chr, isa_kmod_edu_can_receive, NULL,
				 NULL, NULL, edu, NULL, true);
	isa_register_portio_list(isadev, &edu->portio_list, edu->iobase,
				 isa_kmod_edu_portio_list,
				 edu, TYPE_ISA_KMOD_EDU);
}

static Property isa_kmod_edu_properties[] = {
    DEFINE_PROP_UINT32("iobase", ISAKmodEduState, iobase,  -1),
    DEFINE_PROP_UINT32("mmiobase", ISAKmodEduState, mmiobase,  -1),
    DEFINE_PROP_UINT32("irq",   ISAKmodEduState, isairq,  6),
    DEFINE_PROP_CHR("chardev",  ISAKmodEduState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void isa_kmod_edu_class_initfn(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);

	dc->realize = isa_kmod_edu_realizefn;
	device_class_set_props(dc, isa_kmod_edu_properties);
	set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo isa_kmod_edu_info = {
	.name		= TYPE_ISA_KMOD_EDU,
	.parent		= TYPE_ISA_DEVICE,
	.instance_size	= sizeof(ISAKmodEduState),
	.class_init	= isa_kmod_edu_class_initfn,
};

static void isa_kmod_edu_register_types(void)
{
	type_register_static(&isa_kmod_edu_info);
}

type_init(isa_kmod_edu_register_types)
