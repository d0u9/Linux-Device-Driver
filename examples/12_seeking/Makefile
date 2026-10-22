ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    seeking-objs := main.o fops.o
    obj-m := seeking.o
    CFLAGS_main.o := -DDEBUG
    CFLAGS_fops.o := -DDEBUG
else
    include ../common.mk
endif
