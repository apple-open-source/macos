#ifndef _GLOBALS_H_
#define _GLOBALS_H_

/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include <netinet/in.h>
#include <SystemConfiguration/SCDynamicStore.h>
#include "configthreads_common.h"

extern int					G_verbose;
extern CFBundleRef			G_bundle;
extern SCDynamicStoreRef	G_scd_session;
extern IFStateList_t		G_ifstate_list;
extern int					G_debug;
extern const struct in6_addr	G_ip6_zeroes;

#endif /* _GLOBALS_H_ */
