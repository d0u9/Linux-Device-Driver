ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    zero_copy-objs := main.o
    obj-m := zero_copy.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
