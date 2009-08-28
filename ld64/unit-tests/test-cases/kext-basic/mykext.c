#include <mach/mach_types.h>


int my_global = 3;
extern int extern_global;

kern_return_t mykext_start (kmod_info_t * ki, void * d) {
	++my_global;
	++extern_global;
    return KERN_SUCCESS;
}


kern_return_t mykext_stop (kmod_info_t * ki, void * d) {
	--my_global;
	--extern_global;
    return KERN_SUCCESS;
}
