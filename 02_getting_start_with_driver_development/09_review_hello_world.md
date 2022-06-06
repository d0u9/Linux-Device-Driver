# Review Hello World


## Details in code

There are two functions defined in our hello world module, one is the
constructor function, and another is the destructor function. The module loader
invokes the constructor function during the loading time, and the same, the
module loader invokes the destructor function when the module is removed from
the kernel. Unlike C programs in the userspace, there is no `main()` function
or alike in the kernel module. The module logical entry point is the function
passed to the `module_init()` macro. Also, unlike userspace programs, the kernel
won't help us free any resources allocated during the running of the module.
That is the deconstructor's work to release any resources, such as hardware
handlers or memory segments, allocated before. This deconstructor is registered
via the `module_exit()` macro and is invoked as the last function of the module.

Another macro that has been mentioned before is `MODULE_LICENSE()`. It is used
to tell the kernel what the license this module opts for use. Using functions
exported as GPL by the kernel is prohibited in a module that declares to use a
"Proprietary" license.

`MODULE_AUTHOR()` declares the author of this module, and `MODULE_DESCRIPTION()`
describes what the module does. Information declared in these two macros is
embedded in the final `.ko` binary file as in a separate ELF section and can be
inspected via the `modinfo` command in userspace.

The header file, `linux/module.h`, contains functions and macros essential to
kernel module programming. Every kernel module requires this header file with no
exception. In addition, this header file includes other necessary header files
recursively, which are used during the compiling, loading and running time.

Another noticeable point is using the `__init` and the `__exit` markers. These
two markers give the module loader clues about the memory lifetime of marked
functions. Functions marked with the `__init` marker are one-time used during
the loading time; the loader will release the memory of the functions after
initialization. Similarly, `__exit` marked functions are only used at the end
of the module's lifetime. In our example, function `m_init()` is used only once
during the loading period of the module, and then it fulfilled its mission and
can be removed from memory. The other two marks, `__initdata` and `__exitdata`,
decorate the variables the same way as `__init` and `__exit` to functions.

# ¶ The end
