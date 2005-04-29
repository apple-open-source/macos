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

#ifndef _S_AFPUSERS_H
#define _S_AFPUSERS_H

typedef struct {
    ni_id		dir;
    NIDomain_t *	domain;
    PLCache_t		list;
} AFPUsers_t;

void		AFPUsers_free(AFPUsers_t * users);
boolean_t	AFPUsers_set_password(AFPUsers_t * users, 
				      PLCacheEntry_t * entry,
				      u_char * passwd);
boolean_t	AFPUsers_init(AFPUsers_t * users, NIDomain_t * domain);
boolean_t	AFPUsers_create(AFPUsers_t * users, gid_t gid,
				uid_t start, int count);
void		AFPUsers_print(AFPUsers_t * users);

#endif _S_AFPUSERS_H
