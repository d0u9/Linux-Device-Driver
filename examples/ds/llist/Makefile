# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    llist-objs := main.o
    obj-m := llist.o
else
    include ../../common.mk
endif
