#include "qemu/osdep.h"

#include "qapi/error.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "qemu/units.h"
#include "qom/object.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev-properties-system.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pcie.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "migration/vmstate.h"

/* BAR0 registers, see ldd-pcie.md */
#define LDD_REG_ID              0x00
#define LDD_REG_VERSION         0x04
#define LDD_REG_CAPS            0x08
#define LDD_REG_SCRATCH         0x0c
#define LDD_REG_CTRL            0x10
#define LDD_REG_STATUS          0x14
#define LDD_REG_INT_STATUS      0x18
#define LDD_REG_INT_MASK        0x1c
#define LDD_REG_INT_RAISE       0x20
#define LDD_REG_MAX_SEG         0x24
#define LDD_REG_MAX_DESC        0x28
#define LDD_REG_DESC_ADDR_LO    0x40
#define LDD_REG_DESC_ADDR_HI    0x44
#define LDD_REG_DESC_COUNT      0x48
#define LDD_REG_DEV_OFFSET      0x4c
#define LDD_REG_DMA_CTRL        0x50
#define LDD_REG_XFER_BYTES      0x54
#define LDD_REG_ERR_DESC        0x58
#define LDD_REG_XOR             0x5c

#define LDD_ID                  0x4c444450      /* "LDDP" */
#define LDD_VERSION             0x00010000      /* 1.0 */

#define LDD_CAPS_DMA64          BIT(0)

#define LDD_CTRL_RESET          BIT(0)

#define LDD_STATUS_BUSY         BIT(0)
#define LDD_STATUS_DONE         BIT(1)
#define LDD_STATUS_ERROR        BIT(2)
#define LDD_STATUS_ERR_CODE_SHIFT 8

/* STATUS.ERR_CODE */
#define LDD_ERR_NO_MASTER       1
#define LDD_ERR_BAD_COUNT       2
#define LDD_ERR_BAD_DESC        3
#define LDD_ERR_OUT_OF_RANGE    4
#define LDD_ERR_DMA_FAULT       5

/* INT_STATUS, INT_MASK and INT_RAISE; bit n uses MSI-X vector n */
#define LDD_INT_DMA_DONE        BIT(0)
#define LDD_INT_DMA_ERROR       BIT(1)
#define LDD_INT_SOFT            BIT(2)
#define LDD_INT_ALL             (LDD_INT_DMA_DONE | LDD_INT_DMA_ERROR | \
                                 LDD_INT_SOFT)
#define LDD_MSIX_VECTORS        3

#define LDD_DMA_CTRL_DIR        BIT(0)
#define LDD_DMA_CTRL_START      BIT(1)

#define LDD_DESC_ADDR_ALIGN     16

#define LDD_MEM_SIZE            (1 * MiB)

/* one entry of the descriptor table, little endian in guest memory */
typedef struct LDDPCIEDesc {
    uint64_t addr;
    uint32_t len;
    uint32_t flags;
} QEMU_PACKED LDDPCIEDesc;

#define TYPE_LDD_PCIE "ldd-pcie"
OBJECT_DECLARE_SIMPLE_TYPE(LDDPCIEState, LDD_PCIE)

struct LDDPCIEState {
    PCIDevice parent_obj;
    MemoryRegion bar0;
    MemoryRegion bar2;
    QEMUTimer *timer;           /* completes the transfer after latency-us */

    /* properties: set from the command line, kept across reset */
    bool dma64;
    uint32_t max_seg;
    uint32_t max_desc;
    uint32_t latency_us;

    /* register state: cleared by reset */
    struct {
        uint32_t scratch;
        uint32_t status;
        uint32_t int_status;
        uint32_t int_mask;
        uint64_t desc_addr;
        uint32_t desc_count;
        uint32_t dev_offset;
        uint32_t dma_ctrl;      /* only DIR is kept; START reads as 0 */
        uint32_t xfer_bytes;
        uint32_t err_desc;
        uint32_t xor;
        uint32_t irq_sent;      /* pending bits already signalled */
        uint32_t result;        /* ERR_CODE reported when the timer fires */
    } regs;
};

/*
 * Signal what INT_STATUS & INT_MASK holds. MSI-X and MSI are edge
 * triggered: a message goes out for each bit that has become pending
 * since the last call. INTx is level triggered: the line follows
 * whether anything is pending.
 */
static void ldd_pcie_update_irq(LDDPCIEState *ps)
{
    PCIDevice *pdev = PCI_DEVICE(ps);
    uint32_t pending = ps->regs.int_status & ps->regs.int_mask;
    uint32_t new = pending & ~ps->regs.irq_sent;

    if (msix_enabled(pdev)) {
        pci_irq_deassert(pdev);
        for (int i = 0; i < LDD_MSIX_VECTORS; i++) {
            if (new & BIT(i))
                msix_notify(pdev, i);
        }
    } else if (msi_enabled(pdev)) {
        pci_irq_deassert(pdev);
        if (new)
            msi_notify(pdev, 0);
    } else {
        pci_set_irq(pdev, pending != 0);
    }
    ps->regs.irq_sent = pending;
}

static void ldd_pcie_reset(LDDPCIEState *ps)
{
    timer_del(ps->timer);
    memset(&ps->regs, 0, sizeof(ps->regs));
    pci_irq_deassert(PCI_DEVICE(ps));
}

/* the highest address the engine can reach is addr + len - 1 */
static bool ldd_pcie_dma_ok(LDDPCIEState *ps, uint64_t addr, uint64_t len)
{
    return ps->dma64 || addr + len <= 4 * GiB;
}

/*
 * Run the transfer described by the registers and return its ERR_CODE,
 * 0 on success. Every check is done before any data is moved, except
 * DMA faults, which can only be seen while copying.
 */
static uint32_t ldd_pcie_transfer(LDDPCIEState *ps)
{
    PCIDevice *pdev = PCI_DEVICE(ps);
    uint8_t *mem = memory_region_get_ram_ptr(&ps->bar2);
    bool to_guest = ps->regs.dma_ctrl & LDD_DMA_CTRL_DIR;
    uint32_t count = ps->regs.desc_count;
    g_autofree LDDPCIEDesc *desc = NULL;
    uint64_t total = 0;
    uint32_t off;

    if (!(pci_get_word(pdev->config + PCI_COMMAND) & PCI_COMMAND_MASTER))
        return LDD_ERR_NO_MASTER;

    if (count == 0 || count > ps->max_desc)
        return LDD_ERR_BAD_COUNT;

    desc = g_new(LDDPCIEDesc, count);
    if (!ldd_pcie_dma_ok(ps, ps->regs.desc_addr, count * sizeof(*desc)) ||
        pci_dma_read(pdev, ps->regs.desc_addr, desc,
                     count * sizeof(*desc)) != MEMTX_OK)
        return LDD_ERR_DMA_FAULT;       /* ERR_DESC stays 0 */

    for (uint32_t i = 0; i < count; i++) {
        desc[i].addr = le64_to_cpu(desc[i].addr);
        desc[i].len = le32_to_cpu(desc[i].len);
        desc[i].flags = le32_to_cpu(desc[i].flags);

        if (desc[i].len == 0 || desc[i].len > ps->max_seg ||
            desc[i].flags != 0) {
            ps->regs.err_desc = i;
            return LDD_ERR_BAD_DESC;
        }
        total += desc[i].len;
    }

    if (ps->regs.dev_offset > LDD_MEM_SIZE ||
        total > LDD_MEM_SIZE - ps->regs.dev_offset)
        return LDD_ERR_OUT_OF_RANGE;

    off = ps->regs.dev_offset;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t addr = desc[i].addr;
        uint32_t len = desc[i].len;
        MemTxResult res;

        if (!ldd_pcie_dma_ok(ps, addr, len)) {
            res = MEMTX_DECODE_ERROR;
        } else if (to_guest && ps->regs.xor) {
            g_autofree uint8_t *buf = g_malloc(len);

            for (uint32_t j = 0; j < len; j++)
                buf[j] = mem[off + j] ^ ps->regs.xor;
            res = pci_dma_write(pdev, addr, buf, len);
        } else if (to_guest) {
            res = pci_dma_write(pdev, addr, mem + off, len);
        } else {
            res = pci_dma_read(pdev, addr, mem + off, len);
        }

        if (res != MEMTX_OK) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "ldd-pcie: DMA fault at descriptor %u, address 0x%"
                          PRIx64 "\n", i, addr);
            ps->regs.err_desc = i;
            return LDD_ERR_DMA_FAULT;
        }
        off += len;
        ps->regs.xfer_bytes += len;
    }

    /* the guest may have mapped BAR2 and read it through KVM */
    memory_region_set_dirty(&ps->bar2, ps->regs.dev_offset,
                            off - ps->regs.dev_offset);
    return 0;
}

static void ldd_pcie_start(LDDPCIEState *ps)
{
    ps->regs.status = LDD_STATUS_BUSY;
    ps->regs.xfer_bytes = 0;
    ps->regs.err_desc = 0;

    /*
     * The data is moved now; only the completion is delayed, so that the
     * driver really has to wait for the interrupt or poll BUSY.
     */
    ps->regs.result = ldd_pcie_transfer(ps);
    if (ps->regs.result)
        qemu_log_mask(LOG_GUEST_ERROR, "ldd-pcie: transfer failed, ERR_CODE %u\n",
                      ps->regs.result);

    timer_mod(ps->timer, qemu_clock_get_us(QEMU_CLOCK_VIRTUAL) +
                         ps->latency_us);
}

static void ldd_pcie_complete(void *opaque)
{
    LDDPCIEState *ps = opaque;

    if (ps->regs.result) {
        ps->regs.status = LDD_STATUS_ERROR |
                          ps->regs.result << LDD_STATUS_ERR_CODE_SHIFT;
        ps->regs.int_status |= LDD_INT_DMA_ERROR;
    } else {
        ps->regs.status = LDD_STATUS_DONE;
        ps->regs.int_status |= LDD_INT_DMA_DONE;
    }
    ldd_pcie_update_irq(ps);
}

static uint64_t ldd_pcie_bar0_read(void *opaque, hwaddr addr, unsigned size)
{
    LDDPCIEState *ps = opaque;

    switch (addr) {
    case LDD_REG_ID:
        return LDD_ID;
    case LDD_REG_VERSION:
        return LDD_VERSION;
    case LDD_REG_CAPS:
        return ps->dma64 ? LDD_CAPS_DMA64 : 0;
    case LDD_REG_SCRATCH:
        return ps->regs.scratch;
    case LDD_REG_CTRL:              /* write only */
    case LDD_REG_INT_RAISE:
        return 0;
    case LDD_REG_STATUS:
        return ps->regs.status;
    case LDD_REG_INT_STATUS:
        return ps->regs.int_status;
    case LDD_REG_INT_MASK:
        return ps->regs.int_mask;
    case LDD_REG_MAX_SEG:
        return ps->max_seg;
    case LDD_REG_MAX_DESC:
        return ps->max_desc;
    case LDD_REG_DESC_ADDR_LO:
        return extract64(ps->regs.desc_addr, 0, 32);
    case LDD_REG_DESC_ADDR_HI:
        return extract64(ps->regs.desc_addr, 32, 32);
    case LDD_REG_DESC_COUNT:
        return ps->regs.desc_count;
    case LDD_REG_DEV_OFFSET:
        return ps->regs.dev_offset;
    case LDD_REG_DMA_CTRL:
        return ps->regs.dma_ctrl & LDD_DMA_CTRL_DIR;
    case LDD_REG_XFER_BYTES:
        return ps->regs.xfer_bytes;
    case LDD_REG_ERR_DESC:
        return ps->regs.err_desc;
    case LDD_REG_XOR:
        return ps->regs.xor;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "ldd-pcie: read of reserved offset 0x%" HWADDR_PRIx "\n",
                      addr);
        return 0;
    }
}

static void ldd_pcie_bar0_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned size)
{
    LDDPCIEState *ps = opaque;
    bool busy = ps->regs.status & LDD_STATUS_BUSY;

    switch (addr) {
    case LDD_REG_SCRATCH:
        ps->regs.scratch = val;
        break;
    case LDD_REG_CTRL:
        if (val & LDD_CTRL_RESET)
            ldd_pcie_reset(ps);
        break;
    case LDD_REG_INT_STATUS:        /* W1C */
        ps->regs.int_status &= ~val;
        ldd_pcie_update_irq(ps);
        break;
    case LDD_REG_INT_MASK:
        ps->regs.int_mask = val & LDD_INT_ALL;
        ldd_pcie_update_irq(ps);
        break;
    case LDD_REG_INT_RAISE:
        ps->regs.int_status |= val & LDD_INT_ALL;
        ldd_pcie_update_irq(ps);
        break;
    case LDD_REG_DESC_ADDR_LO:
    case LDD_REG_DESC_ADDR_HI:
    case LDD_REG_DESC_COUNT:
    case LDD_REG_DEV_OFFSET:
        if (busy) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "ldd-pcie: write to 0x%" HWADDR_PRIx
                          " while busy, ignored\n", addr);
            break;
        }
        if (addr == LDD_REG_DESC_ADDR_LO)
            ps->regs.desc_addr = deposit64(ps->regs.desc_addr, 0, 32,
                                           val & ~(LDD_DESC_ADDR_ALIGN - 1));
        else if (addr == LDD_REG_DESC_ADDR_HI)
            ps->regs.desc_addr = deposit64(ps->regs.desc_addr, 32, 32, val);
        else if (addr == LDD_REG_DESC_COUNT)
            ps->regs.desc_count = val;
        else
            ps->regs.dev_offset = val;
        break;
    case LDD_REG_DMA_CTRL:
        if (busy) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "ldd-pcie: DMA_CTRL written while busy, ignored\n");
            break;
        }
        ps->regs.dma_ctrl = val & LDD_DMA_CTRL_DIR;
        if (val & LDD_DMA_CTRL_START)
            ldd_pcie_start(ps);
        break;
    case LDD_REG_XOR:
        ps->regs.xor = val & 0xff;
        break;
    case LDD_REG_ID:                /* read only */
    case LDD_REG_VERSION:
    case LDD_REG_CAPS:
    case LDD_REG_STATUS:
    case LDD_REG_MAX_SEG:
    case LDD_REG_MAX_DESC:
    case LDD_REG_XFER_BYTES:
    case LDD_REG_ERR_DESC:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "ldd-pcie: write to read-only offset 0x%" HWADDR_PRIx
                      "\n", addr);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "ldd-pcie: write to reserved offset 0x%" HWADDR_PRIx
                      "\n", addr);
        break;
    }
}

static const MemoryRegionOps ldd_pcie_bar0_ops = {
    .read = ldd_pcie_bar0_read,
    .write = ldd_pcie_bar0_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
    .impl  = { .min_access_size = 4, .max_access_size = 4 },
};

static void ldd_pcie_realize(PCIDevice *pdev, Error **errp)
{
    LDDPCIEState *ps = LDD_PCIE(pdev);

    if (ps->max_seg < 512 || ps->max_seg > 1 * MiB ||
        !is_power_of_2(ps->max_seg)) {
        error_setg(errp, "ldd-pcie: max-seg must be a power of 2 from 512 to 1048576");
        return;
    }
    if (ps->max_desc < 1 || ps->max_desc > 4096) {
        error_setg(errp, "ldd-pcie: max-desc must be 1 to 4096");
        return;
    }

    pci_config_set_interrupt_pin(pdev->config, 1);

    if (pci_pm_init(pdev, 0, errp) < 0)
        return;

    if (pcie_endpoint_cap_init(pdev, 0) < 0) {
        error_setg(errp, "ldd-pcie: cannot add PCIe capability");
        return;
    }
    pcie_cap_flr_init(pdev);

    if (msi_init(pdev, 0, 1, true, false, errp) < 0)
        return;


    memory_region_init_io(&ps->bar0, OBJECT(ps), &ldd_pcie_bar0_ops, ps,
                          "ldd-pcie-bar0", 4 * KiB);
    pci_register_bar(pdev, 0,
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64,
                     &ps->bar0);


    if (!memory_region_init_ram(&ps->bar2, OBJECT(ps),
                                "ldd-pcie-bar2", LDD_MEM_SIZE, errp))
        return;
    pci_register_bar(pdev, 2,
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64 |
                     PCI_BASE_ADDRESS_MEM_PREFETCH,
                     &ps->bar2);

    if (msix_init(pdev, LDD_MSIX_VECTORS, &ps->bar0, 0, 0x800, &ps->bar0, 0, 0xc00, 0, errp) < 0)
        return;

    for (int i = 0; i < LDD_MSIX_VECTORS; i++)
        msix_vector_use(pdev, i);

    ps->timer = timer_new_us(QEMU_CLOCK_VIRTUAL, ldd_pcie_complete, ps);
}

static void ldd_pcie_exit(struct PCIDevice *pdev)
{
    LDDPCIEState *ps = LDD_PCIE(pdev);

    timer_free(ps->timer);
    msix_unuse_all_vectors(pdev);
    msix_uninit(pdev, &ps->bar0, &ps->bar0);
    msi_uninit(pdev);
    pcie_cap_exit(pdev);
}

/* Initiate FLR in Device Control resets the function, see ldd_pcie_reset() */
static void ldd_pcie_write_config(PCIDevice *pdev, uint32_t addr,
                                  uint32_t val, int len)
{
    pci_default_write_config(pdev, addr, val, len);
    pcie_cap_flr_write_config(pdev, addr, val, len);
}

/* BAR2 RAM is migrated on its own; properties come from the command line */
static const VMStateDescription ldd_pcie_vmstate = {
    .name = TYPE_LDD_PCIE,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj, LDDPCIEState),
        VMSTATE_MSIX(parent_obj, LDDPCIEState),
        VMSTATE_TIMER_PTR(timer, LDDPCIEState),
        VMSTATE_UINT32(regs.scratch, LDDPCIEState),
        VMSTATE_UINT32(regs.status, LDDPCIEState),
        VMSTATE_UINT32(regs.int_status, LDDPCIEState),
        VMSTATE_UINT32(regs.int_mask, LDDPCIEState),
        VMSTATE_UINT64(regs.desc_addr, LDDPCIEState),
        VMSTATE_UINT32(regs.desc_count, LDDPCIEState),
        VMSTATE_UINT32(regs.dev_offset, LDDPCIEState),
        VMSTATE_UINT32(regs.dma_ctrl, LDDPCIEState),
        VMSTATE_UINT32(regs.xfer_bytes, LDDPCIEState),
        VMSTATE_UINT32(regs.err_desc, LDDPCIEState),
        VMSTATE_UINT32(regs.xor, LDDPCIEState),
        VMSTATE_UINT32(regs.irq_sent, LDDPCIEState),
        VMSTATE_UINT32(regs.result, LDDPCIEState),
        VMSTATE_END_OF_LIST()
    },
};

static const Property ldd_pcie_properties[] = {
    DEFINE_PROP_BOOL("dma64", LDDPCIEState, dma64, true),
    DEFINE_PROP_UINT32("max-seg", LDDPCIEState, max_seg, 64 * KiB),
    DEFINE_PROP_UINT32("max-desc", LDDPCIEState, max_desc, 256),
    DEFINE_PROP_UINT32("latency-us", LDDPCIEState, latency_us, 100),
};

static void ldd_pcie_qdev_reset(DeviceState *dev)
{
    ldd_pcie_reset(LDD_PCIE(dev));
}

static void ldd_pcie_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = PCI_VENDOR_ID_QEMU;
    pc->device_id = 0xcafe;
    pc->realize = ldd_pcie_realize;
    pc->exit = ldd_pcie_exit;
    pc->config_write = ldd_pcie_write_config;
    pc->revision = 1;
    pc->class_id = PCI_CLASS_SYSTEM_DMA;
    dc->desc = "LDD PCIE device for driver examples";
    dc->vmsd = &ldd_pcie_vmstate;
    device_class_set_props(dc, ldd_pcie_properties);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    device_class_set_legacy_reset(dc, ldd_pcie_qdev_reset);
}

static const TypeInfo ldd_pcie_info = {
    .name          = TYPE_LDD_PCIE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(LDDPCIEState),
    .class_init    = ldd_pcie_class_init,
    .interfaces    = (const InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        {}
    },
};

static void ldd_pcie_register_types(void)
{
    type_register_static(&ldd_pcie_info);
}

type_init(ldd_pcie_register_types)

