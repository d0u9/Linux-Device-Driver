ifneq ($(KERNELRELEASE),)
    short_irq-objs := main.o
    obj-m := short_irq.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
