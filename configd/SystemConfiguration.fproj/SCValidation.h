/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _SCVALIDATION_H
#define _SCVALIDATION_H

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

static __inline__ CFTypeRef
isA_CFType(CFTypeRef obj, CFTypeID type)
{
	if (obj == NULL)
		return (NULL);

	if (CFGetTypeID(obj) != type)
		return (NULL);

	return (obj);
}

static __inline__ CFTypeRef
isA_CFArray(CFTypeRef obj)
{
	return (isA_CFType(obj, CFArrayGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFBoolean(CFTypeRef obj)
{
	return (isA_CFType(obj, CFBooleanGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFData(CFTypeRef obj)
{
	return (isA_CFType(obj, CFDataGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFDate(CFTypeRef obj)
{
	return (isA_CFType(obj, CFDateGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFDictionary(CFTypeRef obj)
{
	return (isA_CFType(obj, CFDictionaryGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFNumber(CFTypeRef obj)
{
	return (isA_CFType(obj, CFNumberGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFPropertyList(CFTypeRef obj)
{
	CFTypeID	type;

	if (obj == NULL)
		return (NULL);

	type = CFGetTypeID(obj);
	if (type == CFArrayGetTypeID()		||
	    type == CFBooleanGetTypeID()	||
	    type == CFDataGetTypeID()		||
	    type == CFDateGetTypeID()		||
	    type == CFDictionaryGetTypeID()	||
	    type == CFNumberGetTypeID()		||
	    type == CFStringGetTypeID())
		return (obj);

	return (NULL);
}


static __inline__ CFTypeRef
isA_CFString(CFTypeRef obj)
{
	return (isA_CFType(obj, CFStringGetTypeID()));
}

__END_DECLS

#endif /* _SCVALIDATION_H */

