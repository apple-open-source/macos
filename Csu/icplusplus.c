#include <stdlib.h>
#include <mach-o/ldsyms.h>

__private_extern__
int _dyld_func_lookup(
    const char *dyld_func_name,
    unsigned long *address);

/*
 * __initialize_Cplusplus() is a symbols specific to each shared library that
 * can be called in the shared library's initialization routine to force the
 * C++ runtime to be initialized so it can be used.  Shared library 
 * initialization routines are called before C++ static initializers are called
 * so if a shared library's initialization routine depends on them it must make
 * a call to __initialize_Cplusplus() first.
 */
__private_extern__
void
__initialize_Cplusplus(void)
{
    void (*p)(const struct mach_header *);

        _dyld_func_lookup("__dyld_call_module_initializers_for_dylib",
                          (unsigned long *)&p);
	if(p != NULL)
	    p(&_mh_dylib_header);
}
