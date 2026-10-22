# Build every example from one place.
#
#   make [-j N] [KDIR=...]       build all finished examples
#   make 07_ioctl 22_lddbus      build the named examples and their subdirectories
#   make clean
#
# Each directory prints PASS or FAIL; a failing build also prints its log.
# Unfinished *_NOT_COMPLETE examples are skipped unless named explicitly.

EXAMPLES := $(patsubst %/Makefile,%,$(wildcard */Makefile))
SUBDIRS := $(patsubst %/Makefile,%,$(wildcard */*/Makefile))
DIRS := $(EXAMPLES) $(SUBDIRS)

.PHONY: all clean $(EXAMPLES) $(addprefix build-,$(DIRS)) $(addprefix clean-,$(DIRS))
all: $(filter-out %_NOT_COMPLETE,$(EXAMPLES))

# Naming an example builds it together with its subdirectories.
$(EXAMPLES): %: build-%
$(foreach d,$(SUBDIRS),$(eval $(firstword $(subst /, ,$d)): build-$d))

# Modules that link against symbols exported by another module.
build-22_lddbus/sculld: build-22_lddbus

$(addprefix build-,$(DIRS)): build-%:
	@if out="$$($(MAKE) -C $* 2>&1)"; then \
		echo "PASS $*"; \
	else \
		printf 'FAIL %s\n%s\n' "$*" "$$out"; exit 1; \
	fi

clean: $(addprefix clean-,$(DIRS))
$(addprefix clean-,$(DIRS)): clean-%:
	@$(MAKE) -s -C $* clean
