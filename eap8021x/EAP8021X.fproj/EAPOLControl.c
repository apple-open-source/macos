/*
 * Copyright (c) 2002-2011 Apple Inc. All rights reserved.
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
#include <bsm/audit.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <stdbool.h>
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
#include "EAPOLControlTypesPrivate.h"
#if ! TARGET_OS_EMBEDDED
#include <CoreFoundation/CFPreferences.h>
#include <notify.h>
#include <net/ethernet.h>
#include "EAPOLClientConfigurationPrivate.h"
const CFStringRef	kEAPOLControlAutoDetectInformationNotifyKey = CFSTR("com.apple.network.eapolcontrol.autodetect");
const CFStringRef	kEAPOLAutoDetectSecondsSinceLastPacket = CFSTR("SecondsSinceLastPacket");
const CFStringRef	kEAPOLAutoDetectAuthenticatorMACAddress = CFSTR("AuthenticatorMACAddress");
#endif /* ! TARGET_OS_EMBEDDED */
#include "EAPClientProperties.h"

#ifndef kSCEntNetEAPOL
#define kSCEntNetEAPOL		CFSTR("EAPOL")
#endif /* kSCEntNetEAPOL */

static Boolean
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
    mach_port_t			au_session = MACH_PORT_NULL;
    CFDataRef			data = NULL;
    if_name_t			if_name;
    boolean_t			need_deallocate;
    mach_port_t			server;
    int				result = 0;
    kern_return_t		status;

    status = mach_port_mod_refs(mach_task_self(), bootstrap_port,
				MACH_PORT_RIGHT_SEND, 1);
    if (status != KERN_SUCCESS) {
	need_deallocate = FALSE;
	mach_error("mach_port_mod_refs failed", status);
	result = ENXIO;
	goto done;
    }
    need_deallocate = TRUE;
    au_session = audit_session_self();
    if (au_session == MACH_PORT_NULL) {
	perror("audit_session_port");
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
				   bootstrap_port,
				   au_session,
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
    if (need_deallocate) {
	(void)mach_port_mod_refs(mach_task_self(), bootstrap_port,
				 MACH_PORT_RIGHT_SEND, -1);
    }
    if (au_session != MACH_PORT_NULL) {
	(void)mach_port_deallocate(mach_task_self(), au_session);
    }
    my_CFRelease(&data);
    return (result);
}


#if ! TARGET_OS_EMBEDDED
static Boolean
auth_info_is_valid(CFDictionaryRef * dict_p)
{
    int				count;
    CFDictionaryRef		dict;
    const void *		keys[3];
    int				keys_count = sizeof(keys) / sizeof(keys[0]);

    dict = *dict_p;
    if (dict == NULL) {
	return (TRUE);
    }
    if (isA_CFDictionary(dict) == NULL) {
	return (FALSE);
    }
    count = CFDictionaryGetCount(dict);
    if (count == 0) {
	/* ignore it */
	*dict_p = NULL;
    }
    else if (count > keys_count) {
	return (FALSE);
    }
    else {
	int		i;

	CFDictionaryGetKeysAndValues(dict, keys, NULL);
	for (i = 0; i < count; i++) {
	    if (!CFEqual(keys[i], kEAPClientPropUserName)
		&& !CFEqual(keys[i], kEAPClientPropUserPassword)
		&& !CFEqual(keys[i], kEAPClientPropTLSIdentityHandle)) {
		return (FALSE);
	    }
	}
    }
    return (TRUE);
}

const CFStringRef	kEAPOLControlStartOptionClientItemID = CFSTR("ClientItemID");
const CFStringRef	kEAPOLControlStartOptionManagerName = CFSTR("ManagerName");
const CFStringRef	kEAPOLControlStartOptionAuthenticationInfo = CFSTR("AuthenticationInfo");


int
EAPOLControlStartWithOptions(const char * if_name,
			     EAPOLClientItemIDRef itemID,
			     CFDictionaryRef options)
{
    CFDictionaryRef 		auth_info;
    CFDictionaryRef		config_dict;
    int				count;
    CFDictionaryRef		item_dict;
    const void *		keys[3];
    CFStringRef			manager_name;
    int				ret;
    const void *		values[3];

    auth_info 
	= CFDictionaryGetValue(options, 
			       kEAPOLControlStartOptionAuthenticationInfo);
    if (auth_info_is_valid(&auth_info) == FALSE) {
	return (EINVAL);
    }
    manager_name = CFDictionaryGetValue(options,
					kEAPOLControlStartOptionManagerName);
    if (manager_name != NULL && isA_CFString(manager_name) == NULL) {
	return (EINVAL);
    }
    item_dict = EAPOLClientItemIDCopyDictionary(itemID);
    if (item_dict == NULL) {
	return (EINVAL);
    }
    count = 0;
    keys[count] = (const void *)kEAPOLControlClientItemID;
    values[count] = (const void *)item_dict;
    count++;
    if (auth_info != NULL) {
	keys[count] = (const void *)kEAPOLControlEAPClientConfiguration;
	values[count] = (const void *)auth_info;
	count++;
    }
    if (manager_name != NULL) {
	keys[count] = (const void *)kEAPOLControlManagerName;
	values[count] = (const void *)manager_name;
	count++;
    }
    config_dict = CFDictionaryCreate(NULL, keys, values, count,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFRelease(item_dict);
    ret = EAPOLControlStart(if_name, config_dict);
    CFRelease(config_dict);
    return (ret);
}

int
EAPOLControlStartWithClientItemID(const char * if_name,
				  EAPOLClientItemIDRef itemID,
				  CFDictionaryRef auth_info)
{
    CFDictionaryRef	config_dict;
    int			count;
    CFDictionaryRef	item_dict;
    const void *	keys[2];
    int			ret;
    const void *	values[2];

    if (auth_info_is_valid(&auth_info) == FALSE) {
	return (EINVAL);
    }
    item_dict = EAPOLClientItemIDCopyDictionary(itemID);
    if (item_dict == NULL) {
	return (EINVAL);
    }

    keys[0] = (const void *)kEAPOLControlClientItemID;
    values[0] = (const void *)item_dict;
    count = 1;
    if (auth_info != NULL) {
	keys[1] = (const void *)kEAPOLControlEAPClientConfiguration;
	values[1] = (const void *)auth_info;
	count = 2;
    }
    config_dict = CFDictionaryCreate(NULL, keys, values, count,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFRelease(item_dict);
    ret = EAPOLControlStart(if_name, config_dict);
    CFRelease(config_dict);
    return (ret);
}

#endif /* ! TARGET_OS_EMBEDDED */

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

int
EAPOLControlStartSystemWithClientItemID(const char * interface_name,
					EAPOLClientItemIDRef itemID)
{
    CFDictionaryRef	config_dict;
    CFDictionaryRef	item_dict;
    CFStringRef		key;
    int			ret;

    item_dict = EAPOLClientItemIDCopyDictionary(itemID);
    if (item_dict == NULL) {
	return (EINVAL);
    }
    key = kEAPOLControlClientItemID;
    config_dict = CFDictionaryCreate(NULL,
				     (const void * *)&key, 
				     (const void * *)&item_dict,
				     1,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFRelease(item_dict);
    ret = EAPOLControlStartSystem(interface_name, config_dict);
    CFRelease(config_dict);
    return (ret);
}

static int
S_copy_loginwindow_config(const char * interface_name,
			  CFDictionaryRef * ret_config_p)
{
    CFDictionaryRef		dict = NULL;
    if_name_t			if_name;
    int				result = 0;
    mach_port_t			server;
    kern_return_t		status;
    xmlDataOut_t		config = NULL;
    unsigned int		config_len = 0;

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
    dict = my_CFPropertyListCreateWithBytePtrAndLength(config,
						       config_len);
    (void)vm_deallocate(mach_task_self(), (vm_address_t)config,
			config_len);
    if (dict == NULL) {
	result = ENOMEM;
	goto done;
    }

 done:
    *ret_config_p = dict;
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
    CFDictionaryRef		dict = NULL;
    int				result = 0;

    result = S_copy_loginwindow_config(interface_name, &dict);
    if (result != 0) {
	goto done;
    }

    /* if this contains an EAPOLClientItemID dictionary, return NULL */
    if (CFDictionaryContainsKey(dict, kEAPOLControlClientItemID)) {
	my_CFRelease(&dict);
	result = ENOENT;
    }

 done:
    *ret_config_p = dict;
    return (result);
}

/*
 * Function: EAPOLControlCopyLoginWindowClientItemID
 *
 * Purpose:
 *   If LoginWindow mode is activated during this login session, returns the
 *   EAPOLClientItemIDRef corresponding to the profile that was used.  The
 *   information is cleared when the user logs out.
 *
 * Returns:
 *   0 and non-NULL EAPOLClientItemIDRef on success,
 *   non-zero errno on failure.
 */
int
EAPOLControlCopyLoginWindowClientItemID(const char * interface_name,
					EAPOLClientItemIDRef * itemID_p)
{
    EAPOLClientConfigurationRef	cfg;
    CFDictionaryRef		dict = NULL;
    EAPOLClientItemIDRef	itemID = NULL;
    CFDictionaryRef		itemID_dict;
    int				result = 0;

    result = S_copy_loginwindow_config(interface_name, &dict);
    if (result != 0) {
	goto done;
    }
    itemID_dict = CFDictionaryGetValue(dict, kEAPOLControlClientItemID);
    if (itemID_dict == NULL) {
	result = ENOENT;
	goto done;
    }
    cfg = EAPOLClientConfigurationCreate(NULL);
    if (cfg != NULL) {
	itemID = EAPOLClientItemIDCreateWithDictionary(cfg, itemID_dict);
	CFRelease(cfg);
    }
    if (itemID == NULL) {
	result = ENOENT;
    }

 done:
    my_CFRelease(&dict);
    *itemID_p = itemID;
    return (result);
}

int
EAPOLControlCopyAutoDetectInformation(CFDictionaryRef * info_p)
{
    int				result = 0;
    mach_port_t			server;
    kern_return_t		status;
    xmlDataOut_t		info_data = NULL;
    unsigned int		info_data_len = 0;

    *info_p = NULL;
    if (get_server_port(&server, &status) == FALSE) {
	result = ENXIO;
	goto done;
    }
    status = eapolcontroller_copy_autodetect_info(server,
						  &info_data, &info_data_len,
						  &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_copy_autodetect_info failed", status);
	result = ENXIO;
	goto done;
    }
    if (info_data != NULL) {
	*info_p =
	    my_CFPropertyListCreateWithBytePtrAndLength(info_data, 
							info_data_len);
	(void)vm_deallocate(mach_task_self(), (vm_address_t)info_data, 
			    info_data_len);
	if (*info_p == NULL) {
	    result = ENOMEM;
	    goto done;
	}
    }
    
 done:
    return (result);
}

boolean_t
EAPOLControlDidUserCancel(const char * interface_name)
{
    boolean_t			cancelled = FALSE;
    if_name_t			if_name;
    mach_port_t			server;
    kern_return_t		status;

    if (get_server_port(&server, &status) == FALSE) {
	goto done;
    }
    strncpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_did_user_cancel(server,
					     if_name,
					     &cancelled);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_copy_loginwindow_config failed", status);
    }
 done:
    return (cancelled);
}

#define kEthernetAutoConnect		CFSTR("EthernetAutoConnect")
#define kEthernetAutoConnectIsVerbose	CFSTR("EthernetAutoConnectIsVerbose")
#define kEthernetAuthenticatorList	CFSTR("EthernetAuthenticatorList")

#define kProfileID			CFSTR("ProfileID")

#define kEAPOLControlUserSettingsApplicationID	CFSTR("com.apple.network.eapolcontrol")

const char * kEAPOLControlUserSettingsNotifyKey = "com.apple.network.eapolcontrol.user";

static int	token;
static bool	token_valid;

static void
settings_change_check(void)
{
    int		check = 0;
    uint32_t	status;

    if (!token_valid) {
	status = notify_register_check(kEAPOLControlUserSettingsNotifyKey,
				       &token);
	if (status != NOTIFY_STATUS_OK) {
	    syslog(LOG_NOTICE,
		   "EAPOLControl: notify_register_check returned %d", status);
	    return;
	}
	token_valid = TRUE;
    }
    status = notify_check(token, &check);
    if (status != NOTIFY_STATUS_OK) {
	syslog(LOG_NOTICE,
	       "EAPOLControl: notify_check returned %d",
	       status);
	return;
    }
    if (check != 0) {
	CFPreferencesSynchronize(kEAPOLControlUserSettingsApplicationID,
				 kCFPreferencesCurrentUser,
				 kCFPreferencesCurrentHost);
    }
    return;
}

static void
settings_change_notify(void)
{
    uint32_t	status;

    status = notify_post(kEAPOLControlUserSettingsNotifyKey);
    if (status != NOTIFY_STATUS_OK) {
	syslog(LOG_NOTICE,
	       "EAPOLControl: notify_post returned %d", status);
    }
    return;
}

static CFPropertyListRef
EAPOLControlUserCopyValue(CFStringRef key)
{
    settings_change_check();
    return (CFPreferencesCopyValue(key,
				   kEAPOLControlUserSettingsApplicationID,
				   kCFPreferencesCurrentUser,
				   kCFPreferencesCurrentHost));
}

static Boolean
EAPOLControlUserGetBooleanValue(CFStringRef key, Boolean def_value)
{
    Boolean		result;
    CFBooleanRef	val;

    val = EAPOLControlUserCopyValue(key);
    if (isA_CFBoolean(val) == NULL) {
	result = def_value;
    }
    else {
	result = CFBooleanGetValue(val);
    }
    my_CFRelease(&val);
    return (result);
}

void
EAPOLControlUserSetBooleanValue(CFStringRef key, Boolean enable)
{
    CFPreferencesSetValue(key,
			  enable ? kCFBooleanTrue : kCFBooleanFalse,
			  kEAPOLControlUserSettingsApplicationID,
			  kCFPreferencesCurrentUser,
			  kCFPreferencesCurrentHost);
    CFPreferencesSynchronize(kEAPOLControlUserSettingsApplicationID,
			     kCFPreferencesCurrentUser,
			     kCFPreferencesCurrentHost);

    settings_change_notify();
    return;
}

void
EAPOLControlSetUserAutoConnectEnabled(Boolean enable)
{
    EAPOLControlUserSetBooleanValue(kEthernetAutoConnect, enable);
    return;
}

Boolean
EAPOLControlIsUserAutoConnectEnabled(void)
{
    return (EAPOLControlUserGetBooleanValue(kEthernetAutoConnect, TRUE));
}

void
EAPOLControlSetUserAutoConnectVerboseEnabled(Boolean enable)
{
    EAPOLControlUserSetBooleanValue(kEthernetAutoConnectIsVerbose, enable);
    return;
}

Boolean
EAPOLControlIsUserAutoConnectVerboseEnabled(void)
{
    return (EAPOLControlUserGetBooleanValue(kEthernetAutoConnectIsVerbose,
					    FALSE));
}


#define EA_FORMAT	"%02x:%02x:%02x:%02x:%02x:%02x"
#define EA_CH(e, i)	((u_char)((u_char *)(e))[(i)])
#define EA_LIST(ea)	EA_CH(ea,0),EA_CH(ea,1),EA_CH(ea,2),EA_CH(ea,3),EA_CH(ea,4),EA_CH(ea,5)

static CFStringRef
S_authenticator_copy_string(CFDataRef authenticator)
{
    const UInt8 *	bytes;

    if (CFDataGetLength(authenticator) != sizeof(struct ether_addr)) {
	return (NULL);
    }
    bytes = CFDataGetBytePtr(authenticator);
    return (CFStringCreateWithFormat(NULL, NULL, CFSTR(EA_FORMAT),
				     EA_LIST(bytes)));
}

EAPOLClientItemIDRef
EAPOLControlCopyItemIDForAuthenticator(CFDataRef authenticator)
{
    CFDictionaryRef		binding;
    CFDictionaryRef		list;
    EAPOLClientItemIDRef	itemID = NULL;
    CFStringRef			str;

    list = EAPOLControlUserCopyValue(kEthernetAuthenticatorList);
    if (isA_CFDictionary(list) == NULL) {
	goto done;
    }
    str = S_authenticator_copy_string(authenticator);
    binding = CFDictionaryGetValue(list, str);
    if (isA_CFDictionary(binding) != NULL) {
	CFStringRef	profileID;

	profileID = CFDictionaryGetValue(binding, kProfileID);
	if (isA_CFString(profileID) != NULL) {
	    EAPOLClientConfigurationRef		cfg;
	    EAPOLClientProfileRef		profile;

	    cfg = EAPOLClientConfigurationCreate(NULL);
	    profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
	    if (profile != NULL) {
		itemID = EAPOLClientItemIDCreateWithProfile(profile);
	    }
	    else {
		/* profile is no longer present */
	    }
	    CFRelease(cfg);
	}
	else {
	    itemID = EAPOLClientItemIDCreateDefault();
	}
    }
    CFRelease(str);

 done:
    my_CFRelease(&list);
    return (itemID);
}

void
EAPOLControlSetItemIDForAuthenticator(CFDataRef authenticator,
				      EAPOLClientItemIDRef itemID)
{
    CFDictionaryRef		binding = NULL;
    Boolean			changed = FALSE;
    CFDictionaryRef		list;
    CFStringRef			profileID = NULL;
    CFStringRef			str;

    list = EAPOLControlUserCopyValue(kEthernetAuthenticatorList);
    str = S_authenticator_copy_string(authenticator);
    if (itemID == NULL) {
	/* if there was a previous binding, we need to remove it */
	if (list != NULL && CFDictionaryGetValue(list, str) != NULL) {
	    changed = TRUE;
	}
    }
    else {
	EAPOLClientProfileRef	profile = EAPOLClientItemIDGetProfile(itemID);
	CFStringRef		prev_profileID = NULL;

	if (profile != NULL) {
	    profileID = EAPOLClientProfileGetID(profile);
	}
	if (list != NULL) {
	    binding = isA_CFDictionary(CFDictionaryGetValue(list, str));
	    if (binding != NULL) {
		prev_profileID 
		    = isA_CFString(CFDictionaryGetValue(binding, kProfileID));
	    }
	}
	if (my_CFEqual(prev_profileID, profileID) == FALSE) {
	    changed = TRUE;
	}
	else if (binding == NULL && profile == NULL) {
	    /* no existing binding, and the user chose Default */
	    changed = TRUE;
	}
    }
    if (changed) {
	CFMutableDictionaryRef	new_list;

	if (isA_CFDictionary(list) != NULL) {
	    new_list = CFDictionaryCreateMutableCopy(NULL, 0, list);
	}
	else {
	    new_list 
		= CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	}
	if (itemID == NULL) {
	    /* clear the binding */
	    CFDictionaryRemoveValue(new_list, str);
	}
	else {
	    CFMutableDictionaryRef	new_binding = NULL;

	    if (binding != NULL) {
		new_binding = CFDictionaryCreateMutableCopy(NULL, 0, binding);
	    }
	    else {
		new_binding 
		    = CFDictionaryCreateMutable(NULL, 0,
						&kCFTypeDictionaryKeyCallBacks,
						&kCFTypeDictionaryValueCallBacks);
	    }
	    if (profileID != NULL) {
		CFDictionarySetValue(new_binding, kProfileID, profileID);
	    }
	    else {
		CFDictionaryRemoveValue(new_binding, kProfileID);
	    }
	    CFDictionarySetValue(new_list, str, new_binding);
	    CFRelease(new_binding);
	    
	}
	CFPreferencesSetValue(kEthernetAuthenticatorList,
			      new_list,
			      kEAPOLControlUserSettingsApplicationID,
			      kCFPreferencesCurrentUser,
			      kCFPreferencesCurrentHost);
	CFPreferencesSynchronize(kEAPOLControlUserSettingsApplicationID,
				 kCFPreferencesCurrentUser,
				 kCFPreferencesCurrentHost);
	settings_change_notify();
	CFRelease(new_list);
    }
    my_CFRelease(&list);
    my_CFRelease(&str);
    return;
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
