
/*
 * Copyright (c) 2002-2009 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/boolean.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <CoreFoundation/CFMachPort.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "eapolcontroller.h"
#include "eapolcontroller_types.h"
#include "eapolcontroller_ext.h"
#include "myCFUtil.h"
#include "EAPOLControl.h"
#include "EAPOLControlTypes.h"

#ifndef kSCEntNetEAPOL
#define kSCEntNetEAPOL		CFSTR("EAPOL")
#endif kSCEntNetEAPOL

static boolean_t
get_server_port(mach_port_t * server, kern_return_t * status)
{
    *status = eapolcontroller_server_port(server);
    if (*status != BOOTSTRAP_SUCCESS) {
	fprintf(stderr, "eapolcontroller_server_port failed, %s\n", 
		mach_error_string(*status));
	return (FALSE);
    }
    return (TRUE);
}

int
EAPOLControlStart(const char * interface_name, CFDictionaryRef config_dict)
{
    CFDataRef			data = NULL;
    if_name_t			if_name;
    mach_port_t			server;
    int				result = 0;
    kern_return_t		status;
    mach_port_t 		unpriv_bootstrap_port = MACH_PORT_NULL;
    
    status = bootstrap_unprivileged(bootstrap_port, &unpriv_bootstrap_port);
    if (status != KERN_SUCCESS) {
	mach_error("bootstrap_unprivileged failed", status);
	result = ENXIO;
	goto done;
    }
    if (get_server_port(&server, &status) == FALSE) {
	result = ENXIO;
	goto done;
    }
    if (isA_CFDictionary(config_dict) == NULL) {
	result = EINVAL;
	goto done;
    }
    data = CFPropertyListCreateXMLData(NULL, config_dict);
    if (data == NULL) {
	result = ENOMEM;
	goto done;
    }
    strncpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_start(server,
				   if_name, 
				   (xmlDataOut_t)CFDataGetBytePtr(data),
				   CFDataGetLength(data),
				   unpriv_bootstrap_port,
				   &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_start failed", status);
	result = ENXIO;
	goto done;
    }
    if (result != 0) {
	fprintf(stderr, "eapolcontroller_start: result is %d\n", result);
    }
 done:
    if (unpriv_bootstrap_port != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), unpriv_bootstrap_port);
    }
    my_CFRelease(&data);
    return (result);
}

int
EAPOLControlStop(const char * interface_name)
{
    if_name_t			if_name;
    mach_port_t			server;
    int				result = 0;
    kern_return_t		status;

    if (get_server_port(&server, &status) == FALSE) {
	result = ENXIO;
	goto done;
    }
    strncpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_stop(server,
				  if_name, &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_start failed", status);
	result = ENXIO;
	goto done;
    }
 done:
    return (result);
}

int
EAPOLControlUpdate(const char * interface_name, CFDictionaryRef config_dict)
{
    CFDataRef			data = NULL;
    if_name_t			if_name;
    mach_port_t			server;
    int				result = 0;
    kern_return_t		status;

    if (get_server_port(&server, &status) == FALSE) {
	result = ENXIO;
	goto done;
    }
    if (isA_CFDictionary(config_dict) == NULL) {
	result = EINVAL;
	goto done;
    }
    data = CFPropertyListCreateXMLData(NULL, config_dict);
    if (data == NULL) {
	result = ENOMEM;
	goto done;
    }
    strncpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_update(server, if_name,
				    (xmlDataOut_t)CFDataGetBytePtr(data),
				    CFDataGetLength(data),
				    &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_update failed", status);
	result = ENXIO;
	goto done;
    }
 done:
    my_CFRelease(&data);
    return (result);
}

int
EAPOLControlRetry(const char * interface_name)
{
    if_name_t			if_name;
    int				result = 0;
    mach_port_t			server;
    kern_return_t		status;

    if (get_server_port(&server, &status) == FALSE) {
	result = ENXIO;
	goto done;
    }
    strncpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_retry(server, if_name, &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_retry failed", status);
	result = ENXIO;
	goto done;
    }
 done:
    return (result);
}

int
EAPOLControlProvideUserInput(const char * interface_name, 
			     CFDictionaryRef user_input)
{
    CFDataRef			data = NULL;
    if_name_t			if_name;
    mach_port_t			server;
    int				result = 0;
    kern_return_t		status;
    xmlDataOut_t		xml_data = NULL;
    CFIndex			xml_data_len = 0;

    if (get_server_port(&server, &status) == FALSE) {
	result = ENXIO;
	goto done;
    }
    if (user_input != NULL) {
	if (isA_CFDictionary(user_input) == NULL) {
	    result = EINVAL;
	    goto done;
	}
	data = CFPropertyListCreateXMLData(NULL, user_input);
	if (data == NULL) {
	    result = ENOMEM;
	    goto done;
	}
	xml_data = (xmlDataOut_t)CFDataGetBytePtr(data);
	xml_data_len = CFDataGetLength(data);
    }
    strncpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_provide_user_input(server,
						if_name, xml_data,
						xml_data_len, &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_provide_user_input failed", status);
	result = ENXIO;
	goto done;
    }
 done:
    my_CFRelease(&data);
    return (result);
}

int
EAPOLControlCopyStateAndStatus(const char * interface_name, 
			       EAPOLControlState * state,
			       CFDictionaryRef * status_dict_p)
{
    if_name_t			if_name;
    int				result = 0;
    mach_port_t			server;
    kern_return_t		status;
    xmlDataOut_t		status_data = NULL;
    unsigned int		status_data_len = 0;

    *status_dict_p = NULL;
    if (get_server_port(&server, &status) == FALSE) {
	result = ENXIO;
	goto done;
    }
    strncpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_copy_status(server,
					 if_name,
					 &status_data, &status_data_len,
					 (int *)state, &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_copy_status failed", status);
	result = ENXIO;
	goto done;
    }
    if (status_data != NULL) {
	*status_dict_p =
	    my_CFPropertyListCreateWithBytePtrAndLength(status_data, 
							status_data_len);
	(void)vm_deallocate(mach_task_self(), (vm_address_t)status_data, 
			    status_data_len);
	if (*status_dict_p == NULL) {
	    result = ENOMEM;
	    goto done;
	}
    }
    
 done:
    return (result);
}

int
EAPOLControlSetLogLevel(const char * interface_name, int32_t level)
{
    if_name_t			if_name;
    mach_port_t			server;
    int				result = 0;
    kern_return_t		status;

    if (get_server_port(&server, &status) == FALSE) {
	result = ENXIO;
	goto done;
    }

    strncpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_set_logging(server, 
					 if_name, 
					 level, &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_set_logging failed", status);
	result = ENXIO;
	goto done;
    }
 done:
    return (result);
}

CFStringRef
EAPOLControlKeyCreate(const char * interface_name)
{
    CFStringRef		if_name_cf;
    CFStringRef		str;

    if_name_cf = CFStringCreateWithCString(NULL, interface_name,
					   kCFStringEncodingASCII);
    str	= SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							if_name_cf,
							kSCEntNetEAPOL);
    my_CFRelease(&if_name_cf);
    return (str);
}

#if ! TARGET_OS_EMBEDDED
int
EAPOLControlStartSystem(const char * interface_name, CFDictionaryRef options)
{
    CFDataRef			data = NULL;
    if_name_t			if_name;
    mach_port_t			server;
    int				result = 0;
    kern_return_t		status;
    xmlDataOut_t		xml_data = NULL;
    CFIndex			xml_data_len = 0;
    
    if (get_server_port(&server, &status) == FALSE) {
	result = ENXIO;
	goto done;
    }
    if (options != NULL) {
	if (isA_CFDictionary(options) == NULL) {
	    result = EINVAL;
	    goto done;
	}
	data = CFPropertyListCreateXMLData(NULL, options);
	if (data == NULL) {
	    result = ENOMEM;
	    goto done;
	}
	xml_data = (xmlDataOut_t)CFDataGetBytePtr(data);
	xml_data_len = CFDataGetLength(data);
    }
    strncpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_start_system(server, if_name,
					  xml_data, xml_data_len, 
					  &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_start_system failed", status);
	result = ENXIO;
	goto done;
    }
    if (result != 0) {
	fprintf(stderr, "eapolcontroller_start_system: result is %d\n", result);
    }
 done:
    my_CFRelease(&data);
    return (result);
}

/*
 * Function: EAPOLControlCopyLoginWindowConfiguration
 * Purpose:
 *   If LoginWindow mode is activated during this login session, returns the
 *   configuration that was used.  This value is cleared when the user logs out.
 *
 * Returns:
 *   0 and non-NULL CFDictionaryRef value in *config_p on success,
 *   non-zero on failure
 */
int
EAPOLControlCopyLoginWindowConfiguration(const char * interface_name,
					 CFDictionaryRef * ret_config_p)
{
    if_name_t			if_name;
    int				result = 0;
    mach_port_t			server;
    kern_return_t		status;
    xmlDataOut_t		config = NULL;
    unsigned int		config_len = 0;

    *ret_config_p = NULL;
    if (get_server_port(&server, &status) == FALSE) {
	result = ENXIO;
	goto done;
    }
    strncpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_copy_loginwindow_config(server,
						     if_name,
						     &config,
						     &config_len,
						     &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_copy_loginwindow_config failed", status);
	result = ENXIO;
	goto done;
    }
    if (result != 0 || config == NULL) {
	if (result == 0) {
	    /* should not happen */
	    result = ENOENT;
	}
	goto done;
    }
    *ret_config_p = my_CFPropertyListCreateWithBytePtrAndLength(config,
								config_len);
    (void)vm_deallocate(mach_task_self(), (vm_address_t)config,
			config_len);
    if (*ret_config_p == NULL) {
	result = ENOMEM;
	goto done;
    }
    
 done:
    return (result);
}
#endif /* ! TARGET_OS_EMBEDDED */

CFStringRef
EAPOLControlAnyInterfaceKeyCreate(void)
{
    CFStringRef str;
    str	= SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetEAPOL);
    return (str);
}

/*
 * Function: parse_component
 * Purpose:
 *   Given a string 'key' and a string prefix 'prefix',
 *   return the next component in the slash '/' separated
 *   key.  If no slash follows the prefix, return NULL.
 *
 * Examples:
 * 1. key = "a/b/c" prefix = "a/"
 *    returns "b"
 * 2. key = "a/b/c" prefix = "a/b/"
 *    returns NULL
 */
static CFStringRef
parse_component(CFStringRef key, CFStringRef prefix)
{
    CFMutableStringRef	comp;
    CFRange		range;

    if (CFStringHasPrefix(key, prefix) == FALSE) {
	return (NULL);
    }
    comp = CFStringCreateMutableCopy(NULL, 0, key);
    if (comp == NULL) {
	return (NULL);
    }
    CFStringDelete(comp, CFRangeMake(0, CFStringGetLength(prefix)));
    range = CFStringFind(comp, CFSTR("/"), 0);
    if (range.location == kCFNotFound) {
	CFRelease(comp);
	return (NULL);
    }
    range.length = CFStringGetLength(comp) - range.location;
    CFStringDelete(comp, range);
    return (comp);
}

CFStringRef
EAPOLControlKeyCopyInterface(CFStringRef key)
{
    static CFStringRef prefix = NULL;

    if (CFStringHasSuffix(key, kSCEntNetEAPOL) == FALSE) {
	return (NULL);
    }
    if (prefix == NULL) {
	prefix = SCDynamicStoreKeyCreate(NULL,
					 CFSTR("%@/%@/%@/"), 
					 kSCDynamicStoreDomainState,
					 kSCCompNetwork,
					 kSCCompInterface);
    }
    return (parse_component(key, prefix));
}
