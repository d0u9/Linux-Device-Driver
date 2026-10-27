ifneq ($(KERNELRELEASE),)
    netdev-objs := main.o
    obj-m := netdev.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
