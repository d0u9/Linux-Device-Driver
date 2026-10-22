ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    dma_sg-objs := main.o
    obj-m := dma_sg.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
