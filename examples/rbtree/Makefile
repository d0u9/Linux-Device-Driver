# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    rbtree-objs := main.o
    obj-m := rbtree.o
else
    include ../common.mk
endif
