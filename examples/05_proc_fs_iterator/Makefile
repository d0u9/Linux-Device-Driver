ifneq ($(KERNELRELEASE),)
proc_fs_iterator-objs := main.o fops.o
obj-m := proc_fs_iterator.o
CFLAGS_main.o := -DDEBUG
CFLAGS_fops.o := -DDEBUG
else
include ../common.mk
endif
