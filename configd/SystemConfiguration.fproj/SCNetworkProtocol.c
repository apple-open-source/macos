/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

/*
 * Modification History
 *
 * May 13, 2004		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCNetworkConfigurationInternal.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include "SCNetworkConfiguration.h"
#include "SCNetworkConfigurationInternal.h"

#include <pthread.h>


static CFStringRef	__SCNetworkProtocolCopyDescription	(CFTypeRef cf);
static void		__SCNetworkProtocolDeallocate		(CFTypeRef cf);
static Boolean		__SCNetworkProtocolEqual		(CFTypeRef cf1, CFTypeRef cf2);


const CFStringRef kSCNetworkProtocolTypeAppleTalk       = CFSTR("AppleTalk");
const CFStringRef kSCNetworkProtocolTypeDNS		= CFSTR("DNS");
const CFStringRef kSCNetworkProtocolTypeIPv4		= CFSTR("IPv4");
const CFStringRef kSCNetworkProtocolTypeIPv6		= CFSTR("IPv6");
const CFStringRef kSCNetworkProtocolTypeProxies		= CFSTR("Proxies");


static CFTypeID __kSCNetworkProtocolTypeID	= _kCFRuntimeNotATypeID;


static const CFRuntimeClass __SCNetworkProtocolClass = {
	0,					// version
	"SCNetworkProtocol",			// className
	NULL,					// init
	NULL,					// copy
	__SCNetworkProtocolDeallocate,		// dealloc
	__SCNetworkProtocolEqual,		// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__SCNetworkProtocolCopyDescription	// copyDebugDesc
};


static pthread_once_t		initialized	= PTHREAD_ONCE_INIT;


static __inline__ CFTypeRef
isA_SCNetworkProtocol(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkProtocolGetTypeID()));
}


static CFStringRef
__SCNetworkProtocolCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	CFMutableStringRef		result;
	SCNetworkProtocolPrivateRef	protocolPrivate	= (SCNetworkProtocolPrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCNetworkProtocol %p [%p]> { "), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("id=%@"), protocolPrivate->entityID);
	CFStringAppendFormat(result, NULL, CFSTR(", service=%@"), protocolPrivate->service);
	CFStringAppendFormat(result, NULL, CFSTR(" }"));

	return result;
}


static void
__SCNetworkProtocolDeallocate(CFTypeRef cf)
{
	SCNetworkProtocolPrivateRef	protocolPrivate	= (SCNetworkProtocolPrivateRef)cf;

	/* release resources */
	CFRelease(protocolPrivate->entityID);

	return;
}


static Boolean
__SCNetworkProtocolEqual(CFTypeRef cf1, CFTypeRef cf2)
{
	SCNetworkProtocolPrivateRef	p1	= (SCNetworkProtocolPrivateRef)cf1;
	SCNetworkProtocolPrivateRef	p2	= (SCNetworkProtocolPrivateRef)cf2;

	if (p1 == p2)
		return TRUE;

	if (!CFEqual(p1->entityID, p2->entityID))
		return FALSE;	// if not the same protocol type

	if (p1->service == p2->service)
		return TRUE;    // if both point to the same service

	if ((p1->service != NULL) && (p2->service != NULL) && CFEqual(p1->service, p2->service))
		return TRUE;    // if both effectively point to the same service

	return FALSE;
}


static void
__SCNetworkProtocolInitialize(void)
{
	__kSCNetworkProtocolTypeID = _CFRuntimeRegisterClass(&__SCNetworkProtocolClass);
	return;
}


__private_extern__ SCNetworkProtocolPrivateRef
__SCNetworkProtocolCreatePrivate(CFAllocatorRef		allocator,
				 CFStringRef		entityID,
				 SCNetworkServiceRef	service)
{
	SCNetworkProtocolPrivateRef		protocolPrivate;
	uint32_t				size;

	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkProtocolInitialize);

	/* allocate target */
	size           = sizeof(SCNetworkProtocolPrivate) - sizeof(CFRuntimeBase);
	protocolPrivate = (SCNetworkProtocolPrivateRef)_CFRuntimeCreateInstance(allocator,
									      __kSCNetworkProtocolTypeID,
									      size,
									      NULL);
	if (protocolPrivate == NULL) {
		return NULL;
	}

	protocolPrivate->entityID       = CFStringCreateCopy(NULL, entityID);
	protocolPrivate->service	= service;

	return protocolPrivate;
}


__private_extern__ Boolean
__SCNetworkProtocolIsValidType(CFStringRef protocolType)
{
	int				i;
	static const CFStringRef	*valid_types[]   = {
		&kSCNetworkProtocolTypeAppleTalk,
		&kSCNetworkProtocolTypeDNS,
		&kSCNetworkProtocolTypeIPv4,
		&kSCNetworkProtocolTypeIPv6,
		&kSCNetworkProtocolTypeProxies
	};

	for (i = 0; i < sizeof(valid_types)/sizeof(valid_types[0]); i++) {
		if (CFEqual(protocolType, *valid_types[i])) {
			// if known/valid protocol type
			return TRUE;
		}
	}

	if (CFStringFindWithOptions(protocolType,
				    CFSTR("."),
				    CFRangeMake(0, CFStringGetLength(protocolType)),
				    0,
				    NULL)) {
		// if user-defined protocol type (e.g. com.apple.myProtocol)
		return TRUE;
	}

	return FALSE;
}


static CFStringRef
copyProtocolConfigurationPath(SCNetworkProtocolPrivateRef protocolPrivate)
{
	CFStringRef			path;
	SCNetworkServicePrivateRef      servicePrivate;

	servicePrivate = (SCNetworkServicePrivateRef)protocolPrivate->service;
	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      protocolPrivate->entityID);       // entity
	return path;
}


/* ---------- SCNetworkProtocol APIs ---------- */


CFTypeID
SCNetworkProtocolGetTypeID()
{
	pthread_once(&initialized, __SCNetworkProtocolInitialize);	/* initialize runtime */
	return __kSCNetworkProtocolTypeID;
}


CFDictionaryRef
SCNetworkProtocolGetConfiguration(SCNetworkProtocolRef protocol)
{
	CFDictionaryRef			config;
	CFStringRef			path;
	SCNetworkProtocolPrivateRef	protocolPrivate	= (SCNetworkProtocolPrivateRef)protocol;
	SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)protocolPrivate->service;

	path = copyProtocolConfigurationPath(protocolPrivate);
	config = __getPrefsConfiguration(servicePrivate->prefs, path);
	CFRelease(path);

	return config;
}


Boolean
SCNetworkProtocolGetEnabled(SCNetworkProtocolRef protocol)
{
	Boolean				enabled;
	CFStringRef			path;
	SCNetworkProtocolPrivateRef	protocolPrivate	= (SCNetworkProtocolPrivateRef)protocol;
	SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)protocolPrivate->service;

	path = copyProtocolConfigurationPath(protocolPrivate);
	enabled = __getPrefsEnabled(servicePrivate->prefs, path);
	CFRelease(path);

	return enabled;
}


CFStringRef
SCNetworkProtocolGetProtocolType(SCNetworkProtocolRef protocol)
{
	SCNetworkProtocolPrivateRef	protocolPrivate	= (SCNetworkProtocolPrivateRef)protocol;

	return protocolPrivate->entityID;
}


Boolean
SCNetworkProtocolSetConfiguration(SCNetworkProtocolRef protocol, CFDictionaryRef config)
{
	Boolean				ok;
	CFStringRef			path;
	SCNetworkProtocolPrivateRef	protocolPrivate	= (SCNetworkProtocolPrivateRef)protocol;
	SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)protocolPrivate->service;

	path = copyProtocolConfigurationPath(protocolPrivate);
	ok = __setPrefsConfiguration(servicePrivate->prefs, path, config, TRUE);
	CFRelease(path);

	return ok;
}


Boolean
SCNetworkProtocolSetEnabled(SCNetworkProtocolRef protocol, Boolean enabled)
{
	Boolean				ok;
	CFStringRef			path;
	SCNetworkProtocolPrivateRef	protocolPrivate	= (SCNetworkProtocolPrivateRef)protocol;
	SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)protocolPrivate->service;

	path = copyProtocolConfigurationPath(protocolPrivate);
	ok = __setPrefsEnabled(servicePrivate->prefs, path, enabled);
	CFRelease(path);

	return ok;
}
