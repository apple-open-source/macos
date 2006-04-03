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

#ifndef __DSFILTER_H__
#define __DSFILTER_H__

/*
 * NetInfo search filter 
 */

#include <NetInfo/dsassertion.h>

#define DSF_OP_ASSERT 0
#define DSF_OP_AND 1
#define DSF_OP_OR 2
#define DSF_OP_NOT 3

typedef struct dsfilter_s
{
	u_int32_t op;
	dsassertion *assert;
	u_int32_t count;
	struct dsfilter_s **filter;

	u_int32_t retain;
} dsfilter;

dsfilter *dsfilter_new_assert(dsassertion *);
dsfilter *dsfilter_new_composite(u_int32_t);

dsfilter *dsfilter_new_and(void);
dsfilter *dsfilter_new_or(void);
dsfilter *dsfilter_new_not(void);

dsfilter *dsfilter_append_filter(dsfilter *, dsfilter *);

dsfilter *dsfilter_retain(dsfilter *);
void dsfilter_release(dsfilter *);

Logic3 dsfilter_test(dsfilter *, dsrecord *);

#endif __DSFILTER_H__
