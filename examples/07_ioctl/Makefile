ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    ioctl-objs := main.o fops.o
    obj-m := ioctl.o
    CFLAGS_main.o := -DDEBUG
    CFLAGS_fops.o := -DDEBUG
else
    include ../common.mk
endif
