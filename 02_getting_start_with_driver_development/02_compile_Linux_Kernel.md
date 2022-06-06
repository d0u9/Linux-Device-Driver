# Compile Linux Kernel

# Download Source

Linux kernel sources of different versions are listed on its official web, [The Linux Kernel Archives].

At the time I write this documentation, the latest stable kernel version is
**5.10.4**, and the examples in this book are implemented against this version.

There are many ways to obtain the kernel source. For example, download a tarball directly, make a git clone from the official's git repo, or even get a new version by patching an old one.

Here, we directly download the kernel source tarball from the official website and extract it locally in `$LDD_ROOT/kernel/` directory.

To download and extract the source ball:


```
mkdir $LDD_ROOT/kernel && cd $LDD_ROOT/kernel
wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.10.4.tar.xz
tar -xf linux-5.10.4.tar.xz
```

# Configure Linux Kernel

As [LFS] says:

> A good starting place for setting up the kernel configuration is to run make
> defconfig, This will set the base configuration to a good state that takes
> your current system architecture into account.

Firstly, set the kernel to the default configuration. Then, tailor it based on
the default configuration to fit our needs by disabling some unnecessary
drivers.

```
make defconfig
make menuconfig
```

Disable unnecessary components:

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
           <*>     Intel(R) PRO/100+ support
           <*>     Intel(R) PRO/1000 Gigabit Ethernet support
           <*>     Intel(R) PRO/1000 PCI-Express Gigabit Ethernet support 
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
make -j bzImage

# Generate Module.symvers file
make -j modules
```

# ¶ The end

[The Linux Kernel Archives]: https://www.kernel.org/
[LFS]: http://www.linuxfromscratch.org/lfs/view/stable/chapter08/kernel.html

