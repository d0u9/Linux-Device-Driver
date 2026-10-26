ifneq ($(KERNELRELEASE),)
    pci_skel-objs := main.o
    obj-m := pci_skel.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
