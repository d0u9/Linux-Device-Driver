ifneq ($(KERNELRELEASE),)
    usb_skel-objs := main.o
    obj-m := usb_skel.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
