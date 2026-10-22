ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    jit-objs := main.o fops.o
    obj-m := jit.o
    CFLAGS_main.o := -DDEBUG
    CFLAGS_fops.o := -DDEBUG
else
    include ../common.mk
endif
