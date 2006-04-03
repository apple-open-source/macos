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
 * DynaAPI.h
 *
 * Callback API available to dynamically loaded agents
 * Written by Marc Majka
 */

#include <NetInfo/dsrecord.h>

typedef struct
{
	/* Opaque to the dynamically loaded agent */
	void *d0;
	void *d1;

	/* Caller must release the dsrecord returned by these routines. */
	u_int32_t (*dyna_config_global)(void *, int, dsrecord **);
	u_int32_t (*dyna_config_agent)(void *, int, dsrecord **);

} dynainfo;

/* 
 * Categories
 */
typedef enum
{
	LUCategoryUser,
	LUCategoryGroup,
	LUCategoryHost,
	LUCategoryNetwork,
	LUCategoryService,
	LUCategoryProtocol,
	LUCategoryRpc,
	LUCategoryMount,
	LUCategoryPrinter,
	LUCategoryBootparam,
	LUCategoryBootp,
	LUCategoryAlias,
	LUCategoryNetDomain,
	LUCategoryEthernet,
	LUCategoryNetgroup,
	LUCategoryInitgroups,
	LUCategoryHostServices
} LUCategory;

/* Number of categories above */
#define NCATEGORIES 17

/* Null Category (used for non-lookup dictionaries, e.g. statistics) */
#define LUCategoryNull ((LUCategory)-1)

/*
 * RPC lock
 * use syslock_get(RPCLockName) to get the RPC lock
 */
#define RPCLockName "RPC_lock"

/*
 * Passed to the agent in a query as meta-attributes.
 *
 * CATEGORY_KEY will have a string containing an integer
 * representing the category ("0" for user, "1" for group, etc).
 * SINGLE_KEY will be present if only one record is desired.
 */
#define CATEGORY_KEY "lookup_category"
#define SINGLE_KEY "lookup_single"
#define STAMP_KEY "lookup_stamp"
