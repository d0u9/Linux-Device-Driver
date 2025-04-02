# Compile Linux Kernel

## Configure Linux Kernel

As [LFS] says:

> A good starting place for setting up the kernel configuration is to run make
> defconfig, This will set the base configuration to a good state that takes
> your current system architecture into account.

Firstly, set the kernel to the default configuration. Then, tailor it based on
the default configuration to fit our needs by disabling some unnecessary
drivers.

```bash
make defconfig
make menuconfig
```

Disable unnecessary components:

1. Disable all sound card supports:

    ```text
    Device Drivers --->
      < >Sound card support  ----
    ```

2. Disable all wireless lan device supports and USB network adapters:

   ```text
   Device Drivers  --->
     [*] Network device support  --->
       [ ]   Wireless LAN  ----
       < >   USB Network Adapters  ----
   ```

3. Disable all ethernet device supports except the intel e1000 device:

   ```text
   Device Drivers  --->
     [*] Network device support  --->
       [*]   Ethernet driver support  --->
         [*]   Intel devices
           <*>     Intel(R) PRO/100+ support
           <*>     Intel(R) PRO/1000 Gigabit Ethernet support
           <*>     Intel(R) PRO/1000 PCI-Express Gigabit Ethernet support 
   ```

4. Disable IPv6 support. Currently, no content about IPv6 in our examples.

   ```text
    [*] Networking support  --->
      Networking options  --->
        < >   The IPv6 protocol  ----
   ```

5. Disable wireless network support:

    ```text
    [*] Networking support  --->
      [ ]   Wireless  ----
    ```

## More with Rust Configuration

Rust support is turned of by default, and you have to enable it in the kernel configuraion
onece you want to dev on rust.

```text
  General setup  --->
    [*] Rust support
```

Be aware that this option only appears if you have the rust toolchina properly configured on your host.

## Compiling

### Without Rust

Tranditionally, compiling the Kenel uses GCC compiler by default.

```bash
make -j bzImage

# Generate Module.symvers file
make -j modules
```

### With Rust

However, GCC compiler doesn't support Rust compiling at current time. It is LLVM compiler used to generating rust objective files in the kernel.

Than means, if you have enabled RUST and want to compile all rust parts, do it as:

```bash
make LLVM=1 -j bzImage
make LLVM=1 -j modules
```

## ¶ The end

[LFS]: http://www.linuxfromscratch.org/lfs/view/stable/chapter08/kernel.html
