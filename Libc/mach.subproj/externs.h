/*
 * @OSF_COPYRIGHT@
 */

#include <mach/boolean.h>
#include <mach/error.h>
#include <mach/message.h>
#include <mach/vm_types.h>

extern void mig_init(void *);
extern void mach_init_ports(void);
extern void mig_allocate(vm_address_t *, vm_size_t);
extern void mig_deallocate(vm_address_t, vm_size_t);

extern char *mach_error_string_int(mach_error_t, boolean_t *);
