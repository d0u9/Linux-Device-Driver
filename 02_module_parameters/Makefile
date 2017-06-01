ifneq ($(KERNELRELEASE),)
    ccflags-y := -std=gnu99
    module_parameters-objs := main.o
    obj-m := module_parameters.o
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD := $(shell pwd)

.PHONY: modules
modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

.PHONY: clean
clean:
	rm -fr *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers
