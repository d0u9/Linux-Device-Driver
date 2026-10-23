ifneq ($(KERNELRELEASE),)
async_notify-objs := main.o fops.o
obj-m := async_notify.o
CFLAGS_main.o := -DDEBUG
CFLAGS_fops.o := -DDEBUG
else
include ../common.mk
endif
