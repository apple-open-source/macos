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

#ifndef __DSATTRIBUTE_H__
#define __DSATTRIBUTE_H__

/*
 * NetInfo record attribute 
 */

#include <NetInfo/dsdata.h>

typedef struct
{
	dsdata *key;
	u_int32_t count;
	dsdata **value;

	u_int32_t retain;
} dsattribute;

dsattribute *dsattribute_alloc(void);
dsattribute *dsattribute_new(dsdata *);
dsattribute *dsattribute_copy(dsattribute *);

dsattribute *dsattribute_retain(dsattribute *);
void dsattribute_release(dsattribute *);

void dsattribute_insert(dsattribute *, dsdata *, u_int32_t);
void dsattribute_append(dsattribute *, dsdata *);
void dsattribute_remove(dsattribute *, u_int32_t);
void dsattribute_merge(dsattribute *, dsdata *);

u_int32_t dsattribute_index(dsattribute *, dsdata *);
dsdata *dsattribute_value(dsattribute *, u_int32_t);
dsdata *dsattribute_key(dsattribute *a);

int dsattribute_match(dsattribute *, dsattribute *);
int dsattribute_equal(dsattribute *, dsattribute *);

void dsattribute_setkey(dsattribute *, dsdata *);

dsdata *dsattribute_to_dsdata(dsattribute *a);
dsattribute *dsdata_to_dsattribute(dsdata *d);

#endif __DSATTRIBUTE_H__
