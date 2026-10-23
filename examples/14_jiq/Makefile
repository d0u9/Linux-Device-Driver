ifneq ($(KERNELRELEASE),)
jiq-objs := main.o
obj-m := jiq.o
CFLAGS_main.o := -DDEBUG
else
include ../common.mk
endif
