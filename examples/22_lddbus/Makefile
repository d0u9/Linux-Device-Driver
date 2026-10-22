ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    lddbus-objs := main.o
    obj-m := lddbus.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
