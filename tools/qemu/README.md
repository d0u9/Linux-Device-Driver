# Custom QEMU devices

Sources for the emulated devices used by the driver examples, laid out like the
QEMU source tree. Copy `hw/` over a QEMU checkout and add the build entries
below.

| Device | Source | Used by |
| --- | --- | --- |
| `isa-kmod-edu` | [`hw/misc/isa_kmod_edu.c`](hw/misc/isa_kmod_edu.c) | `15_short_ioport` to `19_short_irq_probe` |
| `pci-kmod-edu` | [`hw/misc/pci_kmod_edu.c`](hw/misc/pci_kmod_edu.c) | `20_pci_skel`, `26_msi_interrupt` |
| `usb-kmod-edu` | [`hw/usb/usb_kmod_edu.c`](hw/usb/usb_kmod_edu.c) | `21_usb_skel` |

> TODO: These files still target the QEMU 5.1 API they were written for and do
> not build against QEMU 11.1.2 yet.

## Build entries

Append to `hw/misc/Kconfig`:

```
config ISA_KMOD_EDU
    bool
    default y if TEST_DEVICES
    depends on ISA_BUS

config PCI_KMOD_EDU
    bool
    default y if TEST_DEVICES
    depends on PCI && MSI_NONBROKEN
```

Append to `hw/misc/meson.build`:

```
softmmu_ss.add(when: 'CONFIG_ISA_KMOD_EDU', if_true: files('isa_kmod_edu.c'))
softmmu_ss.add(when: 'CONFIG_PCI_KMOD_EDU', if_true: files('pci_kmod_edu.c'))
```

Append to `hw/usb/Kconfig`:

```
config USB_KMOD_EDU
    bool
    default y if TEST_DEVICES
    depends on USB
```

Append to `hw/usb/meson.build`:

```
softmmu_ss.add(when: 'CONFIG_USB_KMOD_EDU', if_true: files('usb_kmod_edu.c'))
```
