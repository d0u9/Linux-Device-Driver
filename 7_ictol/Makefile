DEBUG = y

ifeq ($(DEBUG),y)
    DEB_FLAGS = -O2 -g -DENABLE_DEBUG
else
    DEB_FLAGS = -O1
endif

EXTRA_CFLAGS += $(DEB_FLAGS)

ifneq ($(KERNELRELEASE),)
    ioctl-objs := main.o
    obj-m := ioctl.o
else
    KERN_DIR ?= /lib/modules/$(shell uname -r)/build
    PWD = $(shell pwd)

modules:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) modules

endif

clean:
	rm -fr *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
