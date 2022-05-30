# Opening

Almost every programming book starts with a simple hello world example, and we
follow this convention. Through this chapter, we first set up our QEMU based
development environment. Then download and compile the Linux kernel from the
source and build a simple `initramfs` filesystem for initiating a shell in our
toy system. After that, some configuration setups are essential for a better
user experience, such as mounting host volumes into the guest VM for fast file
sharing. And finally, a simple kernel module will be implemented, which prints
a "Hello World" string in the Kernel's log system.


This chapter has not relevant to any hardware devices. The purpose is simple:
initiation. Throwing too many concepts once is not wise for an introduction; we
hope the readers can gradually become involved in driver development.

# ¶ The end

