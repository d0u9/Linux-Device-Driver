# Opening

Almost every programming book starts with a simple hello world example, and we
follow this convention. Through this chapter, we firstly setup our QEMU based
development environment. Then download and compile Linux kernel from source,
build a simple `initramfs` for initiating a shell in our tiny system. After
that, some configurations will be made which are essential to a better usage
experience, such as mount host volumes into guest for fast file sharing. And
finally, a simple kernel module will be implemented which will print a string
of "Hello World" in Kernel's log system.

This chapter has no relevant to any hardware devices. The purpose is simple:
initiation. Throw too much concepts once is not wise for introduction, we hope
readers can involve into the process of driver development gradually.

# ¶ The end

