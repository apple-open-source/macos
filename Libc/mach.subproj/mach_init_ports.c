/*
 * @OSF_COPYRIGHT@
 */

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <mach/mach.h>
#include "externs.h"

mach_port_t	bootstrap_port = MACH_PORT_NULL;
mach_port_t	name_server_port = MACH_PORT_NULL;
mach_port_t	environment_port = MACH_PORT_NULL;
mach_port_t	service_port = MACH_PORT_NULL;
mach_port_t	clock_port = MACH_PORT_NULL;
mach_port_t thread_recycle_port = MACH_PORT_NULL;

void
mach_init_ports(void)
{
	mach_port_array_t	ports;
	mach_msg_type_number_t	ports_count;
	kern_return_t		kr;

	/*
	 *	Find those ports important to every task.
	 */
	kr = task_get_special_port(mach_task_self(),
				   TASK_BOOTSTRAP_PORT,
				   &bootstrap_port);
	if (kr != KERN_SUCCESS)
	    return;

	kr = mach_ports_lookup(mach_task_self(), &ports,
			       &ports_count);
	if ((kr != KERN_SUCCESS) ||
	    (ports_count < MACH_PORTS_SLOTS_USED))
	    return;

	name_server_port = ports[NAME_SERVER_SLOT];
	environment_port = ports[ENVIRONMENT_SLOT];
	service_port     = ports[SERVICE_SLOT];

	/* get rid of out-of-line data so brk has a chance of working */

	(void) vm_deallocate(mach_task_self(),
			     (vm_offset_t) ports,
			     (vm_size_t) (ports_count * sizeof *ports));

        /* Get the clock service port for nanosleep */
        kr = host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &clock_port);
        if (kr != KERN_SUCCESS) {
            abort();
        }
        kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &thread_recycle_port);
        if (kr != KERN_SUCCESS) {
            abort();
        }
        kr = mach_port_insert_right(mach_task_self(), thread_recycle_port, thread_recycle_port, MACH_MSG_TYPE_MAKE_SEND);
        if (kr != KERN_SUCCESS) {
            abort();
        }
}

#ifdef notdef
/* will have problems with dylib build --> not needed anyway */
#ifndef	lint
/*
 *	Routines which our library must suck in, to avoid
 *	a later library from referencing them and getting
 *	the wrong version.
 */
extern void _replacements(void);

void
_replacements(void)
{
	(void)sbrk(0);			/* Pull in our sbrk/brk */
	(void)malloc(0);		/* Pull in our malloc package */
}
#endif	/* lint */
#endif /* notdef */
