# Environment setup

BUILD_OPTS := CFLAGS="-DDEBUG"

ifeq ($(LDD_KDIR),)
$(warning LDD_KDIR is not set, using default value)
LDD_KDIR ?= /lib/modules/$(shell uname -r)/build
endif

ifneq ($(LDD_LLVM),)
$(warning Using LLVM to compile kernel modules)
BUILD_OPTS += "LLVM=1"
endif

