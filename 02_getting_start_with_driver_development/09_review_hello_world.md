# Review Hello World


## Details in code

There are two functions defined in our hello world module, one is constructor
function and another is destructor function. Constructor function is invoked
when the module is loaded into the kernel, and destructor function is invoked
when the module is removed from the kernel. Unlike C programs in the userspace,
there is no `main()` function or alike in kernel module source. The entry point
of our own logical is the function which is passed as a variable to
`module_init()` macro. Also, unlike userspace programs, the kernel won't help us
to free any resources that was allocated during the running of our module. So,
that is why a macro called `module_exit()` which accepts an function as its
parameter exists. Function passed to this macro acts as a destructor in which
various resources will be freed, such as memory requested from system or any
registered handlers of hardware.

Another macro which has mentioned before is `MODULE_LICENSE()`. It is used to
tell the kernel what the license this module opts. Any interface exported as GPL
licensed in kernel are usage prohibited in kernel module whose module license is
"Proprietary".

`MODULE_AUTHOR()` declares the author of this module, and `MODULE_DESCRIPTION()`
describes what the module does. Information declared in these two macros are
embedded in the final `.ko` binary file, and can be view via `modinfo` command
in userspace. We will see this in detail later.

Header file of `linux/module.h` contains functions and macros that are essential
to kernel module programming. Every kernel module needs this header file with
no exception. This header file includes other necessary header files recursively
which are used during the compiling, loading and running time of the module's
whole life span.

Another noticeable point is the use of `__init` and `__exit` markers. Similarly,
there are two makers, `__initdata` and `__exitdata`, for variables other than
functions. All these markers give clues to module loader that the functions or
variables marked with are only be used during the loading or unloading time. In
our example, function `m_init()` is used only once during the loading period of
module, and then it fulfilled its mission and can be removed from memory. `


# ¶ The end
