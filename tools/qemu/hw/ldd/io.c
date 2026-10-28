/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"

#include "chardev/char-fe.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qom/object.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev-properties-system.h"
#include "hw/isa/isa.h"
#include "migration/vmstate.h"

/*
 * ldd-io: an ISA device with eight LEDs, an 8x8 LED matrix and a one-byte
 * receive path. See tools/qemu/ldd-io.md for the full description.
 */

/* Register offsets */
#define LDD_IO_DATA         0   /* write: LEDs, read: received byte */
#define LDD_IO_STATUS       1   /* read, write 1 to clear */
#define LDD_IO_CTRL         2   /* read/write */
#define LDD_IO_MATRIX       8   /* 8 bytes, one per row */
#define LDD_IO_MATRIX_ROWS  8
#define LDD_IO_SIZE         16
#define LDD_IO_PORT_LIMIT   0x10000     /* size of the x86 I/O port space */

/* STATUS bits */
#define LDD_IO_STATUS_RX_READY  (1u << 0)

/* CTRL bits */
#define LDD_IO_CTRL_IRQ_EN      (1u << 0)
#define LDD_IO_CTRL_LOOPBACK    (1u << 1)
#define LDD_IO_CTRL_MASK        (LDD_IO_CTRL_IRQ_EN | LDD_IO_CTRL_LOOPBACK)

#define TYPE_LDD_IO "ldd-io"
OBJECT_DECLARE_SIMPLE_TYPE(LDDIOState, LDD_IO)

struct LDDIOState {
    ISADevice parent_obj;

    MemoryRegion mem;
    CharFrontend panel;
    CharFrontend keyboard;
    qemu_irq irq;

    /* properties */
    uint32_t iobase;
    bool mmio;
    uint64_t mmiobase;
    uint32_t isairq;

    /* registers */
    uint8_t data_out;
    uint8_t data_in;
    uint8_t status;
    uint8_t ctrl;
    uint8_t matrix[LDD_IO_MATRIX_ROWS];

    uint32_t irq_pulses;    /* shown on the panel only */

    /* panel output that the chardev has not taken yet */
    GString *panel_out;
    guint panel_watch;
    bool panel_dirty;       /* state changed while panel_out was busy */
};

#define LDD_IO_LED_ON   "\033[1;32m●\033[0m"   /* green */
#define LDD_IO_LED_OFF  "\033[90m○\033[0m"     /* grey */

static void ldd_io_append_leds(GString *line, uint8_t v)
{
    for (int i = 7; i >= 0; i--) {
        g_string_append(line, v & (1u << i) ? LDD_IO_LED_ON : LDD_IO_LED_OFF);
        if (i) {
            g_string_append_c(line, ' ');
        }
    }
}

static const char *ldd_io_bit(bool on)
{
    return on ? LDD_IO_LED_ON : LDD_IO_LED_OFF;
}

/*
 * One panel line: label in 11 columns, content in 17, then the hex value.
 * The content is padded by hand because LEDs take more bytes than columns.
 */
static void ldd_io_append_row(GString *panel, const char *label,
                              const char *content, int width, const char *hex)
{
    g_string_append_printf(panel, " %-11s%s%*s%s\033[K\r\n",
                           label, content, 17 - width, "", hex);
}

/* Append one frame of the panel, drawn from the top left corner */
static void ldd_io_build_panel(LDDIOState *state, GString *panel)
{
    g_autoptr(GString) content = g_string_new(NULL);
    g_autofree char *hex = NULL;
    const char *rule = " ────────────────────────────────\033[K\r\n";

    g_string_append(panel, "\033[H");
    if (state->mmio) {
        g_string_append_printf(panel, " ldd-io  mmio 0x%" PRIx64 "  irq %u",
                               state->mmiobase, state->isairq);
    } else {
        g_string_append_printf(panel, " ldd-io  port 0x%x  irq %u",
                               state->iobase, state->isairq);
    }
    g_string_append(panel, "\033[K\r\n");
    g_string_append(panel, rule);

    ldd_io_append_leds(content, state->data_out);
    hex = g_strdup_printf("0x%02x", state->data_out);
    ldd_io_append_row(panel, "DATA out", content->str, 15, hex);

    if (state->status & LDD_IO_STATUS_RX_READY) {
        char c = g_ascii_isprint(state->data_in) ? state->data_in : '.';

        g_string_printf(content, "'%c'", c);
        g_free(hex);
        hex = g_strdup_printf("0x%02x", state->data_in);
        ldd_io_append_row(panel, "DATA in", content->str, 3, hex);
    } else {
        ldd_io_append_row(panel, "DATA in", "-", 1, "");
    }

    g_string_printf(content, "RX_READY %s",
                    ldd_io_bit(state->status & LDD_IO_STATUS_RX_READY));
    g_free(hex);
    hex = g_strdup_printf("0x%02x", state->status);
    ldd_io_append_row(panel, "STATUS", content->str, 10, hex);

    g_string_printf(content, "IRQ_EN %s LOOP %s",
                    ldd_io_bit(state->ctrl & LDD_IO_CTRL_IRQ_EN),
                    ldd_io_bit(state->ctrl & LDD_IO_CTRL_LOOPBACK));
    g_free(hex);
    hex = g_strdup_printf("0x%02x", state->ctrl);
    ldd_io_append_row(panel, "CTRL", content->str, 15, hex);

    g_string_printf(content, "pulses %u", state->irq_pulses);
    ldd_io_append_row(panel, "IRQ", content->str, content->len, "");

    g_string_append(panel, rule);

    for (int row = 0; row < LDD_IO_MATRIX_ROWS; row++) {
        g_string_truncate(content, 0);
        ldd_io_append_leds(content, state->matrix[row]);
        g_free(hex);
        hex = g_strdup_printf("0x%02x", state->matrix[row]);
        ldd_io_append_row(panel, row ? "" : "MATRIX", content->str, 15, hex);
    }
}

static gboolean ldd_io_panel_writable(void *do_not_use, GIOCondition cond,
                                      void *opaque);

/*
 * Send panel_out without blocking. What the chardev does not take now is
 * sent when it becomes writable, so a slow viewer never stalls the guest.
 * A frame is always sent whole, so escape sequences are never cut; frames
 * drawn in the meantime collapse into one redraw of the latest state.
 */
static void ldd_io_panel_flush(LDDIOState *state)
{
    GString *out = state->panel_out;

    while (out->len) {
        int n = qemu_chr_fe_write(&state->panel, (const uint8_t *)out->str,
                                  out->len);

        if (n < 0 && errno != EAGAIN) {
            g_string_truncate(out, 0);  /* viewer gone, drop the frame */
            return;
        }
        if (n <= 0) {
            if (!state->panel_watch) {
                state->panel_watch = qemu_chr_fe_add_watch(&state->panel,
                                        G_IO_OUT | G_IO_HUP,
                                        ldd_io_panel_writable, state);
            }
            if (!state->panel_watch) {
                g_string_truncate(out, 0);
            }
            return;
        }
        g_string_erase(out, 0, n);

        if (!out->len && state->panel_dirty) {
            state->panel_dirty = false;
            ldd_io_build_panel(state, out);
        }
    }
}

static gboolean ldd_io_panel_writable(void *do_not_use, GIOCondition cond,
                                      void *opaque)
{
    LDDIOState *state = opaque;

    state->panel_watch = 0;
    ldd_io_panel_flush(state);
    return G_SOURCE_REMOVE;
}

/* Redraw the whole panel in place */
static void ldd_io_draw_panel(LDDIOState *state)
{
    if (!qemu_chr_fe_backend_connected(&state->panel)) {
        return;
    }
    if (state->panel_out->len) {
        state->panel_dirty = true;  /* drawn once this frame is out */
        return;
    }
    ldd_io_build_panel(state, state->panel_out);
    ldd_io_panel_flush(state);
}

/* Forget output that was meant for a viewer that has gone */
static void ldd_io_panel_drop(LDDIOState *state)
{
    if (state->panel_watch) {
        g_source_remove(state->panel_watch);
        state->panel_watch = 0;
    }
    g_string_truncate(state->panel_out, 0);
    state->panel_dirty = false;
}

static void ldd_io_pulse(LDDIOState *state)
{
    qemu_irq_pulse(state->irq);
    state->irq_pulses++;
}

/* A byte enters the receive buffer: rule 1 of the interrupt */
static void ldd_io_receive_byte(LDDIOState *state, uint8_t byte)
{
    state->data_in = byte;
    state->status |= LDD_IO_STATUS_RX_READY;
    if (state->ctrl & LDD_IO_CTRL_IRQ_EN) {
        ldd_io_pulse(state);
    }
}

static int ldd_io_keyboard_can_receive(void *opaque)
{
    LDDIOState *state = opaque;

    if (state->status & LDD_IO_STATUS_RX_READY) {
        return 0;
    }
    if (state->ctrl & LDD_IO_CTRL_LOOPBACK) {
        return 0;
    }
    return 1;
}

static void ldd_io_keyboard_receive(void *opaque, const uint8_t *buf,
                                    int size)
{
    LDDIOState *state = opaque;

    /* can_receive allows one byte at a time */
    ldd_io_receive_byte(state, buf[0]);
    ldd_io_draw_panel(state);
}

static void ldd_io_panel_event(void *opaque, QEMUChrEvent event)
{
    LDDIOState *state = opaque;

    switch (event) {
    case CHR_EVENT_OPENED:
        /* clear the screen and hide the cursor, then draw */
        ldd_io_panel_drop(state);
        g_string_append(state->panel_out, "\033[2J\033[?25l");
        ldd_io_build_panel(state, state->panel_out);
        ldd_io_panel_flush(state);
        break;
    case CHR_EVENT_CLOSED:
        ldd_io_panel_drop(state);
        break;
    default:
        break;
    }
}

static bool ldd_io_accepts(void *opaque, hwaddr addr, unsigned size,
                           bool is_write, MemTxAttrs attrs)
{
    if (addr >= LDD_IO_MATRIX) {
        return (addr & (size - 1)) == 0;
    }
    return size == 1;
}

static uint64_t ldd_io_read(void *opaque, hwaddr addr, unsigned size)
{
    LDDIOState *state = opaque;
    uint64_t v = 0;

    if (addr >= LDD_IO_MATRIX) {
        /* little endian: the lowest offset is the least significant byte */
        for (int i = size - 1; i >= 0; i--) {
            v = (v << 8) | state->matrix[addr - LDD_IO_MATRIX + i];
        }
        return v;
    }

    switch (addr) {
    case LDD_IO_DATA:
        return state->data_in;
    case LDD_IO_STATUS:
        return state->status;
    case LDD_IO_CTRL:
        return state->ctrl;
    }
    return 0;   /* reserved */
}

static void ldd_io_write(void *opaque, hwaddr addr, uint64_t data,
                         unsigned size)
{
    LDDIOState *state = opaque;
    uint8_t byte = data;
    uint8_t old;

    if (addr >= LDD_IO_MATRIX) {
        for (unsigned i = 0; i < size; i++) {
            state->matrix[addr - LDD_IO_MATRIX + i] = data >> (8 * i);
        }
        ldd_io_draw_panel(state);   /* once per access, whatever its width */
        return;
    }

    switch (addr) {
    case LDD_IO_DATA:
        state->data_out = byte;
        if ((state->ctrl & LDD_IO_CTRL_LOOPBACK) &&
            !(state->status & LDD_IO_STATUS_RX_READY)) {
            ldd_io_receive_byte(state, byte);
        }
        break;
    case LDD_IO_STATUS:
        if (byte & ~LDD_IO_STATUS_RX_READY) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: reserved STATUS bits set: 0x%02x\n",
                          __func__, byte);
        }
        if ((byte & LDD_IO_STATUS_RX_READY) &&
            (state->status & LDD_IO_STATUS_RX_READY)) {
            state->status &= ~LDD_IO_STATUS_RX_READY;
            qemu_chr_fe_accept_input(&state->keyboard);
        }
        break;
    case LDD_IO_CTRL:
        if (byte & ~LDD_IO_CTRL_MASK) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: reserved CTRL bits set: 0x%02x\n",
                          __func__, byte);
        }
        old = state->ctrl;
        state->ctrl = byte & LDD_IO_CTRL_MASK;
        /* rule 2 of the interrupt */
        if (!(old & LDD_IO_CTRL_IRQ_EN) &&
            (state->ctrl & LDD_IO_CTRL_IRQ_EN) &&
            (state->status & LDD_IO_STATUS_RX_READY)) {
            ldd_io_pulse(state);
        }
        /* input held back during loopback can come in now */
        if ((old & LDD_IO_CTRL_LOOPBACK) &&
            !(state->ctrl & LDD_IO_CTRL_LOOPBACK)) {
            qemu_chr_fe_accept_input(&state->keyboard);
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write 0x%02x to reserved offset %" HWADDR_PRIu "\n",
                      __func__, byte, addr);
        return;
    }
    ldd_io_draw_panel(state);
}

static const MemoryRegionOps ldd_io_ops = {
    .read = ldd_io_read,
    .write = ldd_io_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
    .valid.accepts = ldd_io_accepts,
    .impl.min_access_size = 1,
    .impl.max_access_size = 8,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void ldd_io_reset_enter(Object *obj, ResetType type)
{
    LDDIOState *state = LDD_IO(obj);

    state->data_out = 0;
    state->data_in = 0;
    state->status = 0;
    state->ctrl = 0;
    memset(state->matrix, 0, sizeof(state->matrix));
    state->irq_pulses = 0;
}

static void ldd_io_reset_exit(Object *obj, ResetType type)
{
    LDDIOState *state = LDD_IO(obj);

    qemu_chr_fe_accept_input(&state->keyboard);
    ldd_io_draw_panel(state);
}

static void ldd_io_realize(DeviceState *dev, Error **errp)
{
    ISADevice *isadev = ISA_DEVICE(dev);
    LDDIOState *state = LDD_IO(dev);

    if (state->isairq >= ISA_NUM_IRQS) {
        error_setg(errp, "irq %u is out of range, must be 0 to %d",
                   state->isairq, ISA_NUM_IRQS - 1);
        return;
    }
    if (!state->mmio && state->iobase > LDD_IO_PORT_LIMIT - LDD_IO_SIZE) {
        error_setg(errp, "iobase 0x%x is out of range, must be at most 0x%x",
                   state->iobase, LDD_IO_PORT_LIMIT - LDD_IO_SIZE);
        return;
    }
    if (state->mmio) {
        MemoryRegionSection section;

        /* 8-byte MATRIX accesses must be naturally aligned in memory too */
        if (state->mmiobase & (LDD_IO_SIZE - 1)) {
            error_setg(errp, "mmiobase 0x%" PRIx64 " is not %d-byte aligned",
                       state->mmiobase, LDD_IO_SIZE);
            return;
        }
        /* catches devices already mapped, not ones mapped later */
        section = memory_region_find(isa_address_space(isadev),
                                     state->mmiobase, LDD_IO_SIZE);
        if (section.mr) {
            error_setg(errp, "mmiobase 0x%" PRIx64 " overlaps %s",
                       state->mmiobase, memory_region_name(section.mr));
            memory_region_unref(section.mr);
            return;
        }
    }
    state->irq = isa_get_irq(isadev, state->isairq);
    state->panel_out = g_string_new(NULL);

    memory_region_init_io(&state->mem, OBJECT(dev), &ldd_io_ops, state,
                          TYPE_LDD_IO, LDD_IO_SIZE);
    if (state->mmio) {
        memory_region_add_subregion(isa_address_space(isadev),
                                    state->mmiobase, &state->mem);
    } else {
        isa_register_ioport(isadev, &state->mem, state->iobase);
    }

    qemu_chr_fe_set_handlers(&state->keyboard, ldd_io_keyboard_can_receive,
                             ldd_io_keyboard_receive, NULL, NULL, state, NULL,
                             true);
    qemu_chr_fe_set_handlers(&state->panel, NULL, NULL, ldd_io_panel_event,
                             NULL, state, NULL, true);
}

static void ldd_io_unrealize(DeviceState *dev)
{
    LDDIOState *state = LDD_IO(dev);

    ldd_io_panel_drop(state);
    g_string_free(state->panel_out, true);
}

static int ldd_io_post_load(void *opaque, int version_id)
{
    LDDIOState *state = opaque;

    qemu_chr_fe_accept_input(&state->keyboard);
    ldd_io_draw_panel(state);
    return 0;
}

static const VMStateDescription vmstate_ldd_io = {
    .name = TYPE_LDD_IO,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = ldd_io_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8(data_out, LDDIOState),
        VMSTATE_UINT8(data_in, LDDIOState),
        VMSTATE_UINT8(status, LDDIOState),
        VMSTATE_UINT8(ctrl, LDDIOState),
        VMSTATE_UINT8_ARRAY(matrix, LDDIOState, LDD_IO_MATRIX_ROWS),
        VMSTATE_UINT32(irq_pulses, LDDIOState),
        VMSTATE_END_OF_LIST()
    }
};

static const Property ldd_io_properties[] = {
    DEFINE_PROP_UINT32("iobase", LDDIOState, iobase, 0x200),
    DEFINE_PROP_BOOL("mmio", LDDIOState, mmio, false),
    DEFINE_PROP_UINT64("mmiobase", LDDIOState, mmiobase, 0xff000000),
    DEFINE_PROP_UINT32("irq", LDDIOState, isairq, 5),
    DEFINE_PROP_CHR("panel", LDDIOState, panel),
    DEFINE_PROP_CHR("keyboard", LDDIOState, keyboard),
};

static void ldd_io_class_initfn(ObjectClass *kclass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(kclass);
    ResettableClass *rc = RESETTABLE_CLASS(kclass);

    dc->realize = ldd_io_realize;
    dc->unrealize = ldd_io_unrealize;
    dc->vmsd = &vmstate_ldd_io;
    rc->phases.enter = ldd_io_reset_enter;
    rc->phases.exit = ldd_io_reset_exit;
    device_class_set_props(dc, ldd_io_properties);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo ldd_io_types[] = {
    {
        .name          = TYPE_LDD_IO,
        .parent        = TYPE_ISA_DEVICE,
        .instance_size = sizeof(LDDIOState),
        .class_init    = ldd_io_class_initfn,
    },
};

DEFINE_TYPES(ldd_io_types)
