/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * nbsp.h
 * - NetBoot SharePoint routines
 */

#ifndef _S_NBSP_H
#define _S_NBSP_H

typedef struct {
    char *	name;
    char *	path;
} NBSPEntry, * NBSPEntryRef;

struct NBSPList_s;

typedef struct NBSPList_s * NBSPListRef;

int		NBSPList_count(NBSPListRef list);
NBSPEntryRef	NBSPList_element(NBSPListRef list, int i);
void		NBSPList_print(NBSPListRef list);
void		NBSPList_free(NBSPListRef * list);
NBSPListRef	NBSPList_init(const char * symlink_name);

#endif _S_NBSP_H
