ifneq ($(KERNELRELEASE),)
    simple-objs := main.o
    obj-m := simple.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
