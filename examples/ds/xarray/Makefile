# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    xarray-objs := main.o
    obj-m := xarray.o
else
    include ../../common.mk
endif
