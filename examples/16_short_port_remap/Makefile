ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    short_port_remap-objs := main.o
    obj-m := short_port_remap.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
