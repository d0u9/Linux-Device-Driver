ifneq ($(KERNELRELEASE),) ### In kbuild context

module_parameters-objs := kmod_main.o
obj-m := module_parameters.o

else ### In normal make context

include ../../kmod.mk

PWD := $(shell pwd)

.PHONY: modules
modules:
	$(MAKE) $(BUILD_OPTS) -C $(LDD_KDIR) M=$(PWD) modules

.PHONY: clean
clean:
	$(MAKE) $(BUILD_OPTS) -C $(LDD_KDIR) M=$(PWD) clean

endif