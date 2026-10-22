ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    memdisk-objs := main.o
    obj-m := memdisk.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
