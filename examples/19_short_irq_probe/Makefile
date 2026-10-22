ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    short_irq_probe-objs := main.o
    obj-m := short_irq_probe.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
