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

#ifdef _OS_VERSION_MACOS_X_
static char *LOOKUPD_NAME = "lookup daemon";
#endif
extern sys_port_type _lookupd_port(sys_port_type);

#ifdef _PORT_REGISTRY_BOOTSTRAP_
#include <servers/bootstrap.h>
#else
#include <servers/netname.h>
#endif

int
sys_server_running(char *name)
{
	kern_return_t status;
#ifdef _PORT_REGISTRY_BOOTSTRAP_
	boolean_t active;
#else
	sys_port_type p;
#endif

#ifdef _PORT_REGISTRY_BOOTSTRAP_
	status = bootstrap_status(bootstrap_port, name, &active);
	if ((status == KERN_SUCCESS) && active) return 1;
#else
	status = netname_look_up(name_server_port, "", name, &p);
	if (status == KERN_SUCCESS) return 1;
#endif

	return 0;
}	

/* Returns a server port (with receive rights) */
kern_return_t
sys_create_service(char *name, sys_port_type *p)
{
	kern_return_t status;
#ifdef _PORT_REGISTRY_BOOTSTRAP_
	boolean_t active;
#endif

#ifdef _PORT_REGISTRY_BOOTSTRAP_
	/* Check if service exists */
	status = bootstrap_status(bootstrap_port, name, &active);
	if (status == KERN_SUCCESS)
	{
		if (active) return KERN_FAILURE;
	}
	else
	{
		/* Service must be created */
		status = bootstrap_create_service(bootstrap_port, name, p);
		if (status != KERN_SUCCESS) return status;
	}

	/* Check in */
	status = bootstrap_check_in(bootstrap_port, name, p);
#else
	status = netname_look_up(name_server_port, "", name, p);
	if (status == KERN_SUCCESS) return status;

	status = sys_port_alloc(p);
	if (status != KERN_SUCCESS) return status;

	status = netname_check_in(name_server_port, name, PORT_NULL, *p);
#endif

	return status;
}

/* Returns an existing server port (only send rights) */
kern_return_t
sys_port_look_up(char *name, sys_port_type *p)
{
	kern_return_t status;

#ifdef _PORT_REGISTRY_BOOTSTRAP_
	if (!sys_server_running(name)) return KERN_FAILURE;
	status = bootstrap_look_up(bootstrap_port, name, p);
#else
	status = netname_look_up(name_server_port, "", name, p);
#endif

	return status;
}	

kern_return_t
sys_destroy_service(char *name)
{
#ifdef _PORT_REGISTRY_BOOTSTRAP_
	return bootstrap_check_in(bootstrap_port, name, SYS_PORT_NULL);
#else
	return netname_check_out(name_server_port, name, SYS_PORT_NULL);
#endif

	return KERN_SUCCESS;
}	

kern_return_t 
sys_receive_port_alloc(sys_msg_header_type *h, unsigned int size)
{
#ifdef _IPC_UNTYPED_
	return mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(h->msgh_local_port));
#else
	h->msg_size = size;
	return port_allocate(task_self(), &(h->msg_local_port));
#endif
}

kern_return_t 
sys_port_alloc(sys_port_type *p)
{
#ifdef _IPC_UNTYPED_
	return mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, p);
#else
	return port_allocate(task_self(), p);
#endif
}	

sys_msg_return_type
sys_receive_message(sys_msg_header_type *h, unsigned int size, sys_port_type p, sys_msg_timeout_type t)
{
	unsigned int flags;
#ifdef _IPC_UNTYPED_
	h->msgh_local_port = p;
	h->msgh_size = size;
	flags = MACH_RCV_MSG;
	if (t != 0) flags |= MACH_RCV_TIMEOUT;
	return mach_msg(h, flags, 0, size, p, t, SYS_PORT_NULL);
#else
	flags = MSG_OPTION_NONE;
	if (t != 0) flags = RCV_TIMEOUT;
	h->msg_local_port = p;
	h->msg_size = size;
	return msg_receive(h, flags, t);
#endif
}

kern_return_t
sys_send_message(sys_msg_header_type *h, sys_msg_timeout_type t)
{
	unsigned int flags;
#ifdef _IPC_UNTYPED_
	flags = MACH_SEND_MSG;
	if (t != 0) flags |= MACH_SEND_TIMEOUT;
	return mach_msg(h, flags, h->msgh_size, 0, SYS_PORT_NULL, t, SYS_PORT_NULL);
#else
	flags = SEND_NOTIFY;
	if (t != 0) flags |= SEND_TIMEOUT;
	return msg_send(h, flags, t);

#endif
}

void
sys_port_free(sys_port_type p)
{
#ifdef _IPC_UNTYPED_
	mach_port_deallocate(sys_task_self(), p);
#else
	port_deallocate(sys_task_self(), p);
#endif
}

kern_return_t
sys_port_extract_receive_right(sys_task_port_type et, sys_port_type ep, sys_port_type *p)
{
#ifdef _IPC_UNTYPED_
	mach_msg_type_name_t type;
	return mach_port_extract_right(et, ep, MACH_MSG_TYPE_MOVE_RECEIVE, p, &type);
#else
	return port_extract_receive(et, ep, p);
#endif
}

sys_port_type
lookupd_port(char *name)
{
	sys_port_type p;

	if (name == NULL) return SYS_PORT_NULL;

	if (!strcmp(name, "lookupd"))
	{
#ifdef _OS_VERSION_MACOS_X_
		if (sys_port_look_up(LOOKUPD_NAME, &p) == KERN_SUCCESS) return p;
		return SYS_PORT_NULL;
#else
		return _lookupd_port(0);
#endif
	}

	if (sys_port_look_up(name, &p) == KERN_SUCCESS) return p;
	return SYS_PORT_NULL;
}
