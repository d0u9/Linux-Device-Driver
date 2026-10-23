ifneq ($(KERNELRELEASE),)
module_parameters-objs := main.o
obj-m := module_parameters.o
CFLAGS_main.o := -DDEBUG
else
include ../common.mk
endif
