# lddbus

A virtual bus, after LDD3's lddbus, and `sculld`, a module that puts two
devices and their driver on it. No hardware is needed: the example is about
the device model, that is, how buses, devices, drivers and classes are tied
together and how they show up in sysfs.

- `lddbus.ko` registers the bus `ldd`, its root device `ldd0` and the class
  `ldd`, and exports `register_ldd_device()`, `register_ldd_driver()` and
  their unregister functions.
- `sculld/sculld.ko` registers the driver `sculld` and the devices `sculld0`
  and `sculld1` on the bus, and gives each device a char device and a node
  in `/dev`. The char devices have no file operations.

## what each part shows

- **A bus.** `struct bus_type` names the bus and supplies `match()`, which
  the driver core calls for every device and driver pair. `ldd_match()`
  matches a device whose name starts with the driver's name, so `sculld0`
  and `sculld1` both bind to `sculld`. The driver has no `probe()`, so
  binding only links them.
- **A root device.** `ldd0` is the parent of every device on the bus, so
  they all appear under `/sys/devices/ldd0/`.
- **Embedding.** `struct ldd_device` and `struct ldd_driver` embed a
  `struct device` and a `struct device_driver`; `container_of()` gets back
  from the embedded structure to the outer one. `sculld` embeds the
  `ldd_device` once more, in `struct sculld_dev`.
- **Registering.** `device_register()` needs a `release()` function, and
  if it fails `put_device()` must still be called.
- **Attributes.** The bus has `bus_version`, every driver on the bus has
  `version`, `ldd0` has `dev_info`, and each `sculld` device has `devno`.
  All of them are attribute groups, which the driver core creates together
  with the object, before user space hears of it.
- **uevent.** `ldd_uevent()` adds `LDDBUS_VERSION` to the events of every
  device on the bus.
- **A class.** `device_create()` puts `sculld0` in the class `ldd` and makes
  devtmpfs create `/dev/sculld0`. Its parent is the device on the bus, so
  the class entry links to it.

## where it ends up in sysfs

```
/sys/bus/ldd/
├── bus_version
├── devices/sculld0 -> ../../../devices/ldd0/sculld0
└── drivers/sculld/
    ├── version
    └── sculld0 -> ../../../../devices/ldd0/sculld0

/sys/devices/ldd0/
├── dev_info
└── sculld0/
    ├── devno
    ├── driver -> ../../../bus/ldd/drivers/sculld
    └── ldd/sculld0/dev              the /dev node's device

/sys/class/ldd/sculld0 -> ../../devices/ldd0/sculld0/ldd/sculld0
```

## shorter ways of writing the same

- `root_device_register("ldd0")` allocates and registers a root device
  like `ldd0` in one call.

## older ways of doing the same

- Attributes were created with `bus_create_file()`, `device_create_file()`
  and `driver_create_file()` after registering. That races with udev, which
  may look for them as soon as the uevent arrives; attribute groups do not.
- `show()` functions used `snprintf(buf, PAGE_SIZE, ...)`, and `devno` used
  `print_dev_t()`; both are `sysfs_emit()` now.
- `struct bus_type` can be `const`.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

Then build `sculld` the same way in the `sculld` directory; it uses the
symbols in `../Module.symvers`, so build `lddbus` first.

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

## Usage

Copy **load_module.sh**, **lddbus.ko** and **sculld/sculld.ko** to one
directory on the target machine, then run:

```bash
sh load_module.sh
```

It loads `lddbus` first, then `sculld`; `unload` removes them in reverse.

## test the module

```bash
cat /sys/bus/ldd/bus_version                 # 1.9
cat /sys/bus/ldd/drivers/sculld/version      # 1.21
ls /sys/bus/ldd/drivers/sculld               # sculld0 and sculld1 are bound
cat /sys/bus/ldd/devices/sculld1/uevent      # DRIVER=sculld, LDDBUS_VERSION=1.9
cat /sys/bus/ldd/devices/sculld1/devno       # e.g. 247:1
ls -l /sys/class/ldd/ /dev/sculld*
```

Unbind a device from the driver and bind it again:

```bash
echo sculld0 > /sys/bus/ldd/drivers/sculld/unbind
echo sculld0 > /sys/bus/ldd/drivers/sculld/bind
```

---

### ¶ The end
