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
 * bootstrap -- fundamental service initiator and port server
 * Mike DeMoney, NeXT, Inc.
 * Copyright, 1990.  All rights reserved.
 *
 * bootstrap_internal.h -- global internal data definitions
 */

#import <mach/mach.h>
#import <mach/boolean.h>

#define BASEPRI_USER	10	/* AOF Thu Feb 16 14:42:57 PST 1995 */

#define	BITS_PER_BYTE	8	/* this SHOULD be a well defined constant */
#define	ANYWHERE	TRUE	/* For use with vm_allocate() */

extern const char *program_name;
extern const char *conf_file;
extern const char *default_conf;
extern mach_port_t lookup_only_port;
extern mach_port_t inherited_bootstrap_port;
extern mach_port_t self_port;		/* Compatability hack */
extern boolean_t forward_ok;
extern boolean_t debugging;
extern mach_port_t bootstrap_port_set;
extern int init_priority;
extern boolean_t canReceive(mach_port_t port);
extern boolean_t canSend(mach_port_t port);

extern void msg_destroy_port(mach_port_t p);
