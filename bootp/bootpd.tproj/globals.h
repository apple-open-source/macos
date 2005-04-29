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

#ifndef _S_GLOBALS_H
#define _S_GLOBALS_H

#import <objc/Object.h>
#include "NIDomain.h"
#include "NICache.h"

extern NICache_t	cache;
extern int		bootp_socket;
extern int		debug;
extern int		detect_other_dhcp_server;
extern NIDomain_t *	ni_local;
extern NIDomainList_t	niSearchDomains;
extern int		quiet;
extern unsigned short	server_priority;
extern u_int16_t	reply_threshold_seconds;
extern u_char		rfc_magic[4];
extern char		server_name[MAXHOSTNAMELEN + 1];
extern id		subnets;
extern char *		testing_control;
extern char		transmit_buffer[];
extern int		verbose;
#endif _S_GLOBALS_H
