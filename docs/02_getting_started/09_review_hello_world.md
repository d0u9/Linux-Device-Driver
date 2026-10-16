# Review Hello World


## Details in code

Our hello world module defines a module initialization function and a module
exit function. The initialization function, registered with `module_init()`,
runs when the module is loaded. The exit function, registered with
`module_exit()`, runs when the module is unloaded. Unlike a user space C
program, a kernel module has no `main()` function. The module must release
the resources it acquires, such as allocated memory and hardware resources;
the exit function is responsible for cleanup when the module is unloaded.

Another macro that has been mentioned before is `MODULE_LICENSE()`. It is used
to tell the kernel which license the module declares. Using symbols exported
with `EXPORT_SYMBOL_GPL()` is prohibited in a module that declares a
"Proprietary" license.

`MODULE_AUTHOR()` declares the author of this module, and `MODULE_DESCRIPTION()`
describes what the module does. Information declared in these two macros is
embedded in the final `.ko` binary file in the `.modinfo` ELF section and can be
inspected via the `modinfo` command in userspace.

The header file, `linux/module.h`, contains functions and macros essential to
kernel module programming. This C example includes it for the module macros.
In addition, this header file includes other necessary header files
recursively, which are used during the compiling, loading and running time.

Another noticeable point is using the `__init` and the `__exit` markers. These
two markers give the module loader clues about the memory lifetime of marked
functions. Functions marked with the `__init` marker are one-time used during
the loading time; the loader will release the memory of the functions after
initialization. Similarly, `__exit` marked functions are only used at the end
of the module's lifetime. In our example, function `m_init()` is used only once
during the loading period of the module, and then it fulfilled its mission and
can be removed from memory. The other two marks, `__initdata` and `__exitdata`,
annotate variables in the same way as `__init` and `__exit` annotate functions.

# ¶ The end
