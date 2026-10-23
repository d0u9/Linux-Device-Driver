ifneq ($(KERNELRELEASE),)
poll-objs := main.o fops.o
obj-m := poll.o
CFLAGS_main.o := -DDEBUG
CFLAGS_fops.o := -DDEBUG
else
include ../common.mk
endif
