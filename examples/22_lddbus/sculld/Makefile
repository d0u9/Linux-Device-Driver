ifneq ($(KERNELRELEASE),)
    ccflags-y := -DENABLE_DEBUG
    sculld-objs := main.o
    obj-m := sculld.o
    CFLAGS_main.o := -DDEBUG -I$(src)/../
else
    EXTRA_SYMBOLS := $(CURDIR)/../Module.symvers
    include ../../common.mk
endif
