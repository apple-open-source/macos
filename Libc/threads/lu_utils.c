/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Port and memory management for doing lookups to the lookup server
 * Copyright (C) 1989 by NeXT, Inc.
 */
/*
 * HISTORY
 * 27-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Changed to use bootstrap port instead of service port.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/error.h>
#include <servers/bootstrap.h>

mach_port_t _lu_port = MACH_PORT_NULL;
mach_port_t _ds_port = MACH_PORT_NULL;

static name_t LOOKUP_NAME = "lookup daemon v2";

#ifndef kDSStdMachDSLookupPortName
#define kDSStdMachDSLookupPortName "com.apple.system.DirectoryService.libinfo_v1"
#endif

mach_port_t
_lookupd_port(mach_port_t port)
{
	kern_return_t status;

	if (port != MACH_PORT_NULL)
	{
		status = bootstrap_register(bootstrap_port, LOOKUP_NAME, port);
		if (status != BOOTSTRAP_SUCCESS) abort();

		return port;
	}
	else if ((_lu_port == MACH_PORT_NULL) && (getpid() > 1))
	{
		status = bootstrap_look_up(bootstrap_port, LOOKUP_NAME, &_lu_port);
		if ((status != BOOTSTRAP_SUCCESS) && (status != BOOTSTRAP_UNKNOWN_SERVICE)) _lu_port = MACH_PORT_NULL;
	}

	return _lu_port;
}

/* called as child starts up.  mach ports aren't inherited: trash cache */
void
_lu_fork_child()
{
	_lu_port = MACH_PORT_NULL;
	_ds_port = MACH_PORT_NULL;
}

void
_lu_setport(mach_port_t desired)
{
	if (_lu_port != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), _lu_port);
	_lu_port = desired;
}

int
_lu_running(void)
{
	return ((_lu_port != MACH_PORT_NULL) || (_lookupd_port(MACH_PORT_NULL) != MACH_PORT_NULL));
}

int
_ds_running(void)
{
	kern_return_t status;

	if (_ds_port != MACH_PORT_NULL) return 1;
	
	status = bootstrap_look_up(bootstrap_port, kDSStdMachDSLookupPortName, &_ds_port);
	if ((status != BOOTSTRAP_SUCCESS) && (status != BOOTSTRAP_UNKNOWN_SERVICE)) _ds_port = MACH_PORT_NULL;

	return (_ds_port != MACH_PORT_NULL);
}
