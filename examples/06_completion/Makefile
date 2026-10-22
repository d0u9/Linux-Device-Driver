ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    completion-objs := main.o
    obj-m := completion.o
    CFLAGS_main.o := -DDEBUG
else
    include ../common.mk
endif
