# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    list-objs := main.o
    obj-m := list.o
else
    include ../../common.mk
endif
