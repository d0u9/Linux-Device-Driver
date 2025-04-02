ifneq ($(KERNELRELEASE),) ### In kbuild context

hello_world-objs := kmod_main.o
obj-m := hello_world.o

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