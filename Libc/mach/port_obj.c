/*
 * @OSF_COPYRIGHT@
 */

/*
 * Define a service to map from a kernel-generated port name
 * to server-defined "type" and "value" data to be associated
 * with the port.
 */
#include <mach/port_obj.h>
#include <mach/mach.h>

#define DEFAULT_TABLE_SIZE	(64 * 1024)

struct port_obj_tentry *port_obj_table;
int port_obj_table_size = DEFAULT_TABLE_SIZE;

void port_obj_init(
	int maxsize)
{
	kern_return_t kr;

	kr = vm_allocate(mach_task_self(),
		(vm_offset_t *)&port_obj_table,
		(vm_size_t)(maxsize * sizeof (*port_obj_table)),
		TRUE);
	if (kr != KERN_SUCCESS)
		panic("port_obj_init: can't vm_allocate");
}
