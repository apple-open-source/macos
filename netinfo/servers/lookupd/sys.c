/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * sys.c
 *
 * Miscellaneous OS functions used to localize system-dependent code.
 *
 * Copyright (c) 1999 Apple Computer, Inc.  All Rights Reserved.
 * Written by Marc Majka
 */

#include "sys.h"
#include <string.h>
#include <mach/boolean.h>
#include <servers/bootstrap.h>

extern sys_port_type _lookupd_port(sys_port_type);

int
sys_server_status(char *name)
{
	kern_return_t status;
	int active;

	status = bootstrap_status(bootstrap_port, name, &active);
	if (status == BOOTSTRAP_UNKNOWN_SERVICE) return 0;
	if (status != KERN_SUCCESS) return -1;

	return active;
}	

/* Returns a server port (with receive rights) */
kern_return_t
sys_create_service(char *name, sys_port_type *p, int restart)
{
	kern_return_t status;
	sys_port_type priv_boot_port, send_port;

	status = bootstrap_check_in(bootstrap_port, name, p);
	if (status == KERN_SUCCESS)
	{
		return status;
	}
	else if (status == BOOTSTRAP_UNKNOWN_SERVICE)
	{
		if (restart != 0)
		{
			status = bootstrap_create_server(bootstrap_port, "/usr/sbin/lookupd", 0, TRUE, &priv_boot_port);
			if (status != KERN_SUCCESS) return status;
		}
		else
		{
			priv_boot_port = bootstrap_port;
		}

		status = bootstrap_create_service(priv_boot_port, name, &send_port);
		if (status != KERN_SUCCESS) return status;

		status = bootstrap_check_in(priv_boot_port, name, p);
	}

	return status;
}

/* Returns an existing server port (only send rights) */
kern_return_t
sys_port_look_up(char *name, sys_port_type *p)
{
	kern_return_t status;

	status = bootstrap_look_up(bootstrap_port, name, p);
	return status;
}	

kern_return_t
sys_destroy_service(char *name)
{
	return bootstrap_check_in(bootstrap_port, name, SYS_PORT_NULL);
}	

kern_return_t 
sys_receive_port_alloc(sys_msg_header_type *h, unsigned int size)
{
	return mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(h->msgh_local_port));
}

kern_return_t 
sys_port_alloc(sys_port_type *p)
{
	return mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, p);
}	

sys_msg_return_type
sys_receive_message(sys_msg_header_type *h, unsigned int size, sys_port_type p, sys_msg_timeout_type t)
{
	unsigned int flags;
	h->msgh_local_port = p;
	h->msgh_size = size;
	flags = MACH_RCV_MSG;
	if (t != 0) flags |= MACH_RCV_TIMEOUT;
	return mach_msg(h, flags, 0, size, p, t, SYS_PORT_NULL);
}

kern_return_t
sys_send_message(sys_msg_header_type *h, sys_msg_timeout_type t)
{
	unsigned int flags;
	flags = MACH_SEND_MSG;
	if (t != 0) flags |= MACH_SEND_TIMEOUT;
	return mach_msg(h, flags, h->msgh_size, 0, SYS_PORT_NULL, t, SYS_PORT_NULL);
}

void
sys_port_free(sys_port_type p)
{
	mach_port_destroy(sys_task_self(), p);
}

kern_return_t
sys_port_extract_receive_right(sys_task_port_type et, sys_port_type ep, sys_port_type *p)
{
	mach_msg_type_name_t type;
	return mach_port_extract_right(et, ep, MACH_MSG_TYPE_MOVE_RECEIVE, p, &type);
}

sys_port_type
lookupd_port(char *name)
{
	sys_port_type p;
	kern_return_t status;

	if (name == NULL) return SYS_PORT_NULL;

	status = bootstrap_look_up(bootstrap_port, name, &p);
	if (status != KERN_SUCCESS) return SYS_PORT_NULL;

	return p;
}
