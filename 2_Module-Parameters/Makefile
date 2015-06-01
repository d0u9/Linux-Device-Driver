DEBUG = y

ifeq ($(DEBUG),y)
    DEBFLAGS = -O -g -DENABLE_DEBUG
else
    DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)
    module-parameters-objs := main.o
    obj-m := module-parameters.o
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
