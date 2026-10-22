ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    short_mmio-objs := main.o
    obj-m := short_mmio.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
