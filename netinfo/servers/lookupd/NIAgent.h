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
 * NIAgent.h
 * Written by Marc Majka
 */

#import "LUAgent.h"
#import "LUDictionary.h"
#import <netinfo/ni.h>

typedef struct
{
	unsigned long refcount;
	void *ni;
} ni_shared_handle_t;

typedef struct
{
	ni_shared_handle_t *handle;
	unsigned long checksum;
	unsigned long checksum_time;
} ni_data;

@interface NIAgent : LUAgent
{
	unsigned long domain_count;
	unsigned long timeout;
	unsigned long connect_timeout;
	unsigned long latency;
	ni_data *domain;
}

- (void)setSource:(char *)src;
- (BOOL)findDirectory:(char *)path domain:(void **)d nidir:(ni_id *)nid;
- (BOOL)isSecurityEnabledForOption:(char *)option;

@end

