/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * This file exist only so that the api of _dyld_event_server() can be put in
 * to libsys.  The problem is that the use of _dyld_event_server() requires the
 * user to define _dyld_event_server_callback().  Which means that the symbol
 * _dyld_event_server_callback() would be undefined in libsys.  Since this
 * would prevent libsys from being prebound the direct reference to the
 * undefined symbol must be eliminated.  This is normally done just by changing
 * the reference to the symbol to be used indirectly via a call to
 * _dyld_lookup_and_bind() to get the symbol's address.  The problem with the
 * reference to _dyld_event_server_callback() is that it is called by mig
 * generated code.  So to get this all working a __private_extern__ version of
 * _dyld_event_server_callback() is declared here which simply does a
 * _dyld_lookup_and_bind() on the global symbol "__dyld_event_server_callback"
 * and calls it.  This object module is then linked with the mig generated
 * object module and the resulting object module is what is put into libsys.
 */
#import <stdio.h>
#import <mach/mach.h>
#import <mach-o/dyld.h>

/* this header file is created by mig */
#import "_dyld_debug.h"

/*
 * _dyld_event_server_callback() is the server side of dyld_event() and is
 * passed the event.  _dyld_event_server_callback() is called indirectly
 * through the mig generated function _dyld_event_server() which is called
 * by the message receive loop in server_loop().
 */
__private_extern__
#ifdef __MACH30__
kern_return_t
#else
void
#endif
_dyld_event_server_callback(
#ifdef __MACH30__
mach_port_t subscriber,
#else
port_t subscriber,
#endif
struct dyld_event event)
{
    void (*p)(mach_port_t subscriber, struct dyld_event event);

	_dyld_lookup_and_bind("__dyld_event_server_callback",
		(unsigned long *)&p, NULL);
	p(subscriber, event);
#ifdef __MACH30__
	return(KERN_SUCCESS);
#endif
}
