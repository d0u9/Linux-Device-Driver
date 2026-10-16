# Compile Linux Kernel

# Build dependencies

On Fedora:

```bash
sudo dnf install flex bison ncurses-devel elfutils-libelf-devel openssl-devel
```

# Kernel source

Linux kernel sources of different versions are listed on its official web, [The Linux Kernel Archives].

This chapter targets the Linux **7.2.y** stable series, using **7.2.11** below.

Download the release tarball and extract it into `$LDD_ROOT/src/linux-stable`:

```bash
mkdir -p "$LDD_ROOT/src"
cd "$LDD_ROOT/src"
wget https://cdn.kernel.org/pub/linux/kernel/v7.x/linux-7.2.11.tar.xz
mkdir linux-stable && tar -xf linux-7.2.11.tar.xz -C linux-stable --strip-components=1
cd linux-stable
```

# Configure Linux Kernel

As [LFS] says:

> A good starting place for setting up the kernel configuration is to run make
> defconfig, This will set the base configuration to a good state that takes
> your current system architecture into account.

Firstly, set the kernel to the default configuration. Then, tailor it based on
the default configuration to fit our needs by disabling some unnecessary
drivers.

```bash
make defconfig

scripts/config \
    --disable SOUND \
    --disable WLAN \
    --disable USB_NET_DRIVERS \
    --disable WIRELESS \
    --disable IPV6 \
    --disable IA32_EMULATION \
    --disable E100 \
    --enable E1000 \
    --disable E1000E \
    --disable RUST

make olddefconfig
```

`make olddefconfig` resolves Kconfig dependencies after the edits.

The commands above adjust the listed options, not every Ethernet driver.
Use `make menuconfig` for the remaining drivers or to review the configuration:

1. Disable all sound card supports:

    ```
    Device Drivers --->
      < >Sound card support  ----
    ```

2. Disable all wireless lan device supports and USB network adapters:

   ```
   Device Drivers  --->
     [*] Network device support  --->
       [ ]   Wireless LAN  ----
       < >   USB Network Adapters  ----
   ```

3. Disable all ethernet device supports except the intel e1000 device:

   ```
   Device Drivers  --->
     [*] Network device support  --->
       [*]   Ethernet driver support  --->
         [*]   Intel devices
           < >     Intel(R) PRO/100+ support
           <*>     Intel(R) PRO/1000 Gigabit Ethernet support
           < >     Intel(R) PRO/1000 PCI-Express Gigabit Ethernet support
   ```

4. Disable IPv6 support. Currently, no content about IPv6 in our examples.

   ```
    [*] Networking support  --->
      Networking options  --->
        < >   The IPv6 protocol  ----
   ```

5. Disable wireless network support:

    ```
    [*] Networking support  --->
      [ ]   Wireless  ----
    ```

# Compiling

```
make -j"$(nproc)" bzImage

# Generate Module.symvers file
make -j"$(nproc)" modules
```

# ¶ The end

[The Linux Kernel Archives]: https://www.kernel.org/
[LFS]: http://www.linuxfromscratch.org/lfs/view/stable/chapter08/kernel.html

