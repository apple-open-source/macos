/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include <SystemConfiguration/SystemConfiguration.h>

#include <stdarg.h>

/*
 * SCDKeyCreate*
 * - convenience routines that create a CFString key for an item in the cache
 */

/*
 * Function: SCDKeyCreate
 * Purpose:
 *    Creates a cache key using the given format.
 */
CFStringRef
SCDKeyCreate(CFStringRef fmt, ...)
{
	va_list	args;
	va_start(args, fmt);
	return (CFStringCreateWithFormatAndArguments(NULL,
						     NULL,
						     fmt,
						     args));
}

CFStringRef
SCDKeyCreateNetworkGlobalEntity(CFStringRef domain, CFStringRef entity)
{
	return (CFStringCreateWithFormat(NULL,
					 NULL,
					 CFSTR("%@/%@/%@/%@"),
					 domain,
					 kSCCompNetwork,
					 kSCCompGlobal,
					 entity));
}

CFStringRef
SCDKeyCreateNetworkInterface(CFStringRef domain)
{
	return (CFStringCreateWithFormat(NULL,
					 NULL,
					 CFSTR("%@/%@/%@"),
					 domain,
					 kSCCompNetwork,
					 kSCCompInterface));
}

CFStringRef
SCDKeyCreateNetworkInterfaceEntity(CFStringRef domain,
				   CFStringRef ifname,
				   CFStringRef entity)
{
	if (entity == NULL) {
		return (CFStringCreateWithFormat(NULL,
						 NULL,
						 CFSTR("%@/%@/%@/%@"),
						 domain,
						 kSCCompNetwork,
						 kSCCompInterface,
						 ifname));
	} else {
		return (CFStringCreateWithFormat(NULL,
						 NULL,
						 CFSTR("%@/%@/%@/%@/%@"),
						 domain,
						 kSCCompNetwork,
						 kSCCompInterface,
						 ifname,
						 entity));
	}
}

CFStringRef
SCDKeyCreateNetworkServiceEntity(CFStringRef domain,
				 CFStringRef serviceID,
				 CFStringRef entity)
{
	if (entity == NULL) {
		return (CFStringCreateWithFormat(NULL,
						 NULL,
						 CFSTR("%@/%@/%@/%@"),
						 domain,
						 kSCCompNetwork,
						 kSCCompService,
						 serviceID));
	} else {
		return (CFStringCreateWithFormat(NULL,
						 NULL,
						 CFSTR("%@/%@/%@/%@/%@"),
						 domain,
						 kSCCompNetwork,
						 kSCCompService,
						 serviceID,
						 entity));
	}
}
