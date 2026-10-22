ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    short_ioport-objs := main.o
    obj-m := short_ioport.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
