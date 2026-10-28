# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    ida-objs := main.o
    obj-m := ida.o
else
    include ../../common.mk
endif
