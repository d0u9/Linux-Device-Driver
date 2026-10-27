ifneq ($(KERNELRELEASE),)
    memdisk-objs := main.o
    obj-m := memdisk.o
else
    include ../common.mk
endif
