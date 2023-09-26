/*
 * Copyright (c) 2022-2023 Apple Inc. All rights reserved.
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
 * SCNetworkCategory.c
 * - object to expose per-category service configurations
 */

/*
 * Modification History
 *
 * October 31, 2022		Dieter Siegmund <dieter@apple.com>
 * - initial revision
 */


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <dispatch/dispatch.h>
#include "SCNetworkCategory.h"
#include "SCNetworkConfigurationPrivate.h"
#define __SC_CFRELEASE_NEEDED	1
#include "SCPrivate.h"
#include "SCPreferencesPathKey.h"

const CFStringRef 	kSCNetworkCategoryWiFiSSID = CFSTR("wifi.ssid");

typedef struct __SCNetworkCategory {
	CFRuntimeBase		cfBase;

	CFStringRef		category;
	SCPreferencesRef	prefs;
} SCNetworkCategory;

static CFStringRef 	__SCNetworkCategoryCopyDescription(CFTypeRef cf);
static void	   	__SCNetworkCategoryDeallocate(CFTypeRef cf);
static Boolean		__SCNetworkCategoryEqual(CFTypeRef cf1, CFTypeRef cf2);
static CFHashCode	__SCNetworkCategoryHash(CFTypeRef cf);

static CFTypeID __kSCNetworkCategoryTypeID;

static const CFRuntimeClass SCNetworkCategoryClass = {
	.version = 0,
	.className = "SCNetworkCategory",
	.init = NULL,
	.copy = NULL,
	.finalize = __SCNetworkCategoryDeallocate,
	.equal = __SCNetworkCategoryEqual,
	.hash = __SCNetworkCategoryHash,
	.copyFormattingDesc = NULL,
	.copyDebugDesc = __SCNetworkCategoryCopyDescription
};

static CFStringRef
__SCNetworkCategoryCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef		allocator = CFGetAllocator(cf);
	CFMutableStringRef      result;
	SCNetworkCategoryRef	category = (SCNetworkCategoryRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL,
			     CFSTR("<%s %p [%p]> { ID = %@ }"),
			     SCNetworkCategoryClass.className,
			     category, allocator, category->category);
	return result;
}

static void
__SCNetworkCategoryDeallocate(CFTypeRef cf)
{
	SCNetworkCategoryRef	category = (SCNetworkCategoryRef)cf;

	__SC_CFRELEASE(category->category);
	__SC_CFRELEASE(category->prefs);
	return;
}

static Boolean
__SCNetworkCategoryEqual(CFTypeRef cf1, CFTypeRef cf2)
{
	SCNetworkCategoryRef	c1 = (SCNetworkCategoryRef)cf1;
	SCNetworkCategoryRef	c2 = (SCNetworkCategoryRef)cf2;

	if (c1 == c2) {
		return TRUE;
	}
	return (CFEqual(c1->category, c2->category));
}

static CFHashCode
__SCNetworkCategoryHash(CFTypeRef cf)
{
	SCNetworkCategoryRef	category = (SCNetworkCategoryRef)cf;

	return CFHash(category->category);
}

static void
__SCNetworkCategoryInitialize(void)
{
	static dispatch_once_t  initialized;

	dispatch_once(&initialized, ^{
		__kSCNetworkCategoryTypeID
			= _CFRuntimeRegisterClass(&SCNetworkCategoryClass);
	});

	return;
}

#define __kSCNetworkCategorySize					\
	sizeof(SCNetworkCategory) - sizeof(CFRuntimeBase)

static SCNetworkCategoryRef
__SCNetworkCategoryCreate(SCPreferencesRef prefs,
			  CFStringRef categoryID)
{
	SCNetworkCategoryRef  	category;

	__SCNetworkCategoryInitialize();
	category = (SCNetworkCategoryRef)
		_CFRuntimeCreateInstance(NULL,
					 __kSCNetworkCategoryTypeID,
					 __kSCNetworkCategorySize,
					 NULL);
	if (category == NULL) {
		return NULL;
	}
	category->category = CFStringCreateCopy(NULL, categoryID);
	category->prefs = CFRetain(prefs);
	return (category);
}

static CFDictionaryRef
dict_create_empty(void)
{
	return (CFDictionaryCreate(NULL,
				   NULL, NULL, 0,
				   &kCFTypeDictionaryKeyCallBacks,
				   &kCFTypeDictionaryValueCallBacks));
}

typedef struct {
	SCPreferencesRef	prefs;
	CFMutableArrayRef	list;
} copyAllContext, *copyAllContextRef;

static void
copyAllApplier(const void * key, const void * value, void * _context)
{
	SCNetworkCategoryRef	category;
	copyAllContextRef	context;

	if (isA_CFDictionary(value) == NULL) {
		return;
	}
	context = (copyAllContextRef)_context;
	category = __SCNetworkCategoryCreate(context->prefs, (CFStringRef)key);
	if (context->list == NULL) {
		context->list
			= CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	CFArrayAppendValue(context->list, category);
	CFRelease(category);
}

typedef struct {
	SCPreferencesRef	prefs;
	CFMutableArrayRef	list;
} copyServicesContext, *copyServicesContextRef;

static void
copyServicesApplier(const void * key, const void * value, void * _context)
{
	copyServicesContextRef	context;
	SCNetworkServiceRef	service;

	if (isA_CFDictionary(value) == NULL) {
		return;
	}
	context = (copyServicesContextRef)_context;
	service = SCNetworkServiceCopy(context->prefs, (CFStringRef)key);
	if (service == NULL) {
		/* broken link */
		return;
	}
	if (context->list == NULL) {
		context->list
			= CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	CFArrayAppendValue(context->list, service);
	CFRelease(service);
}

typedef struct {
	CFMutableArrayRef	list;
} copyValuesContext, *copyValuesContextRef;

static void
copyValuesApplier(const void * key, const void * value, void * _context)
{
	copyValuesContextRef	context;

	if (isA_CFDictionary(value) == NULL) {
		return;
	}
	context = (copyValuesContextRef)_context;
	if (context->list == NULL) {
		context->list
			= CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	CFArrayAppendValue(context->list, key);
}

#pragma mark -
#pragma mark SCNetworkCategory APIs

CFTypeID
SCNetworkCategoryGetTypeID(void)
{
	__SCNetworkCategoryInitialize();
	return __kSCNetworkCategoryTypeID;
}

CFArrayRef __nullable
SCNetworkCategoryCopyAll(SCPreferencesRef prefs)
{
	copyAllContext		context;
	CFDictionaryRef		dict;
	CFStringRef		path;

	path = SCPreferencesPathKeyCreateCategories(NULL);
	dict = SCPreferencesPathGetValue(prefs, path);
	CFRelease(path);
	if (dict == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		return (NULL);
	}
	bzero(&context, sizeof(context));
	context.prefs = prefs;
	CFDictionaryApplyFunction(dict, copyAllApplier, &context);
	return (context.list);
}

SCNetworkCategoryRef __nullable
SCNetworkCategoryCreate(SCPreferencesRef prefs, CFStringRef categoryID)
{
	SCNetworkCategoryRef	category;

	category = __SCNetworkCategoryCreate(prefs, categoryID);
	if (category != NULL) {
		SC_log(LOG_DEBUG, "%s(): %@", __func__, category);
	}
	return (category);
}

Boolean
SCNetworkCategoryAddService(SCNetworkCategoryRef category,
			    CFStringRef value,
			    SCNetworkServiceRef service)
{
	CFDictionaryRef	dict;
	Boolean		ok = FALSE;
	CFStringRef	path;
	CFStringRef	serviceID;

	serviceID = SCNetworkServiceGetServiceID(service);
	path = SCPreferencesPathKeyCreateCategoryService(NULL,
							 category->category,
							 value,
							 serviceID);
	dict = SCPreferencesPathGetValue(category->prefs, path);
	if (isA_CFDictionary(dict) != NULL) {
		_SCErrorSet(kSCStatusKeyExists);
		goto done;
	}
	dict = dict_create_empty();
	ok = SCPreferencesPathSetValue(category->prefs, path, dict);
	CFRelease(dict);
 done:
	CFRelease(path);
	return (ok);
}

Boolean
SCNetworkCategoryRemoveService(SCNetworkCategoryRef category,
			       CFStringRef value,
			       SCNetworkServiceRef service)
{
	Boolean		ok = FALSE;
	CFStringRef	path;
	CFStringRef	serviceID;

	serviceID = SCNetworkServiceGetServiceID(service);
	path = SCPreferencesPathKeyCreateCategoryService(NULL,
							 category->category,
							 value,
							 serviceID);
	ok = SCPreferencesPathRemoveValue(category->prefs, path);
	(void)SCNetworkServiceRemove(service);
	CFRelease(path);
	return (ok);
}

CFArrayRef __nullable /* of SCNetworkServiceRef */
SCNetworkCategoryCopyServices(SCNetworkCategoryRef category,
			      CFStringRef value)
{
	copyServicesContext	context;
	CFDictionaryRef		dict;
	CFStringRef		path;

	path = SCPreferencesPathKeyCreateCategoryService(NULL,
							 category->category,
							 value,
							 NULL);
	dict = SCPreferencesPathGetValue(category->prefs, path);
	CFRelease(path);
	if (dict == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		return (NULL);
	}
	bzero(&context, sizeof(context));
	context.prefs = category->prefs;
	CFDictionaryApplyFunction(dict, copyServicesApplier, &context);
	return (context.list);
}

CFArrayRef __nullable /* of CFStringRef */
SCNetworkCategoryCopyValues(SCNetworkCategoryRef category)
{
	copyValuesContext	context;
	CFDictionaryRef		dict;
	CFStringRef		path;

	path = SCPreferencesPathKeyCreateCategory(NULL, category->category);
	dict = SCPreferencesPathGetValue(category->prefs, path);
	CFRelease(path);
	if (dict == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		return (NULL);
	}
	bzero(&context, sizeof(context));
	CFDictionaryApplyFunction(dict, copyValuesApplier, &context);
	return (context.list);
}

Boolean
SCNetworkCategorySetServiceQoSMarkingPolicy(SCNetworkCategoryRef category,
					    CFStringRef value,
					    SCNetworkServiceRef service,
					    CFDictionaryRef __nullable entity)
{
	CFStringRef		path;
	CFStringRef		serviceID;
	Boolean			ok;

	serviceID = SCNetworkServiceGetServiceID(service);
	path = SCPreferencesPathKeyCreateCategoryServiceEntity(NULL,
							       category->category,
							       value,
							       serviceID,
							       kSCEntNetQoSMarkingPolicy);
	if (entity != NULL) {
		ok = SCPreferencesPathSetValue(category->prefs, path, entity);
	}
	else {
		ok = SCPreferencesPathRemoveValue(category->prefs, path);
	}
	CFRelease(path);
	return (ok);
}

CFDictionaryRef
SCNetworkCategoryGetServiceQoSMarkingPolicy(SCNetworkCategoryRef category,
					    CFStringRef value,
					    SCNetworkServiceRef service)
{
	CFDictionaryRef		dict;
	CFStringRef		path;
	CFStringRef		serviceID;

	serviceID = SCNetworkServiceGetServiceID(service);
	path = SCPreferencesPathKeyCreateCategoryServiceEntity(NULL,
							       category->category,
							       value,
							       serviceID,
							       kSCEntNetQoSMarkingPolicy);
	dict = SCPreferencesPathGetValue(category->prefs, path);
	CFRelease(path);
	if (dict == NULL) {
		_SCErrorSet(kSCStatusNoKey);
	}
	return (dict);
}


#ifdef TEST_TRANSFORM

__private_extern__ os_log_t
__log_SCNetworkConfiguration(void)
{
	static os_log_t	log	= NULL;

	if (log == NULL) {
		log = os_log_create("com.apple.SystemConfiguration", "SCNetworkConfiguration");
	}

	return log;
}

static Boolean
transform_and_remove_set(SCPreferencesRef prefs, SCNetworkSetRef set)
{
	SCNetworkCategoryRef	category = NULL;
	CFIndex			count = 0;
	Boolean			ok = FALSE;
	CFStringRef		ssid;
	CFArrayRef		services;
	CFIndex			wifi_count = 0;

	ssid = SCNetworkSetGetName(set);
	if (ssid == NULL) {
		SC_log(LOG_NOTICE, "Set %@ does not have a name", set);
		ok = TRUE; /* remove it anyway */
		goto done;
	}
	services = SCNetworkSetCopyServices(set);
	if (services != NULL) {
		count = CFArrayGetCount(services);
	}
	if (count == 0) {
		ok = TRUE;
		goto done;
	}
	category = SCNetworkCategoryCreate(prefs,
					   kSCNetworkCategoryWiFiSSID);
	for (CFIndex i = 0; i < count; i++) {
		CFStringRef		name;
		SCNetworkInterfaceRef	netif;
		SCNetworkServiceRef	service;
		CFStringRef		type;

		service = (SCNetworkServiceRef)
			CFArrayGetValueAtIndex(services, i);
		netif = SCNetworkServiceGetInterface(service);
		if (netif == NULL) {
			SC_log(LOG_NOTICE, "service %@ has no interface",
			       service);
			ok = FALSE;
			goto done;
		}
		name = SCNetworkInterfaceGetBSDName(netif);
		type = SCNetworkInterfaceGetInterfaceType(netif);
		if (name == NULL
		    || !CFEqual(type, kSCNetworkInterfaceTypeIEEE80211)
		    || !CFStringHasPrefix(name, CFSTR("en"))) {
			SCPrint(TRUE, stdout,
				CFSTR("Set %@ remove service %@\n"),
				set, service);
		}
		else {
			wifi_count++;
			if (wifi_count > 1) {
				SCPrint(TRUE, stdout,
					CFSTR("Set %@ remove %@ (%d)\n"),
					set, service, (int)wifi_count);
			}
			else {
				SCPrint(TRUE, stdout,
					CFSTR("Set %@ preserve %@\n"),
					set, service);
				if (!SCNetworkCategoryAddService(category,
								 ssid,
								 service)) {
					SCPrint(TRUE, stdout,
						CFSTR("AddService %s\n"),
						SCErrorString(SCError()));
					ok = FALSE;
					break;
				}
			}
		}
		SCNetworkSetRemoveService(set, service);
	}
	ok = TRUE;

 done:
	if (ok) {
		SCNetworkSetRemove(set);
	}
	if (category != NULL) {
		CFRelease(category);
	}
	return (ok);
}

static Boolean
transform_sets(SCPreferencesRef prefs, CFArrayRef all, Boolean * has_default)
{
	CFIndex		count;
	Boolean		ok = TRUE;

	*has_default = FALSE;
	count = CFArrayGetCount(all);
	for (CFIndex i = 0; i < count; i++) {
		SCNetworkSetRef		set;

		set = (SCNetworkSetRef)CFArrayGetValueAtIndex(all, i);
		if (_SCNetworkSetIsDefault(set)) {
			*has_default = TRUE;
		}
		else {
			if (!transform_and_remove_set(prefs, set)) {
				ok = FALSE;
				break;
			}
		}
	}
	return (ok);
}

static Boolean
transform(SCPreferencesRef prefs)
{
	CFArrayRef	all;
	Boolean		has_default = FALSE;
	Boolean		ok = FALSE;

	all = SCNetworkSetCopyAll(prefs);
	if (all == NULL) {
		SC_log(LOG_NOTICE, "%@ has no sets", prefs);
	}
	else {
		ok = transform_sets(prefs, all, &has_default);
		if (!has_default) {
			ok = FALSE;
		}
		CFRelease(all);
	}
	return (ok);
}

static CFStringRef
my_CFStringCreate(const char * str)
{
	return CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
}

static CFStringRef
createPath(const char * arg)
{
	char	path[MAXPATHLEN];

	if (arg[0] == '/') {
		return (my_CFStringCreate(arg));
	}
	/* relative path, fully qualify it */
	if (getcwd(path, sizeof(path)) == NULL) {
		fprintf(stderr,
			"Can't get current working directory, %s\n",
			strerror(errno));
		return (NULL);
	}
	return (CFStringCreateWithFormat(NULL, NULL, CFSTR("%s/%s"), path, arg));
}

static void
usage(const char * progname)
{
	fprintf(stderr, "usage: %s <filename>\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	CFStringRef		path;
	SCPreferencesRef	prefs;

	if (argc != 2) {
		usage(argv[0]);
	}
	path = createPath(argv[1]);
	prefs = SCPreferencesCreate(NULL, CFSTR("SCNetworkCategory"), path);
	if (prefs == NULL) {
		fprintf(stderr, "failed to create preferences\n");
		exit(1);
	}
	if (!transform(prefs)) {
		fprintf(stderr, "transformation failed\n");
		exit(1);
	}
	SCPreferencesCommitChanges(prefs);
	printf("transformation succeeded!\n");
	exit(0);
}

#endif /* TEST_TRANSFORM */
