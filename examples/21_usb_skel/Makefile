ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    usb_skel-objs := main.o
    obj-m := usb_skel.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
