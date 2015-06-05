DEBUG = y

ifeq ($(DEBUG),y)
    DEBFLAGS = -O -g -DENABLE_DEBUG
else
    DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)
    echo-dev-objs := main.o fops.o
    obj-m := echo-dev.o
else
    KERN_DIR ?= /lib/modules/$(shell uname -r)/build
    PWD = $(shell pwd)

modules:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) modules
endif

clean:
	rm -fr *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
