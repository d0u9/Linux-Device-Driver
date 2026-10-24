#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/isa/isa.h"

#define TYPE_LDD_PARALLEL "ldd-parallel"
OBJECT_DECLARE_SIMPLE_TYPE(LDDParallelState, LDD_PARALLEL)

struct LDDParallelState {
    ISADevice parent_obj;
};

static const TypeInfo ldd_parallel_types[] = {
    {
        .name          = TYPE_LDD_PARALLEL,
        .parent        = TYPE_ISA_DEVICE,
        .instance_size = sizeof(LDDParallelState),
    },
};

DEFINE_TYPES(ldd_parallel_types)
