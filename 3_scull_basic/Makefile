DEBUG = y

ifeq ($(DEBUG),y)
    DEBFLAGS = -O -g -DENABLE_DEBUG
else
    DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)
    scull-objs := main.o fops.o
    obj-m := scull.o
else
    KENELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KENELDIR) M=$(PWD) modules
endif

clean:
	rm -fr *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
