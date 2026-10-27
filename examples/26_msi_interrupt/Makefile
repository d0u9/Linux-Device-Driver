ifneq ($(KERNELRELEASE),)
    msi_interrupt-objs := main.o
    obj-m := msi_interrupt.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
