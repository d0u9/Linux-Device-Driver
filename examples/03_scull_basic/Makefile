ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    scull-objs := main.o fops.o
    obj-m := scull.o
    CFLAGS_main.o := -DDEBUG
    CFLAGS_fops.o := -DDEBUG
else
    include ../common.mk
endif
