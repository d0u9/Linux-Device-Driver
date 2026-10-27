ifneq ($(KERNELRELEASE),)
    async_io-objs := main.o
    obj-m := async_io.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
