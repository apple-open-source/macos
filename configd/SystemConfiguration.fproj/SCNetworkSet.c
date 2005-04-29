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


static CFStringRef	__SCNetworkSetCopyDescription		(CFTypeRef cf);
static void		__SCNetworkSetDeallocate		(CFTypeRef cf);
static Boolean		__SCNetworkSetEqual			(CFTypeRef cf1, CFTypeRef cf2);


static CFTypeID __kSCNetworkSetTypeID	= _kCFRuntimeNotATypeID;


static const CFRuntimeClass __SCNetworkSetClass = {
	0,				// version
	"SCNetworkSet",			// className
	NULL,				// init
	NULL,				// copy
	__SCNetworkSetDeallocate,       // dealloc
	__SCNetworkSetEqual,		// equal
	NULL,				// hash
	NULL,				// copyFormattingDesc
	__SCNetworkSetCopyDescription	// copyDebugDesc
};


static pthread_once_t		initialized	= PTHREAD_ONCE_INIT;


static __inline__ CFTypeRef
isA_SCNetworkSet(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkSetGetTypeID()));
}


static CFStringRef
__SCNetworkSetCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef		allocator	= CFGetAllocator(cf);
	CFMutableStringRef      result;
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCNetworkSet %p [%p]> { "), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("id=%@"), setPrivate->setID);
//	CFStringAppendFormat(result, NULL, CFSTR(", prefs=%@"), setPrivate->prefs);
	CFStringAppendFormat(result, NULL, CFSTR(" }"));

	return result;
}


static void
__SCNetworkSetDeallocate(CFTypeRef cf)
{
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)cf;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCNetworkSetDeallocate:"));

	/* release resources */

	CFRelease(setPrivate->setID);
	CFRelease(setPrivate->prefs);

	return;
}


static Boolean
__SCNetworkSetEqual(CFTypeRef cf1, CFTypeRef cf2)
{
	SCNetworkSetPrivateRef	s1	= (SCNetworkSetPrivateRef)cf1;
	SCNetworkSetPrivateRef	s2	= (SCNetworkSetPrivateRef)cf2;

	if (s1 == s2)
		return TRUE;

	if (s1->prefs != s2->prefs)
		return FALSE;   // if not the same prefs

	if (!CFEqual(s1->setID, s2->setID))
		return FALSE;	// if not the same set identifier

	return TRUE;
}


static void
__SCNetworkSetInitialize(void)
{
	__kSCNetworkSetTypeID = _CFRuntimeRegisterClass(&__SCNetworkSetClass);
	return;
}


static SCNetworkSetPrivateRef
__SCNetworkSetCreatePrivate(CFAllocatorRef      allocator,
			    SCPreferencesRef	prefs,
			    CFStringRef		setID)
{
	SCNetworkSetPrivateRef  setPrivate;
	uint32_t		size;

	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkSetInitialize);

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCNetworkSetCreatePrivate:"));

	/* allocate target */
	size            = sizeof(SCNetworkSetPrivate) - sizeof(CFRuntimeBase);
	setPrivate = (SCNetworkSetPrivateRef)_CFRuntimeCreateInstance(allocator,
								      __kSCNetworkSetTypeID,
								      size,
								      NULL);
	if (setPrivate == NULL) {
		return NULL;
	}

	setPrivate->setID       = CFStringCreateCopy(NULL, setID);
	setPrivate->prefs       = CFRetain(prefs);

	return setPrivate;
}


#define	N_QUICK	16


Boolean
SCNetworkSetAddService(SCNetworkSetRef set, SCNetworkServiceRef service)
{
	SCNetworkInterfaceRef		interface;
	CFArrayRef			interface_config	= NULL;
	CFStringRef			link;
	Boolean				ok;
	CFStringRef			path;
	SCNetworkServicePrivateRef	servicePrivate		= (SCNetworkServicePrivateRef)service;
	SCNetworkSetPrivateRef		setPrivate		= (SCNetworkSetPrivateRef)set;

#define PREVENT_DUPLICATE_SETS
#ifdef  PREVENT_DUPLICATE_SETS
	CFArrayRef			sets;

	// ensure that each service is only a member of ONE set

	sets = SCNetworkSetCopyAll(setPrivate->prefs);
	if (sets != NULL) {
		CFIndex		i;
		CFIndex		n;

		n = CFArrayGetCount(sets);
		for (i = 0; i < n; i++) {
			Boolean		found;
			CFArrayRef      services;
			SCNetworkSetRef set;

			set = CFArrayGetValueAtIndex(sets, i);
			services = SCNetworkSetCopyServices(set);
			found = CFArrayContainsValue(services,
						     CFRangeMake(0, CFArrayGetCount(services)),
						     service);
			CFRelease(services);

			if (found) {
				CFRelease(sets);
				_SCErrorSet(kSCStatusKeyExists);
				return FALSE;
			}
		}
		CFRelease(sets);
	}
#endif  /* PREVENT_DUPLICATE_SETS */

	// get the [deep] interface configuration settings
	interface = SCNetworkServiceGetInterface(service);
	if (interface != NULL) {
		interface_config = __SCNetworkInterfaceCopyDeepConfiguration(interface);
	}

	// create the link between "set" and the "service"
	path = SCPreferencesPathKeyCreateSetNetworkServiceEntity(NULL,				// allocator
								 setPrivate->setID,		// set
								 servicePrivate->serviceID,     // service
								 NULL);				// entity
	link = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      NULL);				// entity
	ok = SCPreferencesPathSetLink(setPrivate->prefs, path, link);
	CFRelease(path);
	CFRelease(link);
	if (!ok) {
		return ok;
	}

	// push the [deep] interface configuration into all sets which contain this service.
	if (interface != NULL) {
		__SCNetworkInterfaceSetDeepConfiguration(interface, interface_config);
	}

	return ok;
}


SCNetworkSetRef
SCNetworkSetCopy(SCPreferencesRef prefs, CFStringRef setID)
{
	CFDictionaryRef		entity;
	CFStringRef		path;
	SCNetworkSetPrivateRef	setPrivate;

	path = SCPreferencesPathKeyCreateSet(NULL, setID);
	entity = SCPreferencesPathGetValue(prefs, path);
	CFRelease(path);

	if (!isA_CFDictionary(entity)) {
		_SCErrorSet(kSCStatusNoKey);
		return NULL;
	}

	setPrivate = __SCNetworkSetCreatePrivate(NULL, prefs, setID);
	return (SCNetworkSetRef)setPrivate;
}


CFArrayRef /* of SCNetworkServiceRef's */
SCNetworkSetCopyAll(SCPreferencesRef prefs)
{
	CFMutableArrayRef	array;
	CFIndex			n;
	CFStringRef		path;
	CFDictionaryRef		sets;

	path = SCPreferencesPathKeyCreateSets(NULL);
	sets = SCPreferencesPathGetValue(prefs, path);
	CFRelease(path);

	if ((sets != NULL) && !isA_CFDictionary(sets)) {
		return NULL;
	}

	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	n = (sets != NULL) ? CFDictionaryGetCount(sets) : 0;
	if (n > 0) {
		CFIndex		i;
		const void *    keys_q[N_QUICK];
		const void **   keys	= keys_q;
		const void *    vals_q[N_QUICK];
		const void **   vals	= vals_q;

		if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
			vals = CFAllocatorAllocate(NULL, n * sizeof(CFPropertyListRef), 0);
		}
		CFDictionaryGetKeysAndValues(sets, keys, vals);
		for (i = 0; i < n; i++) {
			SCNetworkSetPrivateRef	setPrivate;

			if (!isA_CFDictionary(vals[i])) {
				SCLog(TRUE,
				      LOG_INFO,
				      CFSTR("SCNetworkSetCopyAll(): error w/set \"%@\"\n"),
				      keys[i]);
				continue;
			}

			setPrivate = __SCNetworkSetCreatePrivate(NULL, prefs, keys[i]);
			CFArrayAppendValue(array, (SCNetworkSetRef)setPrivate);
			CFRelease(setPrivate);
		}
		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, vals);
		}
	}

	return array;
}


SCNetworkSetRef
SCNetworkSetCopyCurrent(SCPreferencesRef prefs)
{
	CFArrayRef		components;
	CFStringRef		currentID;
	SCNetworkSetPrivateRef	setPrivate	= NULL;

	currentID = SCPreferencesGetValue(prefs, kSCPrefCurrentSet);
	if (!isA_CFString(currentID)) {
		return NULL;
	}

	components = CFStringCreateArrayBySeparatingStrings(NULL, currentID, CFSTR("/"));
	if (CFArrayGetCount(components) == 3) {
		CFStringRef	setID;
		CFStringRef	path;

		setID = CFArrayGetValueAtIndex(components, 2);
		path = SCPreferencesPathKeyCreateSet(NULL, setID);
		if (CFEqual(path, currentID)) {
			setPrivate = __SCNetworkSetCreatePrivate(NULL, prefs, setID);
		} else {
			SCLog(TRUE, LOG_ERR, CFSTR("SCNetworkSetCopyCurrent(): preferences are non-conformant"));
		}
		CFRelease(path);
	}
	CFRelease(components);

	return (SCNetworkSetRef)setPrivate;
}


CFArrayRef /* of SCNetworkServiceRef's */
SCNetworkSetCopyServices(SCNetworkSetRef set)
{
	CFMutableArrayRef       array;
	CFDictionaryRef		dict;
	CFIndex			n;
	CFStringRef		path;
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)set;

	path = SCPreferencesPathKeyCreateSetNetworkService(NULL, setPrivate->setID, NULL);
	dict = SCPreferencesPathGetValue(setPrivate->prefs, path);
	CFRelease(path);
	if ((dict != NULL) && !isA_CFDictionary(dict)) {
		return NULL;
	}

	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	n = (dict != NULL) ? CFDictionaryGetCount(dict) : 0;
	if (n > 0) {
		CFIndex		i;
		const void *    keys_q[N_QUICK];
		const void **   keys	= keys_q;

		if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
		}
		CFDictionaryGetKeysAndValues(dict, keys, NULL);
		for (i = 0; i < n; i++) {
			CFArrayRef	components;
			CFStringRef	link;

			path = SCPreferencesPathKeyCreateSetNetworkServiceEntity(NULL,
										 setPrivate->setID,
										 (CFStringRef)keys[i],
										 NULL);
			link = SCPreferencesPathGetLink(setPrivate->prefs, path);
			CFRelease(path);
			if (link == NULL) {
				SCLog(TRUE,
				      LOG_INFO,
				      CFSTR("SCNetworkSetCopyServices(): service \"%@\" for set \"%@\" is not a link\n"),
				      keys[i],
				      setPrivate->setID);
				continue;	 // if the service is not a link
			}

			components = CFStringCreateArrayBySeparatingStrings(NULL, link, CFSTR("/"));
			if (CFArrayGetCount(components) == 3) {
				CFStringRef serviceID;

				serviceID = CFArrayGetValueAtIndex(components, 2);
				path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,		// allocator
										      serviceID,	// service
										      NULL);		// entity
				if (CFEqual(path, link)) {
					SCNetworkServicePrivateRef	servicePrivate;

					servicePrivate = __SCNetworkServiceCreatePrivate(NULL,
											 serviceID,
											 NULL,
											 setPrivate->prefs);
					CFArrayAppendValue(array, (SCNetworkServiceRef)servicePrivate);
					CFRelease(servicePrivate);
				}
				CFRelease(path);
			}
			CFRelease(components);
		}
		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
		}
	}

	return array;
}


SCNetworkSetRef
SCNetworkSetCreate(SCPreferencesRef prefs)
{
	CFArrayRef		components;
	CFStringRef		path;
	CFStringRef		prefix;
	CFStringRef		setID;
	SCNetworkSetPrivateRef	setPrivate;

	prefix = SCPreferencesPathKeyCreateSets(NULL);
	path = SCPreferencesPathCreateUniqueChild(prefs, prefix);
	CFRelease(prefix);
	if (path == NULL) {
		return NULL;
	}

	components = CFStringCreateArrayBySeparatingStrings(NULL, path, CFSTR("/"));
	CFRelease(path);

	setID = CFArrayGetValueAtIndex(components, 2);
	setPrivate = __SCNetworkSetCreatePrivate(NULL, prefs, setID);
	CFRelease(components);

	return (SCNetworkSetRef)setPrivate;
}


CFStringRef
SCNetworkSetGetSetID(SCNetworkSetRef set)
{
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)set;

	return setPrivate->setID;
}


CFStringRef
SCNetworkSetGetName(SCNetworkSetRef set)
{
	CFDictionaryRef		entity;
	CFStringRef		name		= NULL;
	CFStringRef		path;
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)set;

	path = SCPreferencesPathKeyCreateSet(NULL, setPrivate->setID);
	entity = SCPreferencesPathGetValue(setPrivate->prefs, path);
	CFRelease(path);

	if (isA_CFDictionary(entity)) {
		name = CFDictionaryGetValue(entity, kSCPropUserDefinedName);
	}

	return isA_CFString(name) ? name : NULL;
}


CFArrayRef /* of serviceID CFStringRef's */
SCNetworkSetGetServiceOrder(SCNetworkSetRef set)
{
	CFDictionaryRef		dict;
	CFStringRef		path;
	CFArrayRef		serviceOrder;
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)set;

	path = SCPreferencesPathKeyCreateSetNetworkGlobalEntity(NULL, setPrivate->setID, kSCEntNetIPv4);
	if (path == NULL) {
		return NULL;
	}

	dict = SCPreferencesPathGetValue(setPrivate->prefs, path);
	CFRelease(path);
	if (!isA_CFDictionary(dict)) {
		return NULL;
	}

	serviceOrder = CFDictionaryGetValue(dict, kSCPropNetServiceOrder);
	serviceOrder = isA_CFArray(serviceOrder);

	return serviceOrder;
}


CFTypeID
SCNetworkSetGetTypeID(void)
{
	pthread_once(&initialized, __SCNetworkSetInitialize);	/* initialize runtime */
	return __kSCNetworkSetTypeID;
}


Boolean
SCNetworkSetRemove(SCNetworkSetRef set)
{
	CFStringRef		currentPath;
	Boolean			ok		= FALSE;
	CFStringRef		path;
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)set;

	currentPath = SCPreferencesGetValue(setPrivate->prefs, kSCPrefCurrentSet);
	path = SCPreferencesPathKeyCreateSet(NULL, setPrivate->setID);
	if (!isA_CFString(currentPath) || !CFEqual(currentPath, path)) {
		ok = SCPreferencesPathRemoveValue(setPrivate->prefs, path);
	}
	CFRelease(path);

	return ok;
}


Boolean
SCNetworkSetRemoveService(SCNetworkSetRef set, SCNetworkServiceRef service)
{
	SCNetworkInterfaceRef		interface;
	CFArrayRef			interface_config	= NULL;
	Boolean				ok;
	CFStringRef			path;
	SCNetworkServicePrivateRef	servicePrivate		= (SCNetworkServicePrivateRef)service;
	SCNetworkSetPrivateRef		setPrivate		= (SCNetworkSetPrivateRef)set;

	// get the [deep] interface configuration settings
	interface = SCNetworkServiceGetInterface(service);
	if (interface != NULL) {
		interface_config = __SCNetworkInterfaceCopyDeepConfiguration(interface);
		if (interface_config != NULL) {
			// remove the interface configuration from all sets which contain this service.
			__SCNetworkInterfaceSetDeepConfiguration(interface, NULL);
		}
	}

	// remove the link between "set" and the "service"
	path = SCPreferencesPathKeyCreateSetNetworkServiceEntity(NULL,
								 setPrivate->setID,
								 servicePrivate->serviceID,
								 NULL);
	ok = SCPreferencesPathRemoveValue(setPrivate->prefs, path);
	CFRelease(path);
	if (!ok) {
		goto done;
	}

	// push the [deep] interface configuration [back] into all sets which contain the service.
	if (interface_config != NULL) {
		__SCNetworkInterfaceSetDeepConfiguration(interface, interface_config);
	}

    done :

	if (interface_config != NULL)     CFRelease(interface_config);
	return ok;
}


Boolean
SCNetworkSetSetCurrent(SCNetworkSetRef set)
{
	Boolean			ok;
	CFStringRef		path;
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)set;

	path = SCPreferencesPathKeyCreateSet(NULL, setPrivate->setID);
	ok = SCPreferencesSetValue(setPrivate->prefs, kSCPrefCurrentSet, path);
	CFRelease(path);
	return ok;
}


Boolean
SCNetworkSetSetName(SCNetworkSetRef set, CFStringRef name)
{
	CFDictionaryRef		entity;
	Boolean			ok		= FALSE;
	CFStringRef		path;
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)set;

#define PREVENT_DUPLICATE_SET_NAMES
#ifdef  PREVENT_DUPLICATE_SET_NAMES
	if (isA_CFString(name)) {
		CFArrayRef      sets;

		// ensure that each set is uniquely named

		sets = SCNetworkSetCopyAll(setPrivate->prefs);
		if (sets != NULL) {
			CFIndex		i;
			CFIndex		n;

			n = CFArrayGetCount(sets);
			for (i = 0; i < n; i++) {
				CFStringRef     otherID;
				CFStringRef     otherName;
				SCNetworkSetRef set		= CFArrayGetValueAtIndex(sets, i);

				otherID = SCNetworkSetGetSetID(set);
				if (CFEqual(setPrivate->setID, otherID)) {
					continue;       // skip current set
				}

				otherName = SCNetworkSetGetName(set);
				if ((otherName != NULL) && CFEqual(name, otherName)) {
					// if "name" not unique
					CFRelease(sets);
					_SCErrorSet(kSCStatusKeyExists);
					return FALSE;
				}
			}
			CFRelease(sets);
		}
	}
#endif  /* PREVENT_DUPLICATE_SET_NAMES */

	// update the "name"

	path = SCPreferencesPathKeyCreateSet(NULL, setPrivate->setID);
	entity = SCPreferencesPathGetValue(setPrivate->prefs, path);
	if ((entity == NULL) && (name != NULL)) {
		entity = CFDictionaryCreate(NULL,
					    NULL,
					    NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	}
	if (isA_CFDictionary(entity)) {
		CFMutableDictionaryRef	newEntity;

		newEntity = CFDictionaryCreateMutableCopy(NULL, 0, entity);
		if (isA_CFString(name)) {
			CFDictionarySetValue(newEntity, kSCPropUserDefinedName, name);
		} else {
			CFDictionaryRemoveValue(newEntity, kSCPropUserDefinedName);
		}
		ok = SCPreferencesPathSetValue(setPrivate->prefs, path, newEntity);
		CFRelease(newEntity);
	}
	CFRelease(path);

	return ok;
}


Boolean
SCNetworkSetSetServiceOrder(SCNetworkSetRef set, CFArrayRef newOrder)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef  newDict;
	Boolean			ok;
	CFStringRef		path;
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)set;

	path = SCPreferencesPathKeyCreateSetNetworkGlobalEntity(NULL, setPrivate->setID, kSCEntNetIPv4);
	if (path == NULL) {
		return FALSE;
	}

	dict = SCPreferencesPathGetValue(setPrivate->prefs, path);
	if (dict != NULL) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	CFDictionarySetValue(newDict, kSCPropNetServiceOrder, newOrder);
	ok = SCPreferencesPathSetValue(setPrivate->prefs, path, newDict);
	CFRelease(newDict);
	CFRelease(path);

	return ok;
}
