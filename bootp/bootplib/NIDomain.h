
#ifndef _S_NIDOMAIN_H
#define _S_NIDOMAIN_H

/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * NIDomain.h
 * - simple (C) object representing a netinfo domain
 */

/*
 * Modification History:
 * 
 * May 20, 1998	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 * March 14, 2000	Dieter Siegmund (dieter@apple.com)
 * - converted to straight C
 */

#include <mach/boolean.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinfo/ni.h>
#include <netinfo/ni_util.h>
#include "dynarray.h"

#define NI_DOMAIN_LOCAL		"."
#define NI_DOMAIN_PARENT	".."

typedef struct NIDomain {
    void *		handle;
    char *		name; 		/* path or host/tag */
    struct sockaddr_in	sockaddr;
    ni_name		tag;
    boolean_t		is_master;
} NIDomain_t;
    
NIDomain_t *
NIDomain_init(ni_name domain_name);

NIDomain_t *
NIDomain_parent(NIDomain_t * domain);

struct in_addr
NIDomain_ip(NIDomain_t * domain);

ni_name
NIDomain_tag(NIDomain_t * domain);

void *
NIDomain_handle(NIDomain_t * domain);

ni_name
NIDomain_name(NIDomain_t * domain);

void
NIDomain_set_master(NIDomain_t * domain, boolean_t master);

boolean_t
NIDomain_is_master(NIDomain_t * domain);

void
NIDomain_free(NIDomain_t * domain);


/* 
 * NIDomainList:
 */

typedef struct {
    dynarray_t	domains;
} NIDomainList_t;

void
NIDomainList_init(NIDomainList_t * list);

void
NIDomainList_free(NIDomainList_t * list);

NIDomain_t *
NIDomainList_element(NIDomainList_t * list, int i);

void
NIDomainList_add(NIDomainList_t * list, NIDomain_t * domain);

int
NIDomainList_count(NIDomainList_t * list);

NIDomain_t *
NIDomainList_find(NIDomainList_t * list, NIDomain_t * domain);

#endif _S_NIDOMAIN_H
