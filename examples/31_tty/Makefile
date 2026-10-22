ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    lddtty-objs := main.o
    obj-m := lddtty.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
