ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    proc_fs_basic-objs := main.o
    obj-m := proc_fs_basic.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
