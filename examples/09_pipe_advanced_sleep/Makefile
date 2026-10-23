ifneq ($(KERNELRELEASE),)
pipe_advanced_sleep-objs := main.o fops.o
obj-m := pipe_advanced_sleep.o
CFLAGS_main.o := -DDEBUG
CFLAGS_fops.o := -DDEBUG
else
include ../common.mk
endif
