ifneq ($(KERNELRELEASE),)
    dma-objs := main.o
    obj-m := dma.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
