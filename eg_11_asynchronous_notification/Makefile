ifneq ($(KERNELRELEASE),)
    ccflags-y := -std=gnu99 -DENABLE_DEBUG
    async_notify-objs := main.o fops.o
    obj-m := async_notify.o
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD := $(shell pwd)

.PHONY: modules
modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

.PHONY: clean
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

CFLAGS_main.o := -DDEBUG
CFLAGS_fops.o := -DDEBUG
