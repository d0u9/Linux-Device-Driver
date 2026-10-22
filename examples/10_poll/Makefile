ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    poll-objs := main.o fops.o
    obj-m := poll.o
    CFLAGS_main.o := -DDEBUG
    CFLAGS_fops.o := -DDEBUG
else
    include ../common.mk
endif
