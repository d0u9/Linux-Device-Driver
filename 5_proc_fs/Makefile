debug=y

ifeq ($(debug),y)
    DEBFLAGS = -O2 -g -DENABLE_DEBUG
else
    DEBFLAGS = -O1
endif

EXTRA_CFLAGS += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)
    proc-fs-objs := main.o
    obj-m := proc-fs.o
else
    KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
    PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
endif

clean:
	rm -fr *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
