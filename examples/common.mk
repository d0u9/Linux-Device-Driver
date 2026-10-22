# Shared "normal make context" half of the examples' double-entering Makefiles.
# Each example keeps its kbuild lines under `ifneq ($(KERNELRELEASE),)` and
# includes this file from the `else` branch. See
# docs/02_getting_started/07_compiling.md for how the two passes work.
#
# Optional variables set by the including Makefile:
#   EXTRA_SYMBOLS  Module.symvers of modules this one depends on.

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: modules
modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) \
		$(if $(EXTRA_SYMBOLS),KBUILD_EXTRA_SYMBOLS=$(EXTRA_SYMBOLS)) modules

.PHONY: clean
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) clean
