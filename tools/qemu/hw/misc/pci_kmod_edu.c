#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/pci/pci.h"
#include "hw/pci/pcie.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "qemu/units.h"
#include "hw/hw.h"
#if 0
#include "hw/pci/msi.h"
#include "qemu/timer.h"
#include "qemu/main-loop.h" /* iothread mutex */
#include "qapi/visitor.h"
#endif

#define KMOD_EDU_MSIX_VEC_NUM	        (4)
#define KMOD_EDU_MSIX_IDX	            (1)
#define KMOD_EDU_MSIX_TABLE	            (0x3000)
#define KMOD_EDU_MSIX_PBA	            (0x3800)
#define KMOD_EDU_MSIX_SIZE              (64 * KiB)

#define TYPE_KMOD_EDU		            "pci-kmod-edu"
#define KMOD_EDU(obj) \
    OBJECT_CHECK(PCIKmodEduState, (obj), TYPE_KMOD_EDU)

typedef struct PCIKmodEduState {
    PCIDevice parent_obj;

    uint32_t irq_status;
    MemoryRegion mmio;

    bool use_msix;
    MemoryRegion msix;
} PCIKmodEduState;

static bool kmod_edu_msix_enabled(PCIKmodEduState *edu)
{
    return msix_enabled(PCI_DEVICE(edu));
}

static void kmod_edu_raise_irq(PCIKmodEduState *edu, uint32_t val)
{
    static int last = 0;
    last = (last + 1) % KMOD_EDU_MSIX_VEC_NUM;
    if (kmod_edu_msix_enabled(edu)) {
        msix_notify(PCI_DEVICE(edu), last);
    }
}

static void kmod_edu_lower_irq(PCIKmodEduState *edu, uint32_t val)
{

}

static uint64_t kmod_edu_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}

static void kmod_edu_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned size)
{
    PCIKmodEduState *edu = opaque;
    switch (addr) {
    case 0x60:          // Trigger interrupt
        kmod_edu_raise_irq(edu, 1);
        kmod_edu_lower_irq(edu, 1);
        break;
    }
}

static const MemoryRegionOps kmod_edu_mmio_ops = {
    .read = kmod_edu_mmio_read,
    .write = kmod_edu_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 8,
    },
};

static void kmod_edu_unuse_msix_vectors(PCIKmodEduState *edu, int num_vectors)
{
    int i;
    for (i = 0; i < num_vectors; i++) {
        msix_vector_unuse(PCI_DEVICE(edu), i);
    }
}

static bool kmod_edu_use_msix_vectors(PCIKmodEduState *edu, int num_vectors)
{
    int i, rv;
    for (i = 0; i < num_vectors; i++) {
        rv = msix_vector_use(PCI_DEVICE(edu), i);
        if (rv < 0) {
            kmod_edu_unuse_msix_vectors(edu, i);
            return false;
        }
    }
    return true;
}

static void kmod_edu_realize(PCIDevice *pdev, Error **errp)
{
    int rv;
    PCIKmodEduState *edu = KMOD_EDU(pdev);
    uint8_t *pci_conf = pdev->config;
    Error *local_err = NULL;

    pci_config_set_interrupt_pin(pci_conf, 1);

    memory_region_init_io(&edu->mmio, OBJECT(edu), &kmod_edu_mmio_ops,
                          edu, "edu-mmio", 8 *MiB);
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &edu->mmio);

    memory_region_init(&edu->msix, OBJECT(edu), "kmod-edu-msix",
                       KMOD_EDU_MSIX_SIZE);
    pci_register_bar(pdev, KMOD_EDU_MSIX_IDX,
                     PCI_BASE_ADDRESS_SPACE_MEMORY, &edu->msix);

    rv = msix_init(pdev, KMOD_EDU_MSIX_VEC_NUM,
                   &edu->msix, KMOD_EDU_MSIX_IDX, KMOD_EDU_MSIX_TABLE,
                   &edu->msix, KMOD_EDU_MSIX_IDX, KMOD_EDU_MSIX_PBA,
                   0, errp);
    if (rv == 0) {
        if (!kmod_edu_use_msix_vectors(edu, KMOD_EDU_MSIX_VEC_NUM)) {
            msix_uninit(PCI_DEVICE(edu), &edu->mmio, &edu->mmio);
        }
    }

    if (pcie_endpoint_cap_init(pdev, 0x0E0) < 0) {
        hw_error("Failed to initialize PCIe capability");
    }

    pci_add_capability(pdev, PCI_CAP_ID_PM, 0x0C8, PCI_PM_SIZEOF, &local_err);


}

static void kmod_edu_uninit(PCIDevice *pdev)
{
    PCIKmodEduState *edu = KMOD_EDU(pdev);

    pcie_cap_exit(pdev);

    if (msix_present(PCI_DEVICE(edu))) {
        kmod_edu_unuse_msix_vectors(edu, KMOD_EDU_MSIX_VEC_NUM);
        msix_uninit(pdev, &edu->mmio, &edu->mmio);
    }
}

static void kmod_edu_instance_init(Object *obj)
{
    // PCIKmodEduState *edu = KMOD_EDU(obj);
}

static void kmod_edu_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = kmod_edu_realize;
    k->exit = kmod_edu_uninit;
    k->vendor_id = PCI_VENDOR_ID_QEMU;
    k->device_id = 0x7863;
    k->revision = 0x10;
    k->class_id = PCI_CLASS_OTHERS;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static InterfaceInfo interfaces[] = {
    // { INTERFACE_CONVENTIONAL_PCI_DEVICE },
    { INTERFACE_PCIE_DEVICE },
    { },
};

static const TypeInfo kmod_edu_info = {
    .name		= TYPE_KMOD_EDU,
    .parent		= TYPE_PCI_DEVICE,
    .instance_size	= sizeof(PCIKmodEduState),
    .instance_init	= kmod_edu_instance_init,
    .class_init	= kmod_edu_class_initfn,
    .interfaces = interfaces,
};

static void kmod_edu_register_types(void)
{
    type_register_static(&kmod_edu_info);
}

type_init(kmod_edu_register_types)
