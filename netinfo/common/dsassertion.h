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

#ifndef __DSASSERTION_H__
#define __DSASSERTION_H__

/*
 * NetInfo attribute assertion
 */

#include <NetInfo/dsdata.h>
#include <NetInfo/dsrecord.h>

#define DSA_LESS 0
#define DSA_LESS_OR_EQUAL 1
#define DSA_EQUAL 2
#define DSA_GREATER_OR_EQUAL 3
#define DSA_GREATER 4
#define DSA_APPROX 5
#define DSA_HAS_KEY 6
#define DSA_PREFIX 7
#define DSA_SUBSTR 8
#define DSA_SUFFIX 9
#define DSA_PRECOMPUTED 10

enum Logic3
{
	L3False = 0,
	L3True = 1,
	L3Undefined = 2
};
typedef enum Logic3 Logic3;

typedef struct
{
	int32_t meta;
	dsdata *key;
	dsdata *value;
	int32_t assertion;
	
	u_int32_t retain;
} dsassertion;

dsassertion *dsassertion_new(int32_t , int32_t , dsdata *, dsdata *);

dsassertion *dsassertion_retain(dsassertion *);
void dsassertion_release(dsassertion *);

Logic3 dsassertion_test(dsassertion *, dsrecord *);

#endif __DSASSERTION_H__
