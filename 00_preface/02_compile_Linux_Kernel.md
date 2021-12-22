# Compile Linux Kernel

# Download Source

Linux kernel source are listed in its official web, [The Linux Kernel Archives].

For the time I write this documentation, the latest stable kernel version is
**5.10.4**. Our examples are implemented on this version of kernel, and maybe
follow the updates of kernel a few versions.

There are many ways to obtain kernel source. Download a tarball directly, make
a git clone from official's git repo, or even patch an older version of kernel.

In our example, we directly download the kernel source tarball from the offical
website, and extract it locally in `$LDD_ROOT/kernel/` directory.

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

Firstly, the kernel is set to the default configuration. Then, we tailor this
configuration to fit our need by disable some unnecessary drivers.

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

2. Disable all wireless lan device support and USB network adapters:

   ```
   Device Drivers  --->
     [*] Network device support  --->
       [ ]   Wireless LAN  ----
       < >   USB Network Adapters  ----
   ```

3. Disable all ethernet device support except intel e1000 device:

   ```
   Device Drivers  --->
     [*] Network device support  --->
       [*]   Ethernet driver support  --->
         [*]   Intel devices
           <*>     Intel(R) PRO/100+ support
           <*>     Intel(R) PRO/1000 Gigabit Ethernet support
           <*>     Intel(R) PRO/1000 PCI-Express Gigabit Ethernet support 
   ```

4. Disable IPv6 support. Temporarily has no intersection with IPv6 protocol in
out examples.

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

