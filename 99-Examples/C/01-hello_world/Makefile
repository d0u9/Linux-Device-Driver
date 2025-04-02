ifneq ($(KERNELRELEASE),)

# In kbuild context
module-objs := hello_world.o
obj-m := hello_world.o

CFLAGS_hello_world.o += $(CFLAGS)

else

# In normal make context
include ../../kmod.mk
PWD := $(shell pwd)

.PHONY: modules
modules:
	$(MAKE) $(BUILD_OPTS) -C $(LDD_KDIR) M=$(PWD) modules

.PHONY: clean
clean:
	$(MAKE) $(BUILD_OPTS) -C $(LDD_KDIR) M=$(PWD) clean

endif
