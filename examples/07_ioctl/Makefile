ifneq ($(KERNELRELEASE),)
ioctl-objs := main.o fops.o
obj-m := ioctl.o
CFLAGS_main.o := -DDEBUG
CFLAGS_fops.o := -DDEBUG
else
include ../common.mk
endif
