/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef __ACSCP_H__
#define __ACSCP_H__

#include "pppd.h"

/*
 * Options.
 */
#define CI_ROUTES		1	/* Remote Routes */
#define	CI_DOMAINS		2	/* Remote DNS Domains */

#define	LATEST_ROUTES_VERSION	1
#define LATEST_DOMAINS_VERSION	1

typedef struct acscp_options {
    bool		neg_routes;
    u_int32_t		routes_version;		/* version for routing data format */
    bool		neg_domains;	
    u_int32_t		domains_version;	/* version for domains format */
} acscp_options;


extern acscp_options acscp_wantoptions[];
extern acscp_options acscp_gotoptions[];
extern acscp_options acscp_allowoptions[];
extern acscp_options acscp_hisoptions[];

extern fsm acscp_fsm[];
extern struct protent acscp_protent;

#endif
