ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    pipe_simple_sleep-objs := main.o fops.o
    obj-m := pipe_simple_sleep.o
    CFLAGS_main.o := -DDEBUG
    CFLAGS_fops.o := -DDEBUG
else
    include ../common.mk
endif
