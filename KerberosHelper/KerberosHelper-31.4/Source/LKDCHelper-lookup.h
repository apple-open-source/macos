/* 
 * Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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

/*! @header	Local KDC Definitions
 *
 * @discussion	This header file describes the functions, callbacks and data structures
 *		that make up the Local KDC (LKDC) implementation
 */

#ifndef H_LKDCHELPER_LOOKUP_H
#define H_LKDCHELPER_LOOKUP_H
	
#include <stdint.h>
#include <time.h>
	
#include "LKDCHelper.h"

typedef struct _LKDCLocator {
        struct _LKDCLocator	*next;
        const char 	        *realmName;
        const char         	*serviceHost;
        uint16_t       		 servicePort;
		uint32_t			ttl;
		time_t				absoluteTTL;
} LKDCLocator;
	
extern	LKDCHelperErrorType	LKDCCreateLocator (LKDCLocator **locator);
extern	LKDCHelperErrorType	LKDCReleaseLocator (LKDCLocator **locator);
extern	LKDCHelperErrorType	LKDCAddLocatorDetails (LKDCLocator *l);
extern	LKDCHelperErrorType	LKDCHostnameForRealm (const char *realm, LKDCLocator **l);
extern	LKDCHelperErrorType	LKDCRealmForHostname (const char *hostname, LKDCLocator **l);
extern	LKDCHelperErrorType LKDCDumpCacheStatus ();

		
#endif  /* H_LKDCHELPER_LOOKUP_H */
