/*
 * Copyright (c) 2011-2013 Apple Inc. All rights reserved.
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
 * IPConfigurationService.c
 * - API to communicate with IPConfiguration to instantiate services
 */
/* 
 * Modification History
 *
 * April 14, 2011 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <pthread.h>
#include <mach/mach_error.h>
#include <mach/mach_port.h>
#include "ipconfig_types.h"
#include "ipconfig_ext.h"
#include "symbol_scope.h"
#include "cfutil.h"
#include "ipconfig.h"
#include "IPConfigurationLog.h"
#include "IPConfigurationServiceInternal.h"
#include "IPConfigurationService.h"

const CFStringRef 
kIPConfigurationServiceOptionMTU = _kIPConfigurationServiceOptionMTU;

const CFStringRef 
kIPConfigurationServiceOptionPerformNUD = _kIPConfigurationServiceOptionPerformNUD;

/**
 ** IPConfigurationService
 **/
struct __IPConfigurationService {
    CFRuntimeBase		cf_base;

    mach_port_t			server;
    CFStringRef			ifname;
    CFDataRef			serviceID_data;
    CFStringRef			store_key;
};


/**
 ** CF object glue code
 **/
STATIC CFStringRef	__IPConfigurationServiceCopyDebugDesc(CFTypeRef cf);
STATIC void		__IPConfigurationServiceDeallocate(CFTypeRef cf);

STATIC CFTypeID __kIPConfigurationServiceTypeID = _kCFRuntimeNotATypeID;

STATIC const CFRuntimeClass __IPConfigurationServiceClass = {
    0,						/* version */
    "IPConfigurationService",			/* className */
    NULL,					/* init */
    NULL,					/* copy */
    __IPConfigurationServiceDeallocate,		/* deallocate */
    NULL,					/* equal */
    NULL,					/* hash */
    NULL,					/* copyFormattingDesc */
    __IPConfigurationServiceCopyDebugDesc	/* copyDebugDesc */
};

STATIC CFStringRef
__IPConfigurationServiceCopyDebugDesc(CFTypeRef cf)
{
    CFAllocatorRef		allocator = CFGetAllocator(cf);
    CFMutableStringRef		result;
    IPConfigurationServiceRef	service = (IPConfigurationServiceRef)cf;

    result = CFStringCreateMutable(allocator, 0);
    CFStringAppendFormat(result, NULL,
			 CFSTR("<IPConfigurationService %p [%p]> {"),
			 cf, allocator);
    CFStringAppendFormat(result, NULL, CFSTR("ifname = %@, serviceID = %.*s"),
			 service->ifname,
			 (int)CFDataGetLength(service->serviceID_data),
			 CFDataGetBytePtr(service->serviceID_data));
    CFStringAppend(result, CFSTR("}"));
    return (result);
}

STATIC void
__IPConfigurationServiceDeallocate(CFTypeRef cf)
{
    kern_return_t		kret;
    IPConfigurationServiceRef 	service = (IPConfigurationServiceRef)cf;
    inline_data_t 		service_id;
    int				service_id_len;
    ipconfig_status_t		status;

    service_id_len = CFDataGetLength(service->serviceID_data);

    bcopy(CFDataGetBytePtr(service->serviceID_data), &service_id,
	  service_id_len);
    kret = ipconfig_remove_service_with_id(service->server,
					   service_id, service_id_len,
					   &status);
    if (kret != KERN_SUCCESS) {
	IPConfigLogFL(LOG_NOTICE,
		      "ipconfig_remove_service_with_id(%.*s) failed, %s",
		      service_id_len, service_id, mach_error_string(kret));
    }
    else if (status != ipconfig_status_success_e) {
	IPConfigLogFL(LOG_NOTICE,
		      "ipconfig_add_service(%.*s) failed: %s",
		      service_id_len, service_id, 
		      ipconfig_status_string(status));
    }
    mach_port_deallocate(mach_task_self(), service->server);
    service->server = MACH_PORT_NULL;
    my_CFRelease(&service->ifname);
    my_CFRelease(&service->serviceID_data);
    my_CFRelease(&service->store_key);
    return;
}

STATIC void
__IPConfigurationServiceInitialize(void)
{
    /* initialize runtime */
    __kIPConfigurationServiceTypeID 
	= _CFRuntimeRegisterClass(&__IPConfigurationServiceClass);
    return;
}

STATIC void
__IPConfigurationServiceRegisterClass(void)
{
    STATIC pthread_once_t	initialized = PTHREAD_ONCE_INIT;

    pthread_once(&initialized, __IPConfigurationServiceInitialize);
    return;
}

STATIC IPConfigurationServiceRef
__IPConfigurationServiceAllocate(CFAllocatorRef allocator)
{
    IPConfigurationServiceRef	service;
    int				size;

    __IPConfigurationServiceRegisterClass();

    size = sizeof(*service) - sizeof(CFRuntimeBase);
    service = (IPConfigurationServiceRef)
	_CFRuntimeCreateInstance(allocator,
				 __kIPConfigurationServiceTypeID, size, NULL);
    bzero(((void *)service) + sizeof(CFRuntimeBase), size);
    return (service);
}

STATIC CFDictionaryRef
config_dict_create(pid_t pid, CFNumberRef mtu, CFBooleanRef perform_nud)
{
    int			count;
    CFDictionaryRef	config_dict;
    const void *	keys[4];
    CFDictionaryRef	ipv6_dict;
    CFDictionaryRef	options;
    const void *	values[4];

    /* monitor pid */    
    count = 0;
    keys[count] = _kIPConfigurationServiceOptionMonitorPID;
    values[count] = kCFBooleanTrue;
    count++;

    /* no publish */
    keys[count] = _kIPConfigurationServiceOptionNoPublish;
    values[count] = kCFBooleanTrue;
    count++;

    /* mtu */
    if (mtu != NULL) {
	keys[count] = kIPConfigurationServiceOptionMTU;
	values[count] = mtu;
	count++;
    }
    
    /* perform NUD */
    if (perform_nud != NULL) {
	keys[count] = kIPConfigurationServiceOptionPerformNUD;
	values[count] = perform_nud;
	count++;
    }

    options
	= CFDictionaryCreate(NULL, keys, values, count,
			     &kCFTypeDictionaryKeyCallBacks,
			     &kCFTypeDictionaryValueCallBacks);

    /* create the IPv6 dictionary */
    ipv6_dict
	= CFDictionaryCreate(NULL,
			     (const void * *)&kSCPropNetIPv6ConfigMethod,
			     (const void * *)&kSCValNetIPv6ConfigMethodAutomatic,
			     1,
			     &kCFTypeDictionaryKeyCallBacks,
			     &kCFTypeDictionaryValueCallBacks);
    keys[0] = kIPConfigurationServiceOptions;
    values[0] = options;
    keys[1] = kSCEntNetIPv6;
    values[1] = ipv6_dict;
    config_dict
	= CFDictionaryCreate(NULL, keys, values, 2,
			     &kCFTypeDictionaryKeyCallBacks,
			     &kCFTypeDictionaryValueCallBacks);
    CFRelease(options);
    CFRelease(ipv6_dict);
    return (config_dict);
}

/**
 ** IPConfigurationService APIs
 **/

PRIVATE_EXTERN CFTypeID
IPConfigurationServiceGetTypeID(void)
{
    __IPConfigurationServiceRegisterClass();
    return (__kIPConfigurationServiceTypeID);
}

IPConfigurationServiceRef
IPConfigurationServiceCreate(CFStringRef interface_name, 
			     CFDictionaryRef options)
{
    CFDictionaryRef		config_dict;
    CFDataRef			data = NULL;
    if_name_t			if_name;
    kern_return_t		kret;
    CFNumberRef			mtu = NULL;
    CFBooleanRef		perform_nud = NULL;
    mach_port_t			server = MACH_PORT_NULL;
    IPConfigurationServiceRef	service = NULL;
    CFStringRef			serviceID;
    inline_data_t 		service_id;
    unsigned int 		service_id_len;
    ipconfig_status_t		status = ipconfig_status_success_e;
    boolean_t			tried_to_delete = FALSE;
    void *			xml_data_ptr = NULL;
    int				xml_data_len = 0;

    if (options != NULL) {
	mtu = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionMTU);
	if (mtu != NULL && isA_CFNumber(mtu) == NULL) {
	    IPConfigLogFL(LOG_NOTICE, "ignoring invalid '%@' option",
			  kIPConfigurationServiceOptionMTU);
	    mtu = NULL;
	}
	perform_nud 
	    = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionPerformNUD);
	if (perform_nud != NULL && isA_CFBoolean(perform_nud) == NULL) {
	    IPConfigLogFL(LOG_NOTICE, "ignoring invalid '%@' option",
			  kIPConfigurationServiceOptionPerformNUD);
	    perform_nud = NULL;
	}
    }
    kret = ipconfig_server_port(&server);
    if (kret != BOOTSTRAP_SUCCESS) {
	IPConfigLogFL(LOG_NOTICE,
		      "ipconfig_server_port, %s",
		      mach_error_string(kret));
	return (NULL);
    }
    config_dict = config_dict_create(getpid(), mtu, perform_nud);
    data = CFPropertyListCreateXMLData(NULL, config_dict);
    CFRelease(config_dict);
    xml_data_ptr = (void *)CFDataGetBytePtr(data);
    xml_data_len = CFDataGetLength(data);
    my_CFStringToCStringAndLength(interface_name, if_name,
				  sizeof(if_name));
    while (1) {
	kret = ipconfig_add_service(server, if_name, 
				    xml_data_ptr, xml_data_len,
				    service_id, &service_id_len,
				    &status);
	if (kret != KERN_SUCCESS) {
	    IPConfigLogFL(LOG_NOTICE,
			  "ipconfig_add_service(%s) failed, %s",
			  if_name, mach_error_string(kret));
	    goto done;
	}
	if (status != ipconfig_status_duplicate_service_e) {
	    break;
	}
	if (tried_to_delete) {
	    /* already tried once */
	    break;
	}
	tried_to_delete = TRUE;
	(void)ipconfig_remove_service(server, if_name,
				      xml_data_ptr, xml_data_len,
				      &status);
    }
    if (status != ipconfig_status_success_e) {
	IPConfigLogFL(LOG_NOTICE,
		      "ipconfig_add_service(%s) failed: %s",
		      if_name, ipconfig_status_string(status));
	goto done;
    }

    /* allocate/return an IPConfigurationServiceRef */
    service = __IPConfigurationServiceAllocate(NULL);
    if (service == NULL) {
	goto done;
    }
    service->ifname = CFStringCreateCopy(NULL, interface_name);
    service->serviceID_data = CFDataCreate(NULL, (const UInt8 *)service_id,
					   service_id_len);
    service->server = server;
    serviceID 
	= CFStringCreateWithBytes(NULL,
				  CFDataGetBytePtr(service->serviceID_data),
				  CFDataGetLength(service->serviceID_data),
				  kCFStringEncodingASCII, FALSE);
    service->store_key = IPConfigurationServiceKey(serviceID);
    CFRelease(serviceID);
    server = MACH_PORT_NULL;

 done:
    if (server != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), server);
    }
    my_CFRelease(&data);
    return (service);
}

/*
 * Function: IPConfigurationServiceGetNotificationKey
 *
 * Purpose:
 *   Return the SCDynamicStoreKeyRef used to monitor the service using
 *   SCDynamicStoreSetNotificationKeys().
 *
 * Parameters:
 *   service			: the service to monitor
 */
CFStringRef
IPConfigurationServiceGetNotificationKey(IPConfigurationServiceRef service)
{
    return (service->store_key);
}

CFDictionaryRef
IPConfigurationServiceCopyInformation(IPConfigurationServiceRef service)
{
    CFDictionaryRef		info;
    SCDynamicStoreRef		store;

    store = SCDynamicStoreCreate(NULL, CFSTR("IPConfigurationService"),
				 NULL, NULL);
    if (store == NULL) {
	return (NULL);
    }
    info = SCDynamicStoreCopyValue(store, service->store_key);
    if (info != NULL
	&& isA_CFDictionary(info) == NULL) {
	my_CFRelease(&info);
    }
    CFRelease(store);
    return (info);
}
