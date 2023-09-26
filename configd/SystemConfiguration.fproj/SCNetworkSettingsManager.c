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
 * SCNetworkSettings.c
 * - higher-level network settings API
 */
/*
 * Modification History
 *
 * December 5, 2022		Dieter Siegmund <dieter@apple.com>
 * - initial revision
 */


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBundlePriv.h>
#include "CategoryManagerPrivate.h"
#define __SC_CFRELEASE_NEEDED	1
#include "SCPrivate.h"
#include "SCNetworkCategory.h"
#include "SCNetworkCategoryManagerInternal.h"
#include "SCNetworkConfigurationInternal.h"
#include "SCPreferencesPathKey.h"
#include "SCNetworkConfigurationPrivate.h"
#include "SCNetworkSettingsManager.h"
#include <SystemConfiguration/SystemConfiguration.h>

static void
__SCNetworkSettingsInitialize(void);

/*
 * SCNSManager object glue
 */
typedef struct __SCNSManager {
	CFRuntimeBase		cfBase;

	SCPreferencesRef	prefs;

	dispatch_queue_t	queue;
	SCNSManagerEventHandler	handler;
	SCDynamicStoreRef	store;

	CFMutableSetRef		changes;
	CFMutableSetRef		removals;
} SCNSManager;

static CFStringRef 	__SCNSManagerCopyDescription(CFTypeRef cf);
static void	   	__SCNSManagerDeallocate(CFTypeRef cf);

static CFTypeID __kSCNSManagerTypeID;

static const CFRuntimeClass SCNSManagerClass = {
	.version = 0,
	.className = "SCNSManager",
	.init = NULL,
	.copy = NULL,
	.finalize = __SCNSManagerDeallocate,
	.equal = NULL,
	.hash = NULL,
	.copyFormattingDesc = NULL,
	.copyDebugDesc = __SCNSManagerCopyDescription
};

static CFStringRef
__SCNSManagerCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef		allocator = CFGetAllocator(cf);
	SCNSManagerRef		manager = (SCNSManagerRef)cf;
	CFMutableStringRef      result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL,
			     CFSTR("<%s %p> {}"),
			     SCNSManagerClass.className,
			     manager);
	return result;
}

static void
__SCNSManagerDeallocate(CFTypeRef cf)
{
	SCNSManagerRef	manager = (SCNSManagerRef)cf;

	if (manager->prefs != NULL) {
		SCPreferencesSetDispatchQueue(manager->prefs, NULL);
		SCPreferencesSetCallback(manager->prefs, NULL, NULL);
		__SC_CFRELEASE(manager->prefs);
	}
	__SC_CFRELEASE(manager->changes);
	__SC_CFRELEASE(manager->removals);
	__SC_CFRELEASE(manager->store);
	if (manager->queue != NULL) {
		dispatch_release(manager->queue);
		manager->queue = NULL;
	}
	if (manager->handler != NULL) {
		Block_release(manager->handler);
		manager->handler = NULL;
	}
	return;
}

#define __kSCNSManagerSize					\
	sizeof(SCNSManager) - sizeof(CFRuntimeBase)

static SCNSManagerRef
__SCNSManagerCreate(SCPreferencesRef prefs)
{
	SCNSManagerRef  	manager;

	__SCNetworkSettingsInitialize();
	manager = (SCNSManagerRef)
		_CFRuntimeCreateInstance(NULL,
					 __kSCNSManagerTypeID,
					 __kSCNSManagerSize,
					 NULL);
	if (manager == NULL) {
		return NULL;
	}
	CFRetain(prefs);
	manager->prefs = prefs;
	return (manager);
}

static CFMutableSetRef
__SCNSManagerGetChanges(SCNSManagerRef manager)
{
	if (manager->changes == NULL) {
		manager->changes
			= CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	}
	return (manager->changes);
}

static CFMutableSetRef
__SCNSManagerGetRemovals(SCNSManagerRef manager)
{
	if (manager->removals == NULL) {
		manager->removals
			= CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	}
	return (manager->removals);
}

/*
 * SCNSService object glue
 */
typedef struct __SCNSService {
	CFRuntimeBase		cfBase;

	SCNSManagerRef		manager;
	SCNetworkServiceRef	service;
	SCNetworkInterfaceRef	netif;

	CFStringRef		set_id;
	
	CFStringRef		category_id;
	CFStringRef		category_value;

	CFDictionaryRef		state;

	CFMutableDictionaryRef	changes;
	CFMutableSetRef		removals;
	Boolean			set_to_default;
} SCNSService;

static CFStringRef 	__SCNSServiceCopyDescription(CFTypeRef cf);
static void	   	__SCNSServiceDeallocate(CFTypeRef cf);

static CFTypeID __kSCNSServiceTypeID;

static const CFRuntimeClass SCNSServiceClass = {
	.version = 0,
	.className = "SCNSService",
	.init = NULL,
	.copy = NULL,
	.finalize = __SCNSServiceDeallocate,
	.equal = NULL,
	.hash = NULL,
	.copyFormattingDesc = NULL,
	.copyDebugDesc = __SCNSServiceCopyDescription
};

static CFStringRef
__SCNSServiceCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef		allocator = CFGetAllocator(cf);
	SCNSServiceRef		service = (SCNSServiceRef)cf;
	CFMutableStringRef      result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL,
			     CFSTR("<%s %p> { service %@"),
			     SCNSServiceClass.className,
			     service, service->service);
	if (service->category_id != NULL && service->category_value != NULL) {
		CFStringAppendFormat(result, NULL,
				     CFSTR(" category (%@, %@)"),
				     service->category_id,
				     service->category_value);
	}
	CFStringAppend(result, CFSTR(" }"));
	return result;
}

static void
__SCNSServiceDeallocate(CFTypeRef cf)
{
	SCNSServiceRef	service = (SCNSServiceRef)cf;

	__SC_CFRELEASE(service->manager);
	__SC_CFRELEASE(service->service);
	__SC_CFRELEASE(service->netif);
	__SC_CFRELEASE(service->set_id);
	__SC_CFRELEASE(service->category_id);
	__SC_CFRELEASE(service->category_value);
	__SC_CFRELEASE(service->state);
	__SC_CFRELEASE(service->changes);
	__SC_CFRELEASE(service->removals);
	return;
}

#define __kSCNSServiceSize					\
	sizeof(SCNSService) - sizeof(CFRuntimeBase)

static SCNSServiceRef
__SCNSServiceCreate(SCNSManagerRef manager,
		    CFStringRef __nullable category_id,
		    CFStringRef __nullable category_value,
		    SCNetworkInterfaceRef netif,
		    SCNetworkServiceRef s)
{
	SCNSServiceRef  	service;

	__SCNetworkSettingsInitialize();
	service = (SCNSServiceRef)
		_CFRuntimeCreateInstance(NULL,
					 __kSCNSServiceTypeID,
					 __kSCNSServiceSize,
					 NULL);
	if (service == NULL) {
		return (NULL);
	}
	service->manager = manager;
	CFRetain(manager);
	if (category_id != NULL && category_value != NULL) {
		service->category_id
			= CFStringCreateCopy(NULL, category_id);
		service->category_value
			= CFStringCreateCopy(NULL, category_value);
	}
	if (netif != NULL) {
		CFRetain(netif);
		service->netif = netif;
	}
	if (s != NULL) {
		CFRetain(s);
		service->service = s;
	}
	return (service);
}

static void
__SCNetworkSettingsInitialize(void)
{
	static dispatch_once_t  initialized;

	dispatch_once(&initialized, ^{
		__kSCNSManagerTypeID
			= _CFRuntimeRegisterClass(&SCNSManagerClass);
		__kSCNSServiceTypeID
			= _CFRuntimeRegisterClass(&SCNSServiceClass);
	});

	return;
}

/*
 * Utility
 */
static SCNetworkServiceRef
copy_service_in_list_by_ID(CFArrayRef services, CFStringRef serviceID)
{
	SCNetworkServiceRef	service = NULL;

	for (CFIndex i = 0, count = CFArrayGetCount(services);
	     i < count; i++) {
		SCNetworkServiceRef	s;
		CFStringRef		this_serviceID;

		s = (SCNetworkServiceRef)CFArrayGetValueAtIndex(services, i);
		this_serviceID = SCNetworkServiceGetServiceID(s);
		if (CFEqual(serviceID, this_serviceID)) {
			CFRetain(s);
			service = s;
			break;
		}
	}
	return (service);
}

static SCNetworkServiceRef
copy_service_in_list(CFArrayRef services,
		     SCNetworkInterfaceRef netif)
{
	CFIndex			count;
	SCNetworkServiceRef	service = NULL;

	count = CFArrayGetCount(services);
	for (CFIndex i = 0; i < count; i++) {
		SCNetworkServiceRef	s;
		SCNetworkInterfaceRef	this_netif;

		s = (SCNetworkServiceRef)CFArrayGetValueAtIndex(services, i);
		this_netif = SCNetworkServiceGetInterface(s);
		if (CFEqual(netif, this_netif)) {
			CFRetain(s);
			service = s;
			break;
		}
	}
	return (service);
}

static SCNetworkServiceRef
copy_service_in_set(SCNetworkSetRef set,
		    SCNetworkInterfaceRef netif)
{
	SCNetworkServiceRef	service = NULL;
	CFArrayRef		services;

	services = SCNetworkSetCopyServices(set);
	if (services != NULL) {
		service = copy_service_in_list(services, netif);
		CFRelease(services);
	}
	return (service);
}

static SCNetworkServiceRef
copy_service_with_ID_in_set(SCNetworkSetRef set, CFStringRef serviceID)
{
	SCNetworkServiceRef	service = NULL;
	CFArrayRef		services;

	services = SCNetworkSetCopyServices(set);
	if (services != NULL) {
		service = copy_service_in_list_by_ID(services, serviceID);
		CFRelease(services);
	}
	return (service);
}

static Boolean
remove_service_with_ID_in_set(SCNetworkSetRef set, CFStringRef serviceID)
{
	Boolean			ok = FALSE;
	SCNetworkServiceRef	service = NULL;
	CFArrayRef		services;

	services = SCNetworkSetCopyServices(set);
	if (services != NULL) {
		service = copy_service_in_list_by_ID(services, serviceID);
		CFRelease(services);
	}
	if (service != NULL) {
		ok = SCNetworkSetRemoveService(set, service);
	}
	return (ok);
}

static SCNetworkServiceRef
copy_service_for_category(SCNetworkCategoryRef category,
			  CFStringRef category_value,
			  SCNetworkInterfaceRef netif)
{
	SCNetworkServiceRef	service = NULL;
	CFArrayRef		services;

	services = SCNetworkCategoryCopyServices(category, category_value);
	if (services != NULL) {
		service = copy_service_in_list(services, netif);
		CFRelease(services);
	}
	return (service);
}

static void
state_dict_entity_key(const void * key, const void * value, void * context)
{
	CFIndex			count;
	CFArrayRef		entities;
	CFMutableDictionaryRef	new_state = (CFMutableDictionaryRef)context;

	entities = CFStringCreateArrayBySeparatingStrings(NULL,
							  (CFStringRef)key,
							  CFSTR("/"));
	if (entities == NULL) {
		return;
	}
	count = CFArrayGetCount(entities);
	if (count > 0) {
		CFStringRef		entity;

		entity = CFArrayGetValueAtIndex(entities, count - 1);
		CFDictionarySetValue(new_state, entity, value);
	}
	__SC_CFRELEASE(entities);
	return;
}

static CF_RETURNS_RETAINED CFDictionaryRef 
state_dict_replace_keys(CFDictionaryRef state)
{
	CFMutableDictionaryRef	new_state;

	new_state = CFDictionaryCreateMutable(NULL, 0,
					      &kCFTypeDictionaryKeyCallBacks,
					      &kCFTypeDictionaryValueCallBacks);
	CFDictionaryApplyFunction(state, state_dict_entity_key, new_state);
	return (new_state);
}

static void
dict_merge_value(const void * key, const void * value, void * context)
{
	CFMutableDictionaryRef	merge = (CFMutableDictionaryRef)context;

	CFDictionarySetValue(merge, key, value);
}

static CFDictionaryRef
copy_merged_state_and_setup(CFDictionaryRef state, CFDictionaryRef setup)
{
	CFDictionaryRef		active;

	if (state != NULL && setup != NULL) {
		CFMutableDictionaryRef	merge;

		merge = CFDictionaryCreateMutableCopy(NULL, 0, state);
		CFDictionaryApplyFunction(setup, dict_merge_value, merge);
		active = merge;
	}
	else if (setup != NULL) {
		CFRetain(setup);
		active = setup;
	}
	else if (state != NULL) {
		CFRetain(state);
		active = state;
	}
	else {
		active = NULL;
	}
	return (active);
}

static SCNetworkServiceRef
create_service_in_set(SCPreferencesRef prefs,
		      SCNetworkSetRef set, SCNetworkInterfaceRef netif)
{
	SCNetworkServiceRef	ret_service = NULL;
	SCNetworkServiceRef	service;

	service = SCNetworkServiceCreate(prefs, netif);
	if (!SCNetworkSetAddService(set, service)) {
		CFRelease(service);
		SC_log(LOG_NOTICE, "%s: failed to add service to set, %s",
		       __func__, SCErrorString(SCError()));
	}
	else {
		ret_service = service;
	}
	return (ret_service);
}

static SCNetworkServiceRef
copy_service_for_category_and_ID(SCNetworkCategoryRef category,
				 CFStringRef category_value,
				 CFStringRef serviceID)
{
	SCNetworkServiceRef	service = NULL;
	CFArrayRef		services;

	services = SCNetworkCategoryCopyServices(category, category_value);
	if (services != NULL) {
		service = copy_service_in_list_by_ID(services, serviceID);
		CFRelease(services);
	}
	return (service);
}

static SCNetworkServiceRef
create_service_in_category(SCPreferencesRef prefs,
			   CFStringRef category_id,
			   CFStringRef category_value,
			   SCNetworkInterfaceRef netif)
{
	SCNetworkCategoryRef	category;
	SCNetworkServiceRef	ret_service = NULL;
	SCNetworkServiceRef	service;

	category = SCNetworkCategoryCreate(prefs, category_id);
	service = SCNetworkServiceCreate(prefs, netif);
	if (!SCNetworkCategoryAddService(category, category_value, service)) {
		SC_log(LOG_NOTICE,
		       "%s: SCNetworkCategoryAddService failed, %s",
		       __func__, SCErrorString(SCError()));
		CFRelease(service);
	}
	else {
		ret_service = service;
	}
	CFRelease(category);
	return (ret_service);
}

/*
 * Internal functions
 */
static CFDictionaryRef
__SCNSServiceCopyState(SCNSServiceRef service)
{
	CFStringRef		pattern;
	CFArrayRef		patterns;
	CFDictionaryRef		ret_state = NULL;
	CFStringRef		serviceID;
	CFDictionaryRef		state;

	serviceID = SCNetworkServiceGetServiceID(service->service);
	pattern = 
		SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
					    kSCDynamicStoreDomainState,
					    serviceID,
					    kSCCompAnyRegex);
	patterns = CFArrayCreate(NULL, (const void * *)&pattern, 1,
				 &kCFTypeArrayCallBacks);
	CFRelease(pattern);
	state = SCDynamicStoreCopyMultiple(NULL, NULL, patterns);
	CFRelease(patterns);
	if (state != NULL) {
		ret_state = state_dict_replace_keys(state);
		CFRelease(state);
	}
	return (ret_state);
}

static CFMutableDictionaryRef
__SCNSServiceGetChanges(SCNSServiceRef service)
{
	if (service->changes == NULL) {
		service->changes
			= CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
	}
	return (service->changes);
}

static CFMutableSetRef
__SCNSServiceGetRemovals(SCNSServiceRef service)
{
	if (service->removals == NULL) {
		service->removals
			= CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	}
	return (service->removals);
}

typedef struct {
	Boolean		failed;
} managerContext, *managerContextRef;

static void
managerRemove(const void * value, void * ctx)
{
	SCNetworkCategoryRef	category = NULL;
	managerContextRef	context = (managerContextRef)ctx;
	Boolean			ok = FALSE;
	SCNSManagerRef		manager;
	SCNSServiceRef		service = (SCNSServiceRef)value;
	CFStringRef		serviceID;

	if (context->failed) {
		/* skip it, we've already hit an error */
		return;
	}
	if (service->service == NULL) {
		/* service removed after being created but before saved */
		return;
	}
	serviceID = SCNetworkServiceGetServiceID(service->service);
	manager = service->manager;
	if (service->category_id != NULL) {
		Boolean			ok;
		SCNetworkServiceRef	s;

		/* remove category-specific service */
		category = SCNetworkCategoryCreate(manager->prefs,
						   service->category_id);
		s = copy_service_for_category_and_ID(category,
						     service->category_value,
						     serviceID);
		if (s == NULL) {
			SC_log(LOG_NOTICE,
			       "%s: can't find service %@ (%@, %@)",
			       __func__, serviceID, service->category_id,
			       service->category_value);
			goto done;
		}
		ok = SCNetworkCategoryRemoveService(category,
						    service->category_value,
						    s);
		CFRelease(s);
		if (!ok) {
			SC_log(LOG_NOTICE,
			       "%s: can't remove service %@ (%@, %@)",
			       __func__, serviceID, service->category_id,
			       service->category_value);
			goto done;
		}
	}
	else {
		SCNetworkSetRef		set = NULL;

		if (service->set_id != NULL) {
			set = SCNetworkSetCopy(manager->prefs,
					       service->set_id);
		}
		if (set == NULL) {
			SC_log(LOG_NOTICE,
			       "%s: set no longer exists %@",
			       __func__, service->set_id);
			goto done;
		}
		
		/* remove set-specific service */		
		ok = remove_service_with_ID_in_set(set, serviceID);
		CFRelease(set);
		if (!ok) {
			SC_log(LOG_NOTICE,
			       "%s: failed to remove service %@ in set %@",
			       __func__, serviceID, service->set_id);
			goto done;
		}
	}
	ok = TRUE;

 done:
	if (!ok) {
		context->failed = TRUE;
	}
	__SC_CFRELEASE(category);
	return;
}

static Boolean
__SCNSManagerProcessRemovals(SCNSManagerRef manager)
{
	managerContext	context;
	Boolean		ok = TRUE;

	if (manager->removals == NULL
	    || CFSetGetCount(manager->removals) == 0) {
		/* nothing to do */
		goto done;
	}
	bzero(&context, sizeof(context));
	CFSetApplyFunction(manager->removals, managerRemove, &context);
	if (context.failed) {
		ok = FALSE;
	}

 done:
	return (ok);
}

static Boolean
__SCNSServiceSetQoSMarkingPolicy(SCNSServiceRef service, CFDictionaryRef policy)
{
	Boolean			ok = FALSE;
	SCNetworkServiceRef	s = service->service;

	if (service->category_id != NULL) {
		SCNetworkCategoryRef	category;
		SCNSManagerRef		manager = service->manager;
		CFStringRef		value = service->category_value;

		category = SCNetworkCategoryCreate(manager->prefs,
						   service->category_id);
		ok = SCNetworkCategorySetServiceQoSMarkingPolicy(category,
								 value,
								 s,
								 policy);
		__SC_CFRELEASE(category);
	}
	else {
		SCNetworkInterfaceRef	netif;

		netif = SCNetworkServiceGetInterface(s);
		if (netif == NULL) {
			goto done;
		}
		ok = SCNetworkInterfaceSetQoSMarkingPolicy(netif, policy);
	}
 done:
	return (ok);
}

static Boolean
__SCNSServiceSetProtocol(SCNSServiceRef service, CFStringRef entity_type,
			 CFDictionaryRef entity)
{
	Boolean			ok = FALSE;
	SCNetworkProtocolRef	protocol;
	SCNetworkServiceRef	s = service->service;

	protocol = SCNetworkServiceCopyProtocol(s, entity_type);
	if (protocol == NULL) {
		if (!SCNetworkServiceAddProtocolType(s, entity_type)) {
			SC_log(LOG_ERR,
			       "%s: %@ add protocol %@ failed, %s",
			       __func__, s, entity_type,
			       SCErrorString(SCError()));
			goto done;
		}
		protocol = SCNetworkServiceCopyProtocol(s, entity_type);
		if (protocol == NULL) {
			SC_log(LOG_ERR,
			       "%s: %@ failed to establish protocol %@, %s",
			       __func__, s, entity_type,
			       SCErrorString(SCError()));
			goto done;
		}
	}
	if (!SCNetworkProtocolSetConfiguration(protocol, entity)) {
		SC_log(LOG_ERR,
		       "%s: %@ failed to update protocol %@, %s",
		       __func__, s, entity_type,
		       SCErrorString(SCError()));
		goto done;
	}
	ok = TRUE;
done:
	__SC_CFRELEASE(protocol);
	return (ok);
}

typedef struct {
	SCNSServiceRef	service;
	Boolean		failed;
} serviceContext, *serviceContextRef;

static void
entityRemove(const void * value, void * ctx)
{
	serviceContextRef	context = (serviceContextRef)ctx;
	CFStringRef		entity_type = (CFStringRef)value;
	Boolean			ok;
	SCNSServiceRef		service = context->service;

	if (CFEqual(entity_type, kSCEntNetQoSMarkingPolicy)) {
		ok = __SCNSServiceSetQoSMarkingPolicy(service, NULL);
	}
	else {
		ok = SCNetworkServiceRemoveProtocolType(service->service,
							entity_type);
	}
	if (!ok && SCError() != kSCStatusNoKey) {
		SC_log(LOG_ERR, "%s: failed to remove %@ from %@, %s",
		       __func__, entity_type, service->service,
		       SCErrorString(SCError()));
		context->failed = TRUE;
	}
	return;
}

static void
entityChange(const void * key, const void * value, void * ctx)
{
	serviceContextRef	context = (serviceContextRef)ctx;
	CFDictionaryRef		entity = (CFDictionaryRef)value;
	CFStringRef		entity_type = (CFStringRef)key;
	Boolean			ok = FALSE;
	SCNSServiceRef		service = context->service;

	if (CFEqual(entity_type, kSCEntNetQoSMarkingPolicy)) {
		ok = __SCNSServiceSetQoSMarkingPolicy(service, entity);
	}
	else {
		ok = __SCNSServiceSetProtocol(service, entity_type, entity);
	}
	if (!ok) {
		context->failed = TRUE;
	}
	return;
}

static Boolean
service_establish_default(SCNetworkServiceRef service)
{
	CFArrayRef 	protocols;
	Boolean		success = FALSE;

	protocols = SCNetworkServiceCopyProtocols(service);
	if (protocols != NULL) {
		for (CFIndex i = 0, count = CFArrayGetCount(protocols);
		     i < count; i++) {
			SCNetworkProtocolRef	proto;
			CFStringRef		type;

			proto = (SCNetworkProtocolRef)
				CFArrayGetValueAtIndex(protocols, i);
			type = SCNetworkProtocolGetProtocolType(proto);
			if (!SCNetworkServiceRemoveProtocolType(service, type)) {
				SC_log(LOG_NOTICE,
				       "%s: failed to remove %@, %s",
				       __func__, type,
				       SCErrorString(SCError()));
				goto done;
			}
		}
	}
	success = SCNetworkServiceEstablishDefaultConfiguration(service);
	if (!success) {
		SC_log(LOG_NOTICE,
		       "%s: failed to establish default, %s",
		       __func__,
		       SCErrorString(SCError()));
	}
	
 done:
	__SC_CFRELEASE(protocols);
	return (success);
}

static Boolean
__SCNSServiceApplyChanges(SCNSServiceRef service)
{
	Boolean		ok = FALSE;

	if (service->set_to_default) {
		SCNetworkServiceRef	s = service->service;

		if (!service_establish_default(s)) {
			goto done;
		}
	}

	/* remove entities */
	if (service->removals != NULL) {
		serviceContext	context;

		bzero(&context, sizeof(context));
		context.service = service;
		CFSetApplyFunction(service->removals, entityRemove,
				   &context);
		if (context.failed) {
			goto done;
		}
	}
	/* change entities */
	if (service->changes != NULL) {
		serviceContext	context;

		bzero(&context, sizeof(context));
		context.service = service;
		CFDictionaryApplyFunction(service->changes, entityChange,
					  &context);
		if (context.failed) {
			goto done;
		}
	}
	ok = TRUE;
	
 done:
	__SC_CFRELEASE(service->removals);
	__SC_CFRELEASE(service->changes);
	service->set_to_default = FALSE;
	return (ok);
}


static Boolean
__SCNSServiceInstantiateService(SCNSServiceRef service)
{
	SCNSManagerRef		manager = service->manager;
	Boolean			ok = FALSE;
	SCNetworkServiceRef	s = NULL;

	if (service->category_id != NULL) {
		s = create_service_in_category(manager->prefs,
					       service->category_id,
					       service->category_value,
					       service->netif);
		if (s == NULL) {
			goto done;
		}
	}
	else {
		SCNetworkSetRef		set = NULL;
		
		if (service->set_id != NULL) {
			set = SCNetworkSetCopy(manager->prefs,
					       service->set_id);
		}
		if (set == NULL) {
			SC_log(LOG_NOTICE,
			       "%s: set no longer exists %@",
			       __func__, service->set_id);
			goto done;
		}
		s = create_service_in_set(manager->prefs, set, service->netif);
		__SC_CFRELEASE(set);
		if (s == NULL) {
			goto done;
		}
	}
	/* start out with fully populated */
	if (!SCNetworkServiceEstablishDefaultConfiguration(s)) {
		SC_log(LOG_NOTICE,
		       "%s: EstablishDefaultConfiguration, %s",
		       __func__, SCErrorString(SCError()));
		goto done;
	}
	if (service->changes == NULL && service->removals == NULL) {
		/* no other changes, make sure we don't do this again */
		service->set_to_default = FALSE;
	}
	CFRetain(s);
	service->service = s;
	ok = TRUE;
 done:
	__SC_CFRELEASE(s);
	return (ok);
}

static Boolean
__SCNSServiceUpdateService(SCNSServiceRef service)
{
	SCNSManagerRef		manager = service->manager;
	Boolean			ok = FALSE;
	SCNetworkServiceRef	s = NULL;
	CFStringRef		serviceID;

	serviceID = SCNetworkServiceGetServiceID(service->service);
	if (service->category_id != NULL) {
		SCNetworkCategoryRef	category;

		category = SCNetworkCategoryCreate(manager->prefs,
						   service->category_id);
		s = copy_service_for_category_and_ID(category,
						     service->category_value,
						     serviceID);
		__SC_CFRELEASE(category);
		if (s == NULL) {
			SC_log(LOG_NOTICE,
			       "%s: can't find service %@ (%@, %@)",
			       __func__, serviceID, service->category_id,
			       service->category_value);
			goto done;
		}
	}
	else {
		SCNetworkSetRef		set = NULL;
		
		if (service->set_id != NULL) {
			set = SCNetworkSetCopy(manager->prefs,
					       service->set_id);
		}
		if (set == NULL) {
			SC_log(LOG_NOTICE,
			       "%s: set %@ no longer exists",
			       __func__, service->set_id);
			goto done;
		}
		s = copy_service_with_ID_in_set(set, serviceID);
		CFRelease(set);
		if (s == NULL) {
			SC_log(LOG_NOTICE,
			       "%s: can't find service %@ in set %@",
			       __func__, serviceID, service->set_id);
			goto done;
		}
	}
	/* replace service object */
	CFRelease(service->service);
	CFRetain(s);
	service->service = s;
	ok = TRUE;
 done:
	__SC_CFRELEASE(s);
	return (ok);
}

static Boolean
__SCNSServiceChangeService(SCNSServiceRef service)
{
	Boolean			ok = FALSE;

	if (service->service == NULL) {
		/* need to create it */
		ok = __SCNSServiceInstantiateService(service);
	}
	else {
		/* need to ensure it's still there */
		ok = __SCNSServiceUpdateService(service);
	}
	if (ok) {
		ok = __SCNSServiceApplyChanges(service);
	}
	return (ok);
	
}

static void
managerChange(const void * value, void * ctx)
{
	managerContextRef	context = (managerContextRef)ctx;
	Boolean			ok = FALSE;
	SCNSServiceRef		service = (SCNSServiceRef)value;

	if (context->failed) {
		/* skip it, we've already hit an error */
		return;
	}
	if (!__SCNSServiceChangeService(service)) {
		if (service->category_id != NULL) {
			SC_log(LOG_NOTICE,
			       "%s: can't update service for %@ (%@, %@)",
			       __func__,
			       service->netif,
			       service->category_id,
			       service->category_value);
		}
		else {
			SC_log(LOG_NOTICE,
			       "%s: can't update service for %@",
			       __func__,
			       service->netif);
		}
		goto done;
	}
	ok = TRUE;
 done:
	if (!ok) {
		context->failed = TRUE;
	}
	return;
}

static Boolean
__SCNSManagerProcessChanges(SCNSManagerRef manager)
{
	managerContext	context;
	Boolean		ok = TRUE;

	if (manager->changes == NULL
	    || CFSetGetCount(manager->changes) == 0) {
		/* nothing to do */
		goto done;
	}
	bzero(&context, sizeof(context));
	CFSetApplyFunction(manager->changes, managerChange, &context);
	if (context.failed) {
		ok = FALSE;
	}
 done:
	return (ok);
}

static CFStringRef
copy_category_value(CFStringRef category_id, SCNetworkInterfaceRef netif)
{
	return __SCNetworkCategoryManagerCopyActiveValueNoSession(category_id,
								  netif);
}

static SCNSServiceRef
__SCNSManagerCopyService(SCNSManagerRef manager,
			 SCNetworkInterfaceRef netif,
			 CFStringRef __nullable category_id,
			 CFStringRef __nullable category_value,
			 Boolean default_service_ok)
{
	SCNetworkServiceRef	s = NULL;
	SCNSServiceRef		service = NULL;
	CFStringRef		set_id = NULL;

	if (category_id != NULL && category_value != NULL) {
		SCNetworkCategoryRef	category;

		category = SCNetworkCategoryCreate(manager->prefs,
						   category_id);
		s = copy_service_for_category(category, category_value, netif);
		__SC_CFRELEASE(category);
	}
	else {
		default_service_ok = TRUE;
	}
	if (s == NULL && default_service_ok) {
		SCNetworkSetRef		set;

		set = SCNetworkSetCopyCurrent(manager->prefs);
		if (set == NULL) {
			SC_log(LOG_NOTICE, "%s: No current set\n", __func__);
		}
		else {
			s = copy_service_in_set(set, netif);
			if (s != NULL) {
				set_id = SCNetworkSetGetSetID(set);
				CFRetain(set_id);
			}
			CFRelease(set);
		}
	}
	if (s == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}
	service = __SCNSServiceCreate(manager, category_id, category_value,
				      netif, s);
	if (service != NULL && set_id != NULL) {
		service->set_id = set_id;
		CFRetain(set_id);
	}

 done:
	__SC_CFRELEASE(s);
	__SC_CFRELEASE(set_id);
	return (service);
}

static SCNSManagerRef
__SCNSManagerCreateCommon(CFStringRef label,
			  AuthorizationRef __nullable authorization,
			  Boolean use_auth)
{
	SCNSManagerRef		manager = NULL;
	SCPreferencesRef	prefs;
	CFStringRef		str;

	str = CFStringCreateWithFormat(NULL, NULL,
				       CFSTR("SCNSManager(%@)"),
				       label);
	if (use_auth) {
		prefs = SCPreferencesCreateWithAuthorization(NULL,
							     str,
							     NULL,
							     authorization);
	}
	else {
		prefs = SCPreferencesCreate(NULL, str, NULL);
	}
	CFRelease(str);
	if (prefs == NULL) {
		SC_log(LOG_NOTICE, "SCPreferencesCreate failed, %s",
		       SCErrorString(SCError()));
		goto done;
	}
	manager = __SCNSManagerCreate(prefs);
	CFRelease(prefs);
	if (manager == NULL) {
		SC_log(LOG_NOTICE, "%s: failed to allocate manager",
		       __func__);
		goto done;
	}
 done:
	return (manager);
}

static void
__SCNSManagerServiceChanged(SCNSManagerRef manager,
			    SCNSServiceRef service)
{
	CFMutableSetRef		changes;

	if (manager->removals != NULL) {
		CFSetRemoveValue(manager->removals, service);
	}
	changes = __SCNSManagerGetChanges(manager);
	CFSetAddValue(changes, service);
}

static void
__SCNSManagerStoreCallback(SCDynamicStoreRef store,
			   CFArrayRef changes,
			   void * info)
{
#pragma unused(store)
#pragma unused(changes)
	SCNSManagerRef		manager;

	manager = (SCNSManagerRef)info;
	if (manager->handler != NULL) {
		(manager->handler)(manager, 0);
	}
}

static Boolean
store_set_notification_keys(SCDynamicStoreRef store)
{
	CFStringRef		entities[] = {
		kSCEntNetIPv4,
		kSCEntNetIPv6,
		kSCEntNetProxies,
		kSCEntNetDNS
	};
	CFIndex			entities_count;
	CFMutableArrayRef	keys;
	Boolean			ok;
	CFMutableArrayRef	patterns;

	/* category changes key */
	keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(keys,
			   kCategoryManagerNotificationKey);

	/* service changes patterns */
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	entities_count = (CFIndex)(sizeof(entities) / sizeof(entities[0]));	
	for (CFIndex i = 0; i < entities_count; i++) {
		CFStringRef	pattern;

		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(
					      NULL,
					      kSCDynamicStoreDomainState,
					      kSCCompAnyRegex,
					      entities[i]);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		
	}
	ok = SCDynamicStoreSetNotificationKeys(store, keys, patterns);
	if (!ok) {
		SC_log(LOG_NOTICE,
		       "%s: SCDynamicStoreSetNotificationKeys failed, %s",
		       __func__, SCErrorString(SCError()));
	}
	CFRelease(keys);
	CFRelease(patterns);
	return (ok);
}

static SCDynamicStoreRef
store_create(SCNSManagerRef manager, dispatch_queue_t queue)
{
	SCDynamicStoreContext	context = {
		.version = 0,
		.info = manager,
		.retain = NULL,
		.release = NULL,
		.copyDescription = NULL
	};
	Boolean			ok = FALSE;
	SCDynamicStoreRef	store;

	ok = FALSE;
	store = SCDynamicStoreCreate(NULL,
				     CFSTR("SCNSManager"),
				     __SCNSManagerStoreCallback,
				     &context);
	if (store == NULL) {
		SC_log(LOG_NOTICE,
		       "%s: SCDynamicStoreCreate failed, %s",
		       __func__, SCErrorString(SCError()));
		goto done;
	}
	if (!store_set_notification_keys(store)) {
		goto done;
	}
	if (!SCDynamicStoreSetDispatchQueue(store, queue)) {
		SC_log(LOG_NOTICE,
		       "%s: SCDynamicStoreSetDispatchQueue failed, %s",
		       __func__, SCErrorString(SCError()));
		goto done;
	}
	ok = TRUE;
 done:
	if (!ok) {
		__SC_CFRELEASE(store);
	}
	return (store);
}

static void
__SCNSManagerPrefsCallback(SCPreferencesRef prefs,
			   SCPreferencesNotification type,
			   void * info)
{
#pragma unused(type)
#pragma unused(prefs)
	SCNSManagerRef	manager;

	manager = (SCNSManagerRef)info;
	if (manager->handler != NULL) {
		(manager->handler)(manager, 0);
	}
	return;
}

static Boolean
__SCNSManagerEnablePrefsCallback(SCNSManagerRef manager, dispatch_queue_t queue)
{
	SCPreferencesContext	context = {
		.version = 0,
		.info = manager,
		.retain = NULL,
		.release = NULL,
		.copyDescription = NULL
	};
	Boolean			ok = FALSE;

	ok = SCPreferencesSetCallback(manager->prefs,
				      __SCNSManagerPrefsCallback,
				      &context);
	if (!ok) {
		SC_log(LOG_NOTICE, "%s: SCPreferencesSetCallback failed, %s",
		       __func__, SCErrorString(SCError()));
		goto done;
	}
	if (!SCPreferencesSetDispatchQueue(manager->prefs, queue)) {
		SCPreferencesSetCallback(manager->prefs, NULL, NULL);
		goto done;
	}
	ok = TRUE;
 done:
	return (ok);
}

static Boolean
__SCNSManagerEnableCallback(SCNSManagerRef manager, dispatch_queue_t queue,
			    SCNSManagerEventHandler handler)
{
	Boolean			ok = FALSE;
	SCDynamicStoreRef	store;

	store = store_create(manager, queue);
	if (store == NULL) {
		goto done;
	}
	if (!__SCNSManagerEnablePrefsCallback(manager, queue)) {
		goto done;
	}
	manager->queue = queue;
	dispatch_retain(queue);
	manager->handler = Block_copy(handler);
	manager->store = CFRetain(store);
	ok = TRUE;
 done:
	__SC_CFRELEASE(store);
	return (ok);
}

static void
__SCNSManagerDisableCallback(SCNSManagerRef manager)
{
	if (manager->queue != NULL) {
		dispatch_release(manager->queue);
		manager->queue = NULL;
	}
	if (manager->handler != NULL) {
		Block_release(manager->handler);
		manager->handler = NULL;
	}
	SCPreferencesSetDispatchQueue(manager->prefs, NULL);
	SCPreferencesSetCallback(manager->prefs, NULL, NULL);
	__SC_CFRELEASE(manager->store);
	return;
}

/*
 * API
 */
SCNSManagerRef
SCNSManagerCreate(CFStringRef label)
{
	return (__SCNSManagerCreateCommon(label, NULL, FALSE));
}

SCNSManagerRef
SCNSManagerCreateWithAuthorization(CFStringRef label,
				   AuthorizationRef __nullable authorization)
{
	return (__SCNSManagerCreateCommon(label, authorization, TRUE));
}

void
SCNSManagerRefresh(SCNSManagerRef manager)
{
	SCPreferencesSynchronize(manager->prefs);
}

void
SCNSManagerRemoveService(SCNSManagerRef manager, SCNSServiceRef service)
{
	CFMutableSetRef		removals;

	if (manager->changes != NULL) {
		CFSetRemoveValue(manager->changes, service);
	}
	removals = __SCNSManagerGetRemovals(manager);
	CFSetAddValue(removals, service);
}

Boolean
SCNSManagerApplyChanges(SCNSManagerRef manager)
{
	Boolean		lock_acquired = FALSE;
	Boolean		ok = FALSE;

	if ((manager->changes == NULL
	     || CFSetGetCount(manager->changes) == 0)
	    && (manager->removals == NULL
		|| CFSetGetCount(manager->removals) == 0)) {
		/* no changes nothing to do */
		SC_log(LOG_NOTICE, "%s: no changes", __func__);
		ok = TRUE;
		goto done;
	}
#define __SCNS_LOCK_RETRY	10
	for (int i = 0; i < __SCNS_LOCK_RETRY; i++) {
		if (SCPreferencesLock(manager->prefs, TRUE)) {
			lock_acquired = TRUE;
			break;
		}
		if (SCError() == kSCStatusStale) {
			/* synchronize and try again */
			SCPreferencesSynchronize(manager->prefs);
		}
		else {
			SC_log(LOG_NOTICE, "%s: failed to get lock, %s",
			       __func__, SCErrorString(SCError()));
			break;
		}
	}
	if (!lock_acquired) {
		SC_log(LOG_NOTICE,
		       "%s: can't acquire lock, giving up",
		       __func__);
		goto done;
	}

	/* remove services */
	if (!__SCNSManagerProcessRemovals(manager)) {
		goto unlock_done;
	}

	/* add/change services */
	if (!__SCNSManagerProcessChanges(manager)) {
		goto unlock_done;
	}

	/* commit/apply */
	if (!SCPreferencesCommitChanges(manager->prefs)) {
		SC_log(LOG_NOTICE,
		       "%s: SCPreferencesCommitChanges failed, %s",
		       __func__, SCErrorString(SCError()));
		goto unlock_done;
	}
	if (!SCPreferencesApplyChanges(manager->prefs)) {
		SC_log(LOG_NOTICE,
		       "%s: SCPreferencesApplyChanges failed, %s",
		       __func__, SCErrorString(SCError()));
		goto unlock_done;
	}
	ok = TRUE;
	
 unlock_done:
	SCPreferencesUnlock(manager->prefs);
 done:
	__SC_CFRELEASE(manager->changes);
	__SC_CFRELEASE(manager->removals);
	return (ok);
}

SCNSServiceRef
SCNSManagerCopyService(SCNSManagerRef manager,
		       SCNetworkInterfaceRef netif,
		       CFStringRef __nullable category_id,
		       CFStringRef __nullable category_value)
{
	SCNSServiceRef		service = NULL;

	if (category_id != NULL && category_value == NULL) {
		/* need to specify both id and value */
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}
		 

	service = __SCNSManagerCopyService(manager, netif,
					   category_id, category_value,
					   FALSE);
 done:
	return (service);
}

SCNSServiceRef
SCNSManagerCreateService(SCNSManagerRef manager,
			 SCNetworkInterfaceRef netif,
			 CFStringRef __nullable category_id,
			 CFStringRef __nullable category_value)
{
	SCNSServiceRef		service = NULL;
	CFStringRef		set_id = NULL;

	if (category_id != NULL && category_value == NULL) {
		/* non-NULL category_id requires non-NULL category_value */
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}
	if (category_id == NULL) {
		SCNetworkSetRef		set;

		set = SCNetworkSetCopyCurrent(manager->prefs);
		if (set == NULL) {
			SC_log(LOG_NOTICE, "%s: No current set\n", __func__);
			goto done;
		}
		set_id = SCNetworkSetGetSetID(set);
		CFRetain(set_id);
		CFRelease(set);
	}
	service = __SCNSServiceCreate(manager, category_id, category_value,
				      netif, NULL);
	if (service != NULL) {
		if (set_id != NULL) {
			CFRetain(set_id);
			service->set_id = set_id;
		}
		__SCNSManagerServiceChanged(manager, service);
	}

 done:
	__SC_CFRELEASE(set_id);
	return (service);
}

SCNSServiceRef
SCNSManagerCopyCurrentService(SCNSManagerRef manager,
			      SCNetworkInterfaceRef netif,
			      CFStringRef __nullable category_id)
{
	CFStringRef		category_value = NULL;
	SCNSServiceRef		service = NULL;

	if (category_id != NULL) {
		category_value = copy_category_value(category_id, netif);
		if (category_value == NULL) {
			/* no current value, use default */
			category_id = NULL;
		}
	}
	service = __SCNSManagerCopyService(manager, netif,
					   category_id, category_value,
					   TRUE);
	__SC_CFRELEASE(category_value);
	return (service);
}

Boolean
SCNSManagerSetEventHandler(SCNSManagerRef manager, dispatch_queue_t queue,
			   SCNSManagerEventHandler handler)
{
	Boolean		ok = FALSE;

	if (queue != NULL) {
		if (manager->queue != NULL || handler == NULL) {
			_SCErrorSet(kSCStatusInvalidArgument);
			goto done;
		}
		if (!__SCNSManagerEnableCallback(manager, queue,
						 handler)) {
			goto done;
		}
	}
	else if (manager->queue != NULL) {
		__SCNSManagerDisableCallback(manager);
	}
	ok = TRUE;
	
 done:
	return (ok);

}
			   
SCNetworkInterfaceRef
SCNSServiceGetInterface(SCNSServiceRef service)
{
	return (service->netif);
}

CFStringRef
SCNSServiceGetServiceID(SCNSServiceRef service)
{
	CFStringRef	str;

	if (service->service == NULL) {
		str = CFSTR("n/a");
	}
	else {
		str = SCNetworkServiceGetServiceID(service->service);
	}
	return (str);
}

CFStringRef
SCNSServiceGetName(SCNSServiceRef service)
{
	CFStringRef	str;

	if (service->service == NULL) {
		str = CFSTR("n/a");
	}
	else {
		str = SCNetworkServiceGetName(service->service);
	}
	return (str);
}

static Boolean
__SCNSServiceGetEntity(SCNSServiceRef service, CFStringRef entity_type,
		       CFDictionaryRef * ret_entity)
{
	CFDictionaryRef	entity = NULL;
	Boolean		present = FALSE;

	if (service->removals != NULL
	    && CFSetContainsValue(service->removals, entity_type)) {
		/* it was removed */
		present = TRUE;
	}
	else if (service->changes != NULL) {
		entity = CFDictionaryGetValue(service->changes, entity_type);
		if (entity != NULL) {
			present = TRUE;
		}
	}
	*ret_entity = entity;
	return (present);
}

CFDictionaryRef
SCNSServiceCopyProtocolEntity(SCNSServiceRef service,
			      CFStringRef entity_type)
{
	CFDictionaryRef		entity = NULL;
	SCNetworkProtocolRef	proto = NULL;
	int			status = kSCStatusOK;

	if (!__SCNetworkProtocolIsValidType(entity_type)) {
		status = kSCStatusInvalidArgument;
		goto done;
	}
	if (__SCNSServiceGetEntity(service, entity_type, &entity)) {
		goto done;
	}
	if (service->service == NULL) {
		/* hasn't been created yet, so return NULL */
	}
	else {
		/* existing */
		proto = SCNetworkServiceCopyProtocol(service->service,
						     entity_type);
		if (proto != NULL) {
			entity = SCNetworkProtocolGetConfiguration(proto);
		}
	}
 done:
	if (entity != NULL) {
		CFRetain(entity);
	}
	else {
		if (status == kSCStatusOK) {
			status = kSCStatusNoKey;
		}
		_SCErrorSet(status);
	}
	__SC_CFRELEASE(proto);
	return (entity);
}

static Boolean
__SCNSServiceSetEntity(SCNSServiceRef service,
		       CFStringRef entity_type,
		       CFDictionaryRef __nullable entity)
{
	Boolean		ok = FALSE;

	/* remember that a change was made */
	if (entity != NULL) {
		CFMutableDictionaryRef	changes;

		changes = __SCNSServiceGetChanges(service);
		CFDictionarySetValue(changes, entity_type, entity);
		if (service->removals != NULL) {
			CFSetRemoveValue(service->removals, entity_type);
		}
	}
	else {
		CFStringRef		other_entity = NULL;
		CFMutableSetRef		removals;

		removals = __SCNSServiceGetRemovals(service);
		if (CFEqual(entity_type, kSCEntNetIPv4)) {
			other_entity = kSCEntNetIPv6;
		}
		else if (CFEqual(entity_type, kSCEntNetIPv6)) {
			other_entity = kSCEntNetIPv4;
		}
		if (other_entity != NULL
		    && CFSetContainsValue(removals, other_entity)) {
			SC_log(LOG_ERR,
			       "%s: can't remove both IPv4 and IPv6",
			       __func__);
			_SCErrorSet(kSCStatusInvalidArgument);
			goto done;
		}
		CFSetAddValue(removals, entity_type);
		if (service->changes != NULL) {
			CFDictionaryRemoveValue(service->changes, entity_type);
		}
	}
	__SCNSManagerServiceChanged(service->manager, service);
	ok = TRUE;
 done:
	return (ok);
}

Boolean
SCNSServiceSetProtocolEntity(SCNSServiceRef service,
			     CFStringRef entity_type,
			     CFDictionaryRef __nullable entity)
{
	if (!__SCNetworkProtocolIsValidType(entity_type)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return (FALSE);
	}
	return (__SCNSServiceSetEntity(service, entity_type, entity));
}

void
SCNSServiceUseDefaultProtocolEntities(SCNSServiceRef service)
{
	service->set_to_default = TRUE;
	__SC_CFRELEASE(service->removals);
	__SC_CFRELEASE(service->changes);
	__SCNSManagerServiceChanged(service->manager, service);
}

CFDictionaryRef __nullable
SCNSServiceCopyActiveEntity(SCNSServiceRef service,
			    CFStringRef entity_type)
{
	CFDictionaryRef		active = NULL;
	Boolean			is_active;
	CFDictionaryRef		setup;
	CFDictionaryRef		state;

	if (service->state == NULL) {
		SCNSServiceRefreshActiveState(service);
	}
	if (service->state == NULL) {
		return (NULL);
	}

	/* check whether the service is active */
	is_active = CFDictionaryContainsKey(service->state,
					    kSCEntNetIPv4)
		|| CFDictionaryContainsKey(service->state,
					   kSCEntNetIPv6);
	if (!is_active) {
		goto done;
	}
	/* merge state and setup, setup keys take precedence */
	state = CFDictionaryGetValue(service->state, entity_type);
	setup = SCNSServiceCopyProtocolEntity(service, entity_type);
	active = copy_merged_state_and_setup(state, setup);
	__SC_CFRELEASE(setup);
 done:
	return (active);
}

void
SCNSServiceRefreshActiveState(SCNSServiceRef service)
{
	__SC_CFRELEASE(service->state);
	service->state = __SCNSServiceCopyState(service);
}


CFStringRef
SCNSServiceGetCategoryID(SCNSServiceRef service)
{
	return (service->category_id);
}

CFStringRef
SCNSServiceGetCategoryValue(SCNSServiceRef service)
{
	return (service->category_value);
}

Boolean
SCNSServiceSetQoSMarkingPolicy(SCNSServiceRef service,
			       CFDictionaryRef __nullable entity)
{
	return (__SCNSServiceSetEntity(service,
				       kSCEntNetQoSMarkingPolicy,
				       entity));
}

CFDictionaryRef __nullable
SCNSServiceCopyQoSMarkingPolicy(SCNSServiceRef service)
{
	CFDictionaryRef		entity = NULL;
	SCNetworkServiceRef	s = service->service;

	if (__SCNSServiceGetEntity(service,
				   kSCEntNetQoSMarkingPolicy,
				   &entity)) {
		goto done;
	}
	if (s == NULL) {
		/* hasn't been created yet, so return NULL */
	}
	else if (service->category_id != NULL) {
		SCNetworkCategoryRef	category;
		SCNSManagerRef		manager = service->manager;
		CFStringRef		value = service->category_value;

		category = SCNetworkCategoryCreate(manager->prefs,
						   service->category_id);
		entity = SCNetworkCategoryGetServiceQoSMarkingPolicy(category,
								     value,
								     s);
		__SC_CFRELEASE(category);
	}
	else {
		SCNetworkInterfaceRef	netif;

		netif = SCNetworkServiceGetInterface(s);
		if (netif == NULL) {
			goto done;
		}
		entity = SCNetworkInterfaceGetQoSMarkingPolicy(netif);
	}
 done:
	if (entity != NULL) {
		CFRetain(entity);
	}
	else {
		_SCErrorSet(kSCStatusNoKey);
	}
	return (entity);
}


#ifdef TEST_SCNETWORK_SETTINGS

#include "SCD.h"

__SCThreadSpecificDataRef
__SCGetThreadSpecificData()
{
	static __SCThreadSpecificDataRef tsd;
	if (tsd == NULL) {
		tsd = CFAllocatorAllocate(kCFAllocatorSystemDefault,
					  sizeof(__SCThreadSpecificData), 0);
		bzero(tsd, sizeof(*tsd));
	}
	return (tsd);
}

Boolean
__SCPreferencesUsingDefaultPrefs(SCPreferencesRef prefs)
{
	return (TRUE);
}

__private_extern__ Boolean
__SCNetworkProtocolIsValidType(CFStringRef protocolType)
{
	static const CFStringRef	*valid_types[]	= {
		&kSCNetworkProtocolTypeDNS,
		&kSCNetworkProtocolTypeIPv4,
		&kSCNetworkProtocolTypeIPv6,
		&kSCNetworkProtocolTypeProxies,
#if	!TARGET_OS_IPHONE
		&kSCNetworkProtocolTypeSMB,
#endif	// !TARGET_OS_IPHONE
	};

	for (size_t i = 0; i < sizeof(valid_types)/sizeof(valid_types[0]); i++) {
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

#include <getopt.h>

static void
usage(const char * progname)
{
	fprintf(stderr,
		"usage: %s -i <ifname> [ -w ] [ -A ][ -C ] [ -c <category> -c <value> ]\n",
		progname);
	exit(1);
}

static inline CFStringRef
cfstring_create_with_cstring(const char * str)
{
	if (str == NULL) {
		return (NULL);
	}
	return (CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8));
}

static void
dump_entities(SCNSServiceRef service)
{
	CFStringRef			entities[] = { kSCEntNetIPv4,
						     kSCEntNetIPv6,
						     kSCEntNetDNS,
						     kSCEntNetInterface,
						     kSCEntNetProxies,
	};
	unsigned int			entities_count = sizeof(entities)
		/ sizeof(entities[0]);
	
	for (unsigned int i = 0; i < entities_count; i++) {
		CFDictionaryRef	active;
		CFDictionaryRef	setup;

		setup = SCNSServiceCopyProtocolEntity(service, entities[i]);
		active = SCNSServiceCopyActiveEntity(service, entities[i]);
		SCPrint(TRUE, stdout, CFSTR("%@:\nsetup %@\nactive %@\n"),
			entities[i], setup, active);
		__SC_CFRELEASE(setup);
		__SC_CFRELEASE(active);
	}
}

static void
show_service(SCNSServiceRef service, Boolean do_current_active)
{
	SCPrint(TRUE, stdout,
		CFSTR("service ID %@ name %@"),
		SCNSServiceGetServiceID(service),
		SCNSServiceGetName(service));
	if (do_current_active) {
		CFStringRef	active_category;
		CFStringRef	active_value;

		active_category = SCNSServiceGetCategoryID(service);
		active_value = SCNSServiceGetCategoryValue(service);
		if (active_category != NULL) {
			SCPrint(TRUE, stdout,
				CFSTR("active category %@ value %@"),
				active_category,
				active_value);
		}
	}
	printf("\n");
	dump_entities(service);
}

static void
manager_event(SCNSManagerRef manager,
	      SCNSManagerEventFlags event_flags,
	      SCNetworkInterfaceRef netif,
	      CFStringRef category,
	      Boolean do_current_active)
{
	SCNSServiceRef	service;

	service = SCNSManagerCopyCurrentService(manager, netif,	category);
	if (service == NULL) {
		fprintf(stderr, "No service\n");
	}
	else {
		show_service(service, do_current_active);
		CFRelease(service);
	}
}

int
main(int argc, char * argv[])
{
	CFStringRef			category = NULL;
	int 				ch;
	Boolean				do_current_active = FALSE;
	Boolean				do_create = FALSE;
	CFStringRef			ifname = NULL;
	SCNSManagerRef			manager;
	SCNetworkInterfaceRef		netif;
	const char *			progname = argv[0];
	SCNSServiceRef			service;
	CFStringRef			value = NULL;
	Boolean				watch_changes = FALSE;

	while ((ch = getopt(argc, argv, "ACc:i:v:w")) != -1) {
		switch (ch) {
		case 'A':
			if (do_current_active) {
				fprintf(stderr,
					"-A specified multiple times\n");
				usage(progname);
			}
			if (value != NULL) {
				fprintf(stderr,
					"-A can't be specified with -v\n");
				usage(progname);
			}
			do_current_active = TRUE;
			break;
		case 'C':
			do_create = TRUE;
			break;
		case 'c':
			if (category != NULL) {
				fprintf(stderr,
					"-c specified multiple times\n");
				usage(progname);
			}
			category = cfstring_create_with_cstring(optarg);
			break;
		case 'i':
			if (ifname != NULL) {
				fprintf(stderr,
					"-i specified multiple times\n");
				usage(progname);
			}
			ifname = cfstring_create_with_cstring(optarg);
			break;
		case 'v':
			if (do_current_active) {
				fprintf(stderr,
					"-v can't be specified with -A\n");
				usage(progname);
			}
			if (value != NULL) {
				fprintf(stderr,
					"-v specified multiple times\n");
				usage(progname);
			}
			value = cfstring_create_with_cstring(optarg);
			break;
		case 'w':
			watch_changes = TRUE;
			break;
		default:
			usage(progname);
			break;
		}
	}
	if (ifname == NULL) {
		usage(progname);
	}
	netif = _SCNetworkInterfaceCreateWithBSDName(NULL, ifname,
					     kIncludeAllVirtualInterfaces);
	if (netif == NULL) {
		fprintf(stderr, "Failed to allocate netif\n");
		exit(2);
	}
	SCPrint(TRUE, stdout, CFSTR("netif %@\n"), netif);
	manager = SCNSManagerCreate(CFSTR("test.SCNetworkSettings"));
	if (manager == NULL) {
		fprintf(stderr, "Failed to allocate manager\n");
		exit(2);
	}
	if (do_current_active) {
		service = SCNSManagerCopyCurrentService(manager, netif,
							category);
	}
	else {
		service = SCNSManagerCopyService(manager, netif,
						 category, value);
	}
	if (service == NULL) {
		fprintf(stderr, "No service\n");
	}
	else {
		show_service(service, do_current_active);
	}
	__SC_CFRELEASE(service);
	if (do_create && value != NULL) {
		service = SCNSManagerCreateService(manager, netif,
						   category, value);
		if (service == NULL) {
			fprintf(stderr,
				"failed to create service, %s\n",
				SCErrorString(SCError()));
		}
		else {
			SCPrint(TRUE, stdout,
				CFSTR("service ID %@ name %@\n"),
				SCNSServiceGetServiceID(service),
				SCNSServiceGetName(service));
			dump_entities(service);
			__SC_CFRELEASE(service);
		}
		
	}
	if (watch_changes) {
		dispatch_queue_t 	queue;
		SCNSManagerEventHandler	handler;


		queue = dispatch_queue_create("test-SCNS", NULL);
		handler = ^(SCNSManagerRef manager,
			    SCNSManagerEventFlags event_flags) {
			manager_event(manager, event_flags, netif,
				      category, do_current_active);
		};
		if (!SCNSManagerSetEventHandler(manager, queue, handler)) {
			fprintf(stderr,
				"Failed to set event handler\n");
			exit(1);
		}
		dispatch_main();
		
	}
	__SC_CFRELEASE(manager);
	__SC_CFRELEASE(netif);
	exit(0);
}

#endif /* TEST_SCNETWORK_SETTINGS */
