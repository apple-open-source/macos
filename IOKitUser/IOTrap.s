
#include <mach/machine/syscall_sw.h>

#if defined(__i386__)

#undef kernel_trap
#define kernel_trap(trap_name,trap_number,number_args) \
LEAF(_##trap_name,0) ;\
	movl	$##trap_number,%eax	;\
	int		$(MACH_INT)		;\
END(_##trap_name)

#endif /* defined(__i386__) */

kernel_trap(iokit_user_client_trap, -100, 8)
