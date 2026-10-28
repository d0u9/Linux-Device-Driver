# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    bitmap-objs := main.o
    obj-m := bitmap.o
else
    include ../../common.mk
endif
