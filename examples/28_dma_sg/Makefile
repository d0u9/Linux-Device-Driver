# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    dma_sg-objs := main.o
    obj-m := dma_sg.o
else
    include ../common.mk
endif
