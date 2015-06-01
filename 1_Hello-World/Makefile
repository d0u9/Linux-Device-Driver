DEBUG = y

ifeq ($(DEBUG),y)
    DEBFLAGS = -O -g -DENABLE_DEBUG
else
    DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)
    hello-world-objs := main.o
    obj-m := hello-world.o
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	rm -fr *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
