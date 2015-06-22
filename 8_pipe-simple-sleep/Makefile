debug=y

ifeq ($(debug),y)
    DEBUG_FLAGS = -O2 -g -DENABLE_DEBUG
else
    DEBUG_FLAGS = -O1
endif

EXTRA_CFLAGS += $(DEBUG_FLAGS)

ifneq ($(KERNELRELEASE),)
    pipe-objs := main.o
    obj-m := pipe.o
else
    KERN_DIR ?= /lib/modules/$(shell uname -r)/build
    PWD = $(shell pwd)

modules:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) modules

endif

clean:
	rm -fr *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
