/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2001 Apple Computer, Inc.  All Rights
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
 * Dyna.h
 *
 * Dynamically loaded agents for lookupd
 * Written by Marc Majka
 */

#import "LUAgent.h"
#import <NetInfo/DynaAPI.h>
#import <NetInfo/dsrecord.h>

/* 
	u_int32_t callout_new(void **cdata, char *arg, void *agent);
	u_int32_t callout_free(void *cdata);
	u_int32_t callout_query(void *cdata, dsrecord *pattern, dsrecord **res);
	u_int32_t callout_validate(void *cdata, char *vstring);
*/

@interface Dyna : LUAgent
{
	void *cdata;
	char *shortname;
	dynainfo dyna;

	u_int32_t (*callout_new)(void **, char *, dynainfo *);
	u_int32_t (*callout_free)(void *);
	u_int32_t (*callout_query)(void *, dsrecord *, dsrecord **);
	u_int32_t (*callout_validate)(void *, char *);
}

@end

