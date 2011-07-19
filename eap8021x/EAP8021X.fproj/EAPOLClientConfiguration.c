/*
 * Copyright (c) 2009, 2010 Apple Inc. All rights reserved.
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
 * EAPOLClientConfiguration.c
 * - implementation of the EAPOLClientConfiguration CF object
 */

/* 
 * Modification History
 *
 * December 8, 2009	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <TargetConditionals.h>
#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SCNetworkConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <notify.h>
#include "EAP.h"
#include "EAPClientProperties.h"
#include "EAPOLControlTypes.h"
#include "EAPOLClientConfigurationInternal.h"
#include "symbol_scope.h"
#include "myCFUtil.h"

#define kPrefsName CFSTR("EAPOLClientConfiguration")

/* used with notify(3) to detect configuration changes */
const char * 	kEAPOLClientConfigurationChangedNotifyKey = "com.apple.network.eapolclientconfiguration";

/**
 ** 802.1X Profiles Schema
 **/
#define kConfigurationKeyProfiles 	CFSTR("Profiles")
#define kConfigurationKeyDefaultAuthenticationProperties	CFSTR("DefaultAuthenticationProperties")

/**
 ** 802.1X Network Prefs Schema
 **/
#define kEAPOL				CFSTR("EAPOL")
#define kSystemProfileID		CFSTR("SystemProfileID")
#define kLoginWindowProfileIDs		CFSTR("LoginWindowProfileIDs")

/**
 ** Constant CFStrings
 **/
const CFStringRef	kEAPOLClientProfileWLANSecurityTypeWEP = CFSTR("WEP");
const CFStringRef	kEAPOLClientProfileWLANSecurityTypeWPA = CFSTR("WPA");
const CFStringRef	kEAPOLClientProfileWLANSecurityTypeWPA2 = CFSTR("WPA2");
const CFStringRef	kEAPOLClientProfileWLANSecurityTypeAny = CFSTR("Any");

/**
 ** Utility Functions
 **/

STATIC SCPreferencesRef
get_sc_prefs(EAPOLClientConfigurationRef cfg)
{
    if (cfg->sc_prefs == NULL) {
	cfg->sc_prefs = SCPreferencesCreate(NULL, kPrefsName, NULL);
	if (cfg->sc_prefs == NULL) {
	    syslog(LOG_NOTICE,
		   "EAPOLClientConfiguration: SCPreferencesCreate failed, %s",
		   SCErrorString(SCError()));
	}
    }
    return (cfg->sc_prefs);
}

/*
 * Function: copy_configured_interface_names
 *
 * Purpose:
 *   Return the BSD interface name of all configured interfaces.  If
 *   'entity' is non-NULL, also check that the interface has the specified
 *   extended configuration.
 */
STATIC CFArrayRef /* of CFStringRef */
copy_configured_interface_names(SCPreferencesRef prefs, CFStringRef entity_name)
{
    SCNetworkSetRef		current_set = NULL;
    int				count;
    int				i;
    CFMutableArrayRef		ret_names = NULL;
    CFRange			ret_names_range = { 0 , 0 };
    CFArrayRef			services = NULL;

    if (prefs == NULL) {
	goto done;
    }
    current_set = SCNetworkSetCopyCurrent(prefs);
    if (current_set == NULL) {
	goto done;
    }
    services = SCNetworkSetCopyServices(current_set);
    if (services == NULL) {
	goto done;
    }

    count = CFArrayGetCount(services);
    for (i = 0; i < count; i++) {
	CFStringRef		this_if_name;
	SCNetworkInterfaceRef	this_if;
	SCNetworkServiceRef	s;

	s = (SCNetworkServiceRef)CFArrayGetValueAtIndex(services, i);
	this_if = SCNetworkServiceGetInterface(s);
	if (this_if == NULL) {
	    continue;
	}
	if (entity_name != NULL
	    && (SCNetworkInterfaceGetExtendedConfiguration(this_if, entity_name)
		== NULL)) {
	    /* interface doesn't have specified entity */
	    continue;
	}
	this_if_name = SCNetworkInterfaceGetBSDName(this_if);
	if (this_if_name == NULL) {
	    continue;
	}
	if (ret_names == NULL
	    || CFArrayContainsValue(ret_names, ret_names_range,
				    this_if_name) == FALSE) {
	    if (ret_names == NULL) {
		ret_names = CFArrayCreateMutable(NULL, count, 
						 &kCFTypeArrayCallBacks);
	    }
	    CFArrayAppendValue(ret_names, this_if_name);
	    ret_names_range.length++;
	}
    }

 done:
    my_CFRelease(&current_set);
    my_CFRelease(&services);
    return (ret_names);
}

STATIC CFDictionaryRef
copy_profiles_for_mode(EAPOLClientConfigurationRef cfg,
		       EAPOLControlMode mode)
{
    CFArrayRef			all_names;
    int				count;
    int				i;
    CFMutableDictionaryRef	ret_profiles = NULL;

    switch (mode) {
    case kEAPOLControlModeLoginWindow:
    case kEAPOLControlModeSystem:
	break;
    default:
	return (NULL);
    }
    all_names = copy_configured_interface_names(get_sc_prefs(cfg), kEAPOL);
    if (all_names == NULL) {
	return (NULL);
    }
    count = CFArrayGetCount(all_names);
    for (i = 0; i < count; i++) {
	CFStringRef		if_name = CFArrayGetValueAtIndex(all_names, i);
	CFTypeRef		val = NULL;

	if (mode == kEAPOLControlModeSystem) {
	    EAPOLClientProfileRef	profile;

	    profile = EAPOLClientConfigurationGetSystemProfile(cfg, if_name);
	    if (profile != NULL) {
		val = profile;
		CFRetain(val);
	    }
	}
	else {
	    CFArrayRef			profiles;

	    profiles 
		= EAPOLClientConfigurationCopyLoginWindowProfiles(cfg,
								  if_name);
	    if (profiles != NULL) {
		val = profiles;
	    }

	}
	if (val != NULL) {
	    if (ret_profiles == NULL) {
		ret_profiles
		    = CFDictionaryCreateMutable(NULL, 0,
						&kCFTypeDictionaryKeyCallBacks,
						&kCFTypeDictionaryValueCallBacks);
	    }
	    CFDictionarySetValue(ret_profiles, if_name, val);
	    CFRelease(val);
	}
    }
    CFRelease(all_names);
    return (ret_profiles);
}

STATIC SCNetworkInterfaceRef
copy_configured_interface(SCPreferencesRef prefs, CFStringRef if_name)
{
    SCNetworkSetRef		current_set = NULL;
    int				count;
    int				i;
    SCNetworkInterfaceRef	ret_if = NULL;
    CFArrayRef			services = NULL;
    
    current_set = SCNetworkSetCopyCurrent(prefs);
    if (current_set == NULL) {
	goto done;
    }
    services = SCNetworkSetCopyServices(current_set);
    if (services == NULL) {
	goto done;
    }
    count = CFArrayGetCount(services);
    for (i = 0; i < count; i++) {
	CFStringRef		this_if_name;
	SCNetworkInterfaceRef	this_if;
	SCNetworkServiceRef	s;

	s = (SCNetworkServiceRef)CFArrayGetValueAtIndex(services, i);
	this_if = SCNetworkServiceGetInterface(s);
	if (this_if == NULL) {
	    continue;
	}
	this_if_name = SCNetworkInterfaceGetBSDName(this_if);
	if (this_if_name == NULL) {
	    continue;
	}
	if (CFEqual(this_if_name, if_name)) {
	    ret_if = this_if;
	    CFRetain(ret_if);
	    break;
	}
    }

 done:
    my_CFRelease(&current_set);
    my_CFRelease(&services);
    return (ret_if);
}

/*
 * Function: copy_present_interface
 * Purpose:
 *   Check the list of interfaces on the system, and find the one corresponding
 *   to the specified BSD name.
 */
STATIC SCNetworkInterfaceRef
copy_present_interface(CFStringRef if_name)
{
    int				count = 0;
    int				i;
    CFArrayRef			list;
    SCNetworkInterfaceRef	ret = NULL;

    list = SCNetworkInterfaceCopyAll();
    if (list != NULL) {
	count = CFArrayGetCount(list);
    }
    if (count == 0) {
	goto done;
    }
    for (i = 0; i < count; i++) {
	SCNetworkInterfaceRef	this_if;
	CFStringRef		this_if_name;

	this_if = (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(list, i);
	this_if_name = SCNetworkInterfaceGetBSDName(this_if);
	if (this_if_name == NULL) {
	    continue;
	}
	if (CFEqual(if_name, this_if_name)) {
	    ret = this_if;
	    CFRetain(ret);
	    break;
	}
    }

 done:
    my_CFRelease(&list);
    return (ret);
}

STATIC void
import_profiles(EAPOLClientConfigurationRef cfg)
{
    int					count = 0;
    int					i;
    const void * *			keys;
    CFDictionaryRef			prefs_dict;
    CFMutableDictionaryRef		profiles_dict;
    CFMutableDictionaryRef		ssids_dict;
    const void * *			values;

    profiles_dict = CFDictionaryCreateMutable(NULL, 0,
					      &kCFTypeDictionaryKeyCallBacks,
					      &kCFTypeDictionaryValueCallBacks);
    ssids_dict = CFDictionaryCreateMutable(NULL, 0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
    prefs_dict = SCPreferencesGetValue(cfg->eap_prefs,
				       kConfigurationKeyProfiles);
    if (isA_CFDictionary(prefs_dict) != NULL) {
	count = CFDictionaryGetCount(prefs_dict);
    }
    if (count == 0) {
	goto done;
    }

    /* allocate a single array, half for keys, half for values */
    keys = (const void * *)malloc(sizeof(*keys) * count * 2);
    values = keys + count;
    CFDictionaryGetKeysAndValues(prefs_dict, keys, values);
    for (i = 0; i < count; i++) {
	EAPOLClientProfileRef	profile;
	CFDictionaryRef		profile_dict = values[i];
	CFStringRef		profileID = keys[i];
	CFDataRef		ssid;

	if (isA_CFDictionary(profile_dict) == NULL) {
	    SCLog(TRUE, LOG_NOTICE, 
		  CFSTR("EAPOLClientConfiguration: invalid profile with id %@"),
		  profileID);
	    continue;
	}
	profile = EAPOLClientProfileCreateWithDictAndProfileID(profile_dict,
							       profileID);
	if (profile == NULL) {
	    continue;
	}
	ssid = EAPOLClientProfileGetWLANSSIDAndSecurityType(profile, NULL);
	if (ssid != NULL) {
	    CFStringRef		conflicting_profileID;

	    conflicting_profileID = CFDictionaryGetValue(ssids_dict, ssid);
	    if (conflicting_profileID != NULL) {
		CFStringRef	ssid_str = my_CFStringCreateWithData(ssid);

		SCLog(TRUE, LOG_NOTICE, 
		      CFSTR("EAPOLClientConfiguration: ignoring profile %@: "
			    "SSID '%@' already used by %@"),
		      profileID, ssid_str, conflicting_profileID);
		CFRelease(ssid_str);
		CFRelease(profile);
		continue;
	    }
	    CFDictionarySetValue(ssids_dict, ssid, profileID);
	}
	CFDictionarySetValue(profiles_dict, profileID, profile);
	EAPOLClientProfileSetConfiguration(profile, cfg);
	CFRelease(profile);
    }
    free(keys);

 done:
    cfg->ssids = ssids_dict;
    cfg->profiles = profiles_dict;
    return;
}

STATIC CFDictionaryRef
export_profiles(EAPOLClientConfigurationRef cfg)
{
    int				count;
    CFMutableDictionaryRef	dict;
    int				i;
    const void * *		values;

    count = CFDictionaryGetCount(cfg->profiles);
    dict = CFDictionaryCreateMutable(NULL, count,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (count == 0) {
	/* return empty dict */
	goto done;
    }

    values = (const void * *)malloc(sizeof(*values) * count);
    CFDictionaryGetKeysAndValues(cfg->profiles, NULL, values);
    for (i = 0; i < count; i++) {
	EAPOLClientProfileRef	profile = (EAPOLClientProfileRef)values[i];
	CFDictionaryRef		profile_dict;
	CFStringRef		profileID;

	profile_dict 
	    = EAPOLClientProfileCreateDictAndProfileID(profile, &profileID);
	if (profile_dict == NULL) {
	    /* error, return NULL */
	    my_CFRelease(&dict);
	    break;
	}
	CFDictionarySetValue(dict, profileID, profile_dict);
	CFRelease(profile_dict);
	CFRelease(profileID);
    }
    free(values);

 done:
    return (dict);
}

STATIC CFArrayRef
myCFArrayCreateWithIntegerList(const int * list, int list_count)
{
    CFMutableArrayRef		array;
    int				i;

    array = CFArrayCreateMutable(NULL, list_count, &kCFTypeArrayCallBacks);
    for (i = 0; i < list_count; i++) {
	CFNumberRef	num;

	num = CFNumberCreate(NULL, kCFNumberIntType, list + i);
	CFArrayAppendValue(array, num);
	CFRelease(num);
    }
    return (array);
}

STATIC CFDictionaryRef
copy_def_auth_props(SCPreferencesRef eap_prefs)
{
    CFArrayRef			accept_types = NULL;
    CFMutableDictionaryRef	auth_props;
    int				list[] = { 
	kEAPTypePEAP,
	kEAPTypeTTLS,
	kEAPTypeCiscoLEAP,
	kEAPTypeEAPFAST,
	kEAPTypeTLS
    };
    int				list_count = sizeof(list) / sizeof(list[0]);
    CFDictionaryRef		pref_auth_props;

    pref_auth_props
	= SCPreferencesGetValue(eap_prefs,
				kConfigurationKeyDefaultAuthenticationProperties);
    if (pref_auth_props != NULL) {
	accept_types = CFDictionaryGetValue(pref_auth_props,
					    kEAPClientPropAcceptEAPTypes);
	if (accept_types_valid(accept_types)) {
	    CFRetain(pref_auth_props);
	    return (pref_auth_props);
	}
	if (accept_types != NULL) {
	    SCLog(TRUE, LOG_NOTICE, 
		  CFSTR("EAPOLClientConfiguration: default Authentication "
			"Properties invalid, %@ - ignoring"), pref_auth_props);
	}
    }
    accept_types = myCFArrayCreateWithIntegerList(list, list_count);
    auth_props = CFDictionaryCreateMutable(NULL, 0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(auth_props,
			 kEAPClientPropAcceptEAPTypes,
			 accept_types);
    CFDictionarySetValue(auth_props, kEAPClientPropEAPFASTUsePAC,
			 kCFBooleanTrue);
    CFDictionarySetValue(auth_props, kEAPClientPropEAPFASTProvisionPAC,
			 kCFBooleanTrue);
    CFRelease(accept_types);
    return (auth_props);

}

/*
 * Function: copy_service
 * Purpose:
 *   Check the global list of SCNetworkServiceRefs for one that is configured
 *   over the specifed SCNetworkInterfaceRef.   The assumption here is that
 *   a previous call has already checked for such a service in the current
 *   set.
 */
STATIC SCNetworkServiceRef
copy_service(SCPreferencesRef prefs, SCNetworkInterfaceRef net_if)
{
    int			count = 0;
    int			i;
    SCNetworkServiceRef	service = NULL;
    CFArrayRef		list;

    list = SCNetworkServiceCopyAll(prefs);
    if (list != NULL) {
	count = CFArrayGetCount(list);
    }
    if (count == 0) {
	goto done;
    }
    for (i = 0; i < count; i++) {
	SCNetworkInterfaceRef	this_if;
	SCNetworkServiceRef	this_service;

	this_service = (SCNetworkServiceRef)CFArrayGetValueAtIndex(list, i);
	this_if = SCNetworkServiceGetInterface(this_service);
	if (this_if == NULL) {
	    continue;
	}
	if (CFEqual(this_if, net_if)) {
	    service = this_service;
	    CFRetain(service);
	    break;
	}
    }
 done:
    my_CFRelease(&list);
    return (service);
}

/*
 * Function: copy_interface
 * Purpose:
 *   Get a reference to an SCNetworkInterfaceRef for the specified
 *   interface name.   First try to get an interface configured in the
 *   current set.  If that fails, copy a service configured over the
 *   specified interface, and add it to the current set.
 *   
 *   Return the interface, service, and set for the caller to release.
 */
STATIC SCNetworkInterfaceRef
copy_interface(SCPreferencesRef prefs, CFStringRef if_name,
	       SCNetworkSetRef * ret_set_p,
	       SCNetworkServiceRef * ret_service_p)
{
    SCNetworkSetRef		current_set = NULL;
    SCNetworkServiceRef		service = NULL;
    SCNetworkInterfaceRef	net_if;
    SCNetworkInterfaceRef	ret = NULL;

    /* if the interface is part of a service/set, we're done */
    net_if = copy_configured_interface(prefs, if_name);
    if (net_if != NULL) {
	ret = net_if;
	CFRetain(ret);
	goto done;
    }

    /* interface isn't part of a service/set, make it so */
    net_if = copy_present_interface(if_name);
    if (net_if == NULL) {
	goto done;
    }

    /* find the service in any set */
    service = copy_service(prefs, net_if);
    if (service == NULL) {
	syslog(LOG_ERR, 
	       "EAPOLClientConfiguration: can't get service");
	goto done;
    }
    /* add the service to the current set */
    current_set = SCNetworkSetCopyCurrent(prefs);
    if (current_set == NULL) {
	syslog(LOG_ERR,
	       "EAPOLClientConfiguration: can't get current set");
	goto done;
    }
    if (SCNetworkSetAddService(current_set, service) == FALSE) {
	syslog(LOG_ERR,
	       "EAPOLClientConfiguration: failed to add dummy service");
	goto done;
    }
    /* return this SCNetworkInterfaceRef since it's bound to the prefs */
    ret = SCNetworkServiceGetInterface(service);
    CFRetain(ret);

 done:
    my_CFRelease(&net_if);
    if (ret == NULL) {
	my_CFRelease(&service);
	my_CFRelease(&current_set);
    }
    *ret_service_p = service;
    *ret_set_p = current_set;
    return (ret);
}

/*
 * Function: set_eapol_configuration
 * Purpose:
 *   Set the EAPOL configuration in the prefs.
 */
STATIC Boolean
set_eapol_configuration(SCPreferencesRef prefs, CFStringRef if_name,
			CFDictionaryRef dict)
{
    SCNetworkSetRef		current_set = NULL;
    SCNetworkInterfaceRef	net_if_save;
    Boolean			ret = FALSE;
    SCNetworkServiceRef		service = NULL;

    net_if_save = copy_interface(prefs, if_name, &current_set, &service);
    if (net_if_save != NULL) {
	ret = SCNetworkInterfaceSetExtendedConfiguration(net_if_save, kEAPOL,
							 dict);
	if (ret == FALSE) {
	    syslog(LOG_ERR,
		   "EAPOLClientConfiguration: SetExtendedConfiguration failed");
	}
    }
    if (current_set != NULL) {
	SCNetworkSetRemoveService(current_set, service);
	CFRelease(current_set);
    }
    my_CFRelease(&service);
    my_CFRelease(&net_if_save);
    return (ret);
}


/*
 * Function: get_eapol_configuration
 * Purpose:
 *   Get the EAPOL configuration for the interface.
 * Note:
 *   Side-effect on prefs is that a service gets added to the current set.
 *   Since this prefs reference will not be saved, we leave the service in
 *   the current set so that we can find it as a configured interface later
 *   when it's time to save the writable prefs.
 */
STATIC CFDictionaryRef
get_eapol_configuration(SCPreferencesRef prefs, CFStringRef if_name,
			SCNetworkInterfaceRef * ret_net_if)
{    
    SCNetworkSetRef		current_set = NULL;
    CFDictionaryRef		dict = NULL;
    SCNetworkServiceRef		service = NULL;
    SCNetworkInterfaceRef	net_if = NULL;

    net_if = copy_interface(prefs, if_name, &current_set, &service);
    if (net_if != NULL) {
	dict = SCNetworkInterfaceGetExtendedConfiguration(net_if, kEAPOL);
    }
    my_CFRelease(&current_set);
    my_CFRelease(&service);
    if (ret_net_if != NULL) {
	*ret_net_if = net_if;
    }
    else {
	my_CFRelease(&net_if);
    }
    return (dict);
}

/*
 * Function: setInterfaceEAPOLConfiguration
 * Purpose:
 *   Set the EAPOL configuration for the particular interface in the
 *   cfg->sc_prefs and add the SCNetworkInterfaceRef to cfg->sc_changed_if.
 *   That allows saveInterfaceEAPOLConfiguration() to know which interfaces
 *   were changed when it commits the changes to the writable prefs.
 */
STATIC Boolean
setInterfaceEAPOLConfiguration(EAPOLClientConfigurationRef cfg, 
			       SCNetworkInterfaceRef net_if,
			       CFDictionaryRef dict)
{
    CFRange	r;
    Boolean	ret;

    ret = SCNetworkInterfaceSetExtendedConfiguration(net_if, kEAPOL, dict);
    if (ret == FALSE) {
	return (ret);
    }
    /* keep track of which SCNetworkInterfaceRef's were changed */
    if (cfg->sc_changed_if == NULL) {
	cfg->sc_changed_if = CFArrayCreateMutable(NULL, 0, 
						  &kCFTypeArrayCallBacks);
    }
    r.location = 0;
    r.length = CFArrayGetCount(cfg->sc_changed_if);
    if (CFArrayContainsValue(cfg->sc_changed_if, r, net_if) == FALSE) {
	CFArrayAppendValue(cfg->sc_changed_if, net_if);
    }
    return (TRUE);
}

/*
 * Function: saveInterfaceEAPOLConfiguration
 * Purpose:
 *   Save the SCNetworkInterface EAPOL information for System and LoginWindow
 *   modes.
 *
 *   Iterate over the changed SCNetworkInterfaceRef list cfg->sc_changed_if,
 *   and for each interface, grab the EAPOL extended information from
 *   cfg->sc_prefs.    Then set the corresponding value in the new
 *   freshly created SCPreferencesRef.
 *
 *   All this done to avoid a writer getting Stale Object errors
 *   when it has its own SCPreferencesRef object that it manipulates while
 *   having an EAPOLClientConfigurationRef open.
 */
STATIC Boolean
saveInterfaceEAPOLConfiguration(EAPOLClientConfigurationRef cfg,
				Boolean * changed_p)
{
    AuthorizationExternalForm *	auth_ext_p;
    int				count;
    int				i;
    SCPreferencesRef		prefs = NULL;
    Boolean			ret = FALSE;

    *changed_p = FALSE;
    if (cfg->sc_changed_if == NULL) {
	return (TRUE);
    }
    auth_ext_p = EAPOLClientConfigurationGetAuthorizationExternalForm(cfg);
    if (auth_ext_p != NULL) {
	AuthorizationRef	auth;
	OSStatus		status;

	status = AuthorizationCreateFromExternalForm(auth_ext_p, &auth);
	if (status != errAuthorizationSuccess) {
	    syslog(LOG_ERR,
		   "EAPOLClientConfiguration: can't allocate Authorization, %d",
		   (int)status);
	    goto done;
	}
	prefs = SCPreferencesCreateWithAuthorization(NULL,
						     kPrefsName, NULL,
						     auth);
	AuthorizationFree(auth, kAuthorizationFlagDefaults);
    }
    else {
	prefs = SCPreferencesCreate(NULL, kPrefsName, NULL);
    }
    count = CFArrayGetCount(cfg->sc_changed_if);
    for (i = 0; i < count; i++) {
	CFDictionaryRef		dict;
	CFStringRef		if_name;
	SCNetworkInterfaceRef	net_if;

	net_if = (SCNetworkInterfaceRef)
	    CFArrayGetValueAtIndex(cfg->sc_changed_if, i);
	if_name = SCNetworkInterfaceGetBSDName(net_if);
	if (if_name == NULL) {
	    /* should not happen */
	    syslog(LOG_ERR, "EAPOLClientConfiguration: missing BSD name");
	    continue;
	}
	dict = SCNetworkInterfaceGetExtendedConfiguration(net_if, kEAPOL);

	/* find the same interface in the saving prefs */
	if (set_eapol_configuration(prefs, if_name, dict) == FALSE) {
	    continue;
	}
    }

    ret = SCPreferencesCommitChanges(prefs);
    if (ret == FALSE) {
	syslog(LOG_NOTICE,
	       "EAPOLClientConfigurationSave SCPreferencesCommitChanges"
	       " failed %s", SCErrorString(SCError()));
	goto done;
    }
    SCPreferencesApplyChanges(prefs);
    *changed_p = TRUE;

 done:
    my_CFRelease(&cfg->sc_changed_if);
    my_CFRelease(&prefs);
    return (ret);
}

/**
 ** CF object glue code
 **/
STATIC CFStringRef	__EAPOLClientConfigurationCopyDebugDesc(CFTypeRef cf);
STATIC void		__EAPOLClientConfigurationDeallocate(CFTypeRef cf);

STATIC CFTypeID __kEAPOLClientConfigurationTypeID = _kCFRuntimeNotATypeID;

STATIC const CFRuntimeClass __EAPOLClientConfigurationClass = {
    0,						/* version */
    "EAPOLClientConfiguration",			/* className */
    NULL,					/* init */
    NULL,					/* copy */
    __EAPOLClientConfigurationDeallocate,	/* deallocate */
    NULL,					/* equal */
    NULL,					/* hash */
    NULL,					/* copyFormattingDesc */
    __EAPOLClientConfigurationCopyDebugDesc	/* copyDebugDesc */
};

STATIC CFStringRef
__EAPOLClientConfigurationCopyDebugDesc(CFTypeRef cf)
{
    CFAllocatorRef		allocator = CFGetAllocator(cf);
    EAPOLClientConfigurationRef	cfg = (EAPOLClientConfigurationRef)cf;
    CFMutableStringRef		result;

    result = CFStringCreateMutable(allocator, 0);
    if (cfg->auth_ext_p == NULL) {
	CFStringAppendFormat(result, NULL,
			     CFSTR("<EAPOLClientConfiguration %p [%p]> {"),
			     cf, allocator);
    }
    else {
	CFStringAppendFormat(result, NULL,
			     CFSTR("<EAPOLClientConfiguration %p [%p] auth> {"),
			     cf, allocator);
    }
    CFStringAppendFormat(result, NULL, CFSTR("profiles = %@"), cfg->profiles);
    CFStringAppendFormat(result, NULL, CFSTR("ssids = %@"), cfg->ssids);
    CFStringAppend(result, CFSTR("}"));
    return (result);
}

STATIC void
__EAPOLClientConfigurationDeallocate(CFTypeRef cf)
{
    EAPOLClientConfigurationRef cfg = (EAPOLClientConfigurationRef)cf;

    if (cfg->auth_ext_p != NULL) {
	free(cfg->auth_ext_p);
	cfg->auth_ext_p = NULL;
    }
    my_CFRelease(&cfg->eap_prefs);
    my_CFRelease(&cfg->sc_prefs);
    my_CFRelease(&cfg->profiles);
    my_CFRelease(&cfg->ssids);
    my_CFRelease(&cfg->def_auth_props);
    my_CFRelease(&cfg->sc_changed_if);
    return;
}

STATIC void
__EAPOLClientConfigurationInitialize(void)
{
    /* initialize runtime */
    __kEAPOLClientConfigurationTypeID 
	= _CFRuntimeRegisterClass(&__EAPOLClientConfigurationClass);
    return;
}

STATIC void
__EAPOLClientConfigurationRegisterClass(void)
{
    STATIC pthread_once_t	initialized = PTHREAD_ONCE_INIT;

    pthread_once(&initialized, __EAPOLClientConfigurationInitialize);
    return;
}

STATIC EAPOLClientConfigurationRef
__EAPOLClientConfigurationAllocate(CFAllocatorRef allocator)
{
    EAPOLClientConfigurationRef	cfg;
    int				size;

    __EAPOLClientConfigurationRegisterClass();

    size = sizeof(*cfg) - sizeof(CFRuntimeBase);
    cfg = (EAPOLClientConfigurationRef)
	_CFRuntimeCreateInstance(allocator,
				 __kEAPOLClientConfigurationTypeID, size, NULL);
    bzero(((void *)cfg) + sizeof(CFRuntimeBase), size);
    return (cfg);
}

/**
 ** EAPOLClientConfiguration APIs
 **/

CFTypeID
EAPOLClientConfigurationGetTypeID(void)
{
    __EAPOLClientConfigurationRegisterClass();
    return (__kEAPOLClientConfigurationTypeID);
}

EAPOLClientConfigurationRef
EAPOLClientConfigurationCreateInternal(CFAllocatorRef allocator,
				       AuthorizationRef auth)
{
    EAPOLClientConfigurationRef		cfg;

    /* allocate/return an EAPOLClientConfigurationRef */
    cfg = __EAPOLClientConfigurationAllocate(allocator);
    if (cfg == NULL) {
	return (NULL);
    }
    if (auth != NULL) {
	cfg->eap_prefs 
	    = SCPreferencesCreateWithAuthorization(allocator,
						   kPrefsName,
						   kEAPOLClientConfigurationPrefsID,
						   auth);
    }
    else {
	cfg->eap_prefs = SCPreferencesCreate(allocator, kPrefsName,
					     kEAPOLClientConfigurationPrefsID);
    }
    if (cfg->eap_prefs == NULL) {
	goto failed;
    }
    if (auth != NULL) {
	AuthorizationExternalForm *	auth_ext_p;
	OSStatus			status;

	auth_ext_p = malloc(sizeof(*auth_ext_p));
	status = AuthorizationMakeExternalForm(auth, auth_ext_p);
	if (status != errAuthorizationSuccess) {
	    free(auth_ext_p);
	    goto failed;
	}
	cfg->auth_ext_p = auth_ext_p;
    }

    import_profiles(cfg);
    cfg->def_auth_props = copy_def_auth_props(cfg->eap_prefs);
    return (cfg);

 failed:
    my_CFRelease(&cfg);
    return (NULL);

}

/*
 * Function: EAPOLClientConfigurationCreate
 *
 * Purpose:
 *   Get reference to EAPOL Client Configuration object.
 *   Used by pure readers of the configuration, and by root processes
 *   that modify the configuration.
 *
 * Returns:
 *   NULL if reference could not be allocated, non-NULL otherwise.
 *
 * Note:
 *   Attempts to invoke EAPOLClientConfigurationSave() as non-root using an
 *   EAPOLClientConfigurationRef acquired by calling this function will fail.
 *   Use EAPOLClientConfigurationCreateWithAuthorization() if you want to
 *   make changes as non-root.
 */
EAPOLClientConfigurationRef
EAPOLClientConfigurationCreate(CFAllocatorRef allocator)
{
    return (EAPOLClientConfigurationCreateInternal(allocator, NULL));
}

/*
 * Function: EAPOLClientConfigurationCreateWithAuthorization
 *
 * Purpose:
 *   Get reference to EAPOL Client Configuration object with the specified
 *   AuthorizationRef.
 *
 *   Used by non-root processes that need to modify the configuration.
 *
 * Returns:
 *   NULL if reference could not be allocated, non-NULL otherwise.
 */
EAPOLClientConfigurationRef
EAPOLClientConfigurationCreateWithAuthorization(CFAllocatorRef allocator,
						AuthorizationRef auth)
{
    if (auth == NULL) {
	return (NULL);
    }
    return (EAPOLClientConfigurationCreateInternal(allocator, auth));
}

/*
 * Function: EAPOLClientConfigurationSave
 * 
 * Purpose:
 *   Write the configuration to persistent storage.
 *
 * Returns:
 *   TRUE if successfully written, FALSE otherwise.
 */	
Boolean
EAPOLClientConfigurationSave(EAPOLClientConfigurationRef cfg)
{
    Boolean		changed = FALSE;
    CFDictionaryRef	existing_prefs_dict;
    CFDictionaryRef	prefs_dict;
    Boolean		ret = FALSE;

    /* save the 802.1X prefs */
    prefs_dict = export_profiles(cfg);
    if (prefs_dict == NULL) {
	syslog(LOG_NOTICE,
	       "EAPOLClientConfigurationSave copy_profiles() failed");
	goto done;
    }
    existing_prefs_dict = SCPreferencesGetValue(cfg->eap_prefs,
						kConfigurationKeyProfiles);
    if (cfg->def_auth_props_changed == FALSE
	&& my_CFEqual(existing_prefs_dict, prefs_dict)) {
	/* configuration is the same, no need to save */
    }
    else {
	if (cfg->def_auth_props_changed) {
	    ret = SCPreferencesSetValue(cfg->eap_prefs,
					kConfigurationKeyDefaultAuthenticationProperties,
					cfg->def_auth_props);
	    if (ret == FALSE) {
		syslog(LOG_NOTICE,
		       "EAPOLClientConfigurationSave SCPreferencesSetValue"
		       " failed %s",
		       SCErrorString(SCError()));
		goto done;
	    }
	}
	ret = SCPreferencesSetValue(cfg->eap_prefs, kConfigurationKeyProfiles,
				    prefs_dict);
	if (ret == FALSE) {
	    syslog(LOG_NOTICE,
		   "EAPOLClientConfigurationSave SCPreferencesSetValue"
		   " failed %s",
		   SCErrorString(SCError()));
	    goto done;
	}
	ret = SCPreferencesCommitChanges(cfg->eap_prefs);
	if (ret == FALSE) {
	    syslog(LOG_NOTICE,
		   "EAPOLClientConfigurationSave SCPreferencesCommitChanges"
		   " failed %s", SCErrorString(SCError()));
	    return (FALSE);
	}
	cfg->def_auth_props_changed = FALSE;
	SCPreferencesApplyChanges(cfg->eap_prefs);
	changed = TRUE;
    }

    /* save the network prefs */
    {
	Boolean		this_changed = FALSE;

	ret = saveInterfaceEAPOLConfiguration(cfg, &this_changed);
	if (ret == FALSE) {
	    goto done;
	}
	if (this_changed) {
	    changed = TRUE;
	}
    }
    my_CFRelease(&cfg->sc_prefs); /* force a refresh */

 done:
    my_CFRelease(&prefs_dict);
    if (changed) {
	notify_post(kEAPOLClientConfigurationChangedNotifyKey);
    }
    return (ret);
}

/*
 * Function: EAPOLClientConfigurationCopyProfiles
 *
 * Purpose:
 *   Get the list of defined profiles.   If there are no profiles defined,
 *   returns NULL.
 *
 * Returns:
 *   NULL if no profiles are defined, non-NULL non-empty array of profiles
 *   otherwise.
 */
CFArrayRef /* of EAPOLClientProfileRef */
EAPOLClientConfigurationCopyProfiles(EAPOLClientConfigurationRef cfg)
{
    CFAllocatorRef		allocator = CFGetAllocator(cfg);
    int				count;
    CFArrayRef			profiles;
    const void * *		values;

    count = CFDictionaryGetCount(cfg->profiles);
    if (count == 0) {
	return (NULL);
    }
    values = (const void * *)malloc(sizeof(*values) * count);
    CFDictionaryGetKeysAndValues(cfg->profiles, NULL, values);
    profiles = CFArrayCreate(allocator, values, count, &kCFTypeArrayCallBacks);
    free(values);
    return (profiles);
}

/*
 * Function: EAPOLClientConfigurationGetProfileWithID
 *
 * Purpose:
 *   Return the profile associated with the specified profileID.
 *
 * Returns:
 *   NULL if no such profile exists, non-NULL profile otherwise.
 */
EAPOLClientProfileRef
EAPOLClientConfigurationGetProfileWithID(EAPOLClientConfigurationRef cfg,
					 CFStringRef profileID)
{
    return ((EAPOLClientProfileRef)
	    CFDictionaryGetValue(cfg->profiles, profileID));
}

/*
 * Function: EAPOLClientConfigurationGetProfileWithWLANSSID
 *
 * Purpose:
 *   Return the profile associated with the specified WLAN SSID.
 *
 * Returns:
 *   NULL if no such profile exists, non-NULL profile otherwise.
 */
EAPOLClientProfileRef
EAPOLClientConfigurationGetProfileWithWLANSSID(EAPOLClientConfigurationRef cfg,
					       CFDataRef ssid)
{
    CFStringRef		profileID;

    profileID = CFDictionaryGetValue(cfg->ssids, ssid);
    if (profileID == NULL) {
	return (NULL);
    }
    return ((EAPOLClientProfileRef)
	    CFDictionaryGetValue(cfg->profiles, profileID));
}

/*
 * Function: EAPOLClientConfigurationRemoveProfile
 *
 * Purpose:
 *   Remove the specified profile from the configuration.
 *
 * Returns:
 *   FALSE if the profile is invalid or not in the configuration,
 *   TRUE otherwise.
 */
Boolean
EAPOLClientConfigurationRemoveProfile(EAPOLClientConfigurationRef cfg,
				      EAPOLClientProfileRef profile)
{
    CFStringRef			profileID = EAPOLClientProfileGetID(profile);
    CFDataRef			ssid;
    
    if (EAPOLClientConfigurationGetProfileWithID(cfg, profileID) != profile) {
	/* trying to remove profile that isn't part of the configuration */
	return (FALSE);
    }
    ssid = EAPOLClientProfileGetWLANSSIDAndSecurityType(profile, NULL);
    if (ssid != NULL) {
	CFDictionaryRemoveValue(cfg->ssids, ssid);
    }
    CFDictionaryRemoveValue(cfg->profiles, profileID);
    return (TRUE);
}

/*
 * Function: EAPOLClientConfigurationAddProfile
 *
 * Purpose:
 *   Add the specified profile to the configuration.
 *
 * Returns:
 *   FALSE if the profile could not be added, either because:
 *   - the profile is already in the configuration, or
 *   - the profile conflicts with an existing profile (profileID or WLAN SSID)
 *   TRUE if the profile was added successfully.
 */
Boolean
EAPOLClientConfigurationAddProfile(EAPOLClientConfigurationRef cfg,
				   EAPOLClientProfileRef profile)
{
    CFStringRef			profileID = EAPOLClientProfileGetID(profile);
    CFDataRef			ssid;

    if (profile->cfg != NULL) {
	/* profile is already part of the configuration */
	return (FALSE);
    }
    if (EAPOLClientConfigurationGetProfileWithID(cfg, profileID) != NULL) {
	/* profile already present with that profileID */
	return (FALSE);
    }
    ssid = EAPOLClientProfileGetWLANSSIDAndSecurityType(profile, NULL);
    if (ssid != NULL
	&& EAPOLClientConfigurationGetProfileWithWLANSSID(cfg, ssid) != NULL) {
	/* profile already present with that SSID */
	return (FALSE);
    }
    CFDictionarySetValue(cfg->profiles, profileID, profile);
    if (ssid != NULL) {
	CFDictionarySetValue(cfg->ssids, ssid, profileID);
    }
    EAPOLClientProfileSetConfiguration(profile, cfg);
    return (TRUE);
    
}

/*
 * Function: EAPOLClientConfigurationCopyMatchingProfiles
 *
 * Purpose:
 *   Find the profile(s) matching the specified profile.
 *   A profile is matched based on the profileID and the SSID, both of which
 *   must be unique in the configuration.
 *
 *   Usually invoked after calling 
 *   EAPOLClientProfileCreateWithPropertyList() to instantiate a profile
 *   from an external format.
 *
 * Returns:
 *   NULL if no matching profile is part of the configuration,
 *   non-NULL CFArrayRef of EAPOLClientProfileRef if found.
 */
CFArrayRef /* of EAPOLClientProfileRef */
EAPOLClientConfigurationCopyMatchingProfiles(EAPOLClientConfigurationRef cfg,
					     EAPOLClientProfileRef profile)
{
    int				count;
    EAPOLClientProfileRef	existing_profile;
    CFStringRef			profileID = EAPOLClientProfileGetID(profile);
    CFDataRef			ssid;
    const void *		values[2] = { NULL, NULL };
    
    count = 0;
    existing_profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
    if (existing_profile != NULL) {
	values[count] = existing_profile;
	count++;
    }
    ssid = EAPOLClientProfileGetWLANSSIDAndSecurityType(profile, NULL);
    if (ssid != NULL) {
	existing_profile 
	    = EAPOLClientConfigurationGetProfileWithWLANSSID(cfg,
							     ssid);
	if (existing_profile != NULL && values[0] != existing_profile) {
	    values[count] = existing_profile;
	    count++;
	}
    }
    if (count == 0) {
	return (NULL);
    }
    return (CFArrayCreate(CFGetAllocator(cfg), values, count,
			  &kCFTypeArrayCallBacks));
}

/*
 * Function: EAPOLClientConfigurationCopyLoginWindowProfiles
 *
 * Purpose:
 *   Return the list of profiles configured for LoginWindow mode on the
 *   specified BSD network interface (e.g. "en0", "en1").
 *
 * Returns:
 *   NULL if no profiles are defined, non-NULL non-empty array of profiles
 *   otherwise.
 */
CFArrayRef /* of EAPOLClientProfileRef */
EAPOLClientConfigurationCopyLoginWindowProfiles(EAPOLClientConfigurationRef cfg,
						CFStringRef if_name)
{
    int				count;
    int				i;
    CFDictionaryRef		dict;
    CFArrayRef			profile_ids;
    CFMutableArrayRef		ret_profiles = NULL;

    dict = get_eapol_configuration(get_sc_prefs(cfg), if_name, NULL);
    if (dict == NULL) {
	goto done;
    }
    profile_ids = CFDictionaryGetValue(dict, kLoginWindowProfileIDs);
    if (isA_CFArray(profile_ids) == NULL) {
	goto done;
    }
    count = CFArrayGetCount(profile_ids);
    if (count == 0) {
	goto done;
    }
    ret_profiles = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    for (i = 0; i < count; i++) {
	CFStringRef		profileID;
	EAPOLClientProfileRef	profile;

	profileID = (CFStringRef)CFArrayGetValueAtIndex(profile_ids, i);
	if (isA_CFString(profileID) == NULL) {
	    continue;
	}
	profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
	if (profile != NULL) {
	    CFArrayAppendValue(ret_profiles, profile);
	}
    }
    if (CFArrayGetCount(ret_profiles) == 0) {
	my_CFRelease(&ret_profiles);
    }

 done:
    return (ret_profiles);
}

/*
 * Function: EAPOLClientConfigurationSetLoginWindowProfiles
 *
 * Purpose:
 *   Set the list of profiles configured for LoginWindow mode on the
 *   specified BSD network interface (e.g. "en0", "en1").
 *
 *   If you pass NULL for the "profiles" argument, the LoginWindow profile
 *   list is cleared.
 */
Boolean
EAPOLClientConfigurationSetLoginWindowProfiles(EAPOLClientConfigurationRef cfg,
					       CFStringRef if_name,
					       CFArrayRef profiles)
{
    CFDictionaryRef		dict;
    CFArrayRef			existing_profile_ids = NULL;
    SCNetworkInterfaceRef	net_if = NULL;
    CFMutableDictionaryRef	new_dict = NULL;
    CFMutableArrayRef		profile_ids = NULL;
    Boolean			ret = FALSE;

    dict = get_eapol_configuration(get_sc_prefs(cfg), if_name, &net_if);
    if (net_if == NULL) {
	goto done;
    }
    if (dict != NULL) {
	existing_profile_ids
	    = CFDictionaryGetValue(dict, kLoginWindowProfileIDs);
	existing_profile_ids = isA_CFArray(existing_profile_ids);
    }
    if (profiles == NULL || CFArrayGetCount(profiles) == 0) {
	profile_ids = NULL;
    }
    else {
	int				count;
	int				i;
	CFRange				r = { 0, 0 };

	count = CFArrayGetCount(profiles);
	profile_ids = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
	for (i = 0; i < count; i++) {
	    EAPOLClientProfileRef	profile;
	    CFStringRef			profileID;

	    profile = (EAPOLClientProfileRef)
		CFArrayGetValueAtIndex(profiles, i);
	    profileID = EAPOLClientProfileGetID(profile);
	    if (CFArrayContainsValue(profile_ids, r, profileID) == FALSE) {
		CFArrayAppendValue(profile_ids, profileID);
		r.length++;
	    }
	}
    }
    if (my_CFEqual(existing_profile_ids, profile_ids)) {
	ret = TRUE;
	goto done;
    }
    if (dict != NULL) {
	/*
	 * remove the AcceptEAPTypes array to give EAPOLController a way to
	 * know whether we're using new configuration or the old
	 */
	new_dict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	CFDictionaryRemoveValue(new_dict, kEAPClientPropAcceptEAPTypes);
	CFDictionaryRemoveValue(new_dict, kSCResvInactive);
    }
    else {
	new_dict = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
    }
    if (profile_ids == NULL) {
	CFDictionaryRemoveValue(new_dict, kLoginWindowProfileIDs);
	if (CFDictionaryGetCount(new_dict) == 0) {
	    my_CFRelease(&new_dict);
	}
    }
    else {
	CFDictionarySetValue(new_dict, kLoginWindowProfileIDs, profile_ids);
    }
    if (setInterfaceEAPOLConfiguration(cfg, net_if, new_dict)
	== FALSE) {
	goto done;
    }
    ret = TRUE;

 done:
    my_CFRelease(&new_dict);
    my_CFRelease(&profile_ids);
    my_CFRelease(&net_if);
    return (ret);
}

/*
 * Function: EAPOLClientConfigurationGetSystemProfile
 *
 * Purpose:
 *   Return the profile configured for System mode on the
 *   specified BSD network interface (e.g. "en0", "en1").
 *
 * Returns:
 *   NULL if no such profile is defined, non-NULL profile
 *   otherwise.
 */
EAPOLClientProfileRef
EAPOLClientConfigurationGetSystemProfile(EAPOLClientConfigurationRef cfg,
					 CFStringRef if_name)
{
    CFDictionaryRef		dict;
    SCNetworkInterfaceRef 	net_if = NULL;
    EAPOLClientProfileRef	profile = NULL;
    CFStringRef			profileID;

    dict = get_eapol_configuration(get_sc_prefs(cfg), if_name, &net_if);
    if (dict != NULL
	&& !CFEqual(SCNetworkInterfaceGetInterfaceType(net_if),
		    kSCNetworkInterfaceTypeIEEE80211)) {
	profileID = CFDictionaryGetValue(dict, kSystemProfileID);
	if (isA_CFString(profileID) != NULL) {
	    profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
	}
    }
    my_CFRelease(&net_if);
    return (profile);
}

/*
 * Function: EAPOLClientConfigurationSetSystemProfile
 *
 * Purpose:
 *   Set the profile configured for System mode on the specified
 *   BSD network interface (e.g. "en0", "en1").
 *
 *   If you pass NULL for the "profile" argument, the System profile
 *   list is cleared.
 */
Boolean
EAPOLClientConfigurationSetSystemProfile(EAPOLClientConfigurationRef cfg,
					 CFStringRef if_name,
					 EAPOLClientProfileRef profile)
{
    CFDictionaryRef		dict;
    CFStringRef			existing_profileID = NULL;
    CFMutableDictionaryRef	new_dict = NULL;
    SCNetworkInterfaceRef	net_if = NULL;
    CFStringRef			profileID = NULL;
    Boolean			ret = FALSE;

    if (profile != NULL) {
	profileID = EAPOLClientProfileGetID(profile);
    }
    dict = get_eapol_configuration(get_sc_prefs(cfg), if_name, &net_if);
    if (net_if == NULL) {
	goto done;
    }
    if (CFEqual(SCNetworkInterfaceGetInterfaceType(net_if),
		kSCNetworkInterfaceTypeIEEE80211)) {
	/* disallow setting static System Mode on AirPort interfaces */
	goto done;
    }

    if (dict != NULL) {
	existing_profileID 
	    = CFDictionaryGetValue(dict, kSystemProfileID);
    }
    if (my_CFEqual(existing_profileID, profileID)) {
	/* nothing to do */
	ret = TRUE;
	goto done;
    }
    if (dict != NULL) {
	/*
	 * remove the AcceptEAPTypes array to give EAPOLController a way to
	 * know whether we're using new configuration or the old
	 */
	new_dict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	CFDictionaryRemoveValue(new_dict, kEAPClientPropAcceptEAPTypes);
	CFDictionaryRemoveValue(new_dict, kSCResvInactive);
    }
    else {
	new_dict = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
    }

    if (profileID == NULL) {
	CFDictionaryRemoveValue(new_dict, kSystemProfileID);
	if (CFDictionaryGetCount(new_dict) == 0) {
	    my_CFRelease(&new_dict);
	}
    }
    else {
	CFDictionarySetValue(new_dict, kSystemProfileID, profileID);
    }
    if (setInterfaceEAPOLConfiguration(cfg, net_if, new_dict)
	== FALSE) {
	goto done;
    }
    ret = TRUE;

 done:
    my_CFRelease(&new_dict);
    my_CFRelease(&net_if);
    return (ret);
}

/*
 * Function: EAPOLClientConfigurationCopyAllSystemProfiles
 *
 * Purpose:
 *   Determine which interfaces have System mode configured.  
 *   Return the results in a dictionary of EAPOLClientProfile
 *   keyed by the interface name.
 *
 * Returns:
 *    NULL if no interfaces are configured for System mode,
 *    non-NULL CFDictionary of (CFString, EAPOLClientProfile) otherwise.
 */
CFDictionaryRef /* of (CFString, EAPOLClientProfile) */
EAPOLClientConfigurationCopyAllSystemProfiles(EAPOLClientConfigurationRef cfg)
{
    return (copy_profiles_for_mode(cfg, kEAPOLControlModeSystem));
}

/*
 * Function: EAPOLClientConfigurationCopyAllLoginWindowProfiles
 *
 * Purpose:
 *   Determine which interfaces have LoginWindow mode configured.  
 *   Return the results in a dictionary of arrays keyed by the interface name.
 *   Each array contains EAPOLClientProfileRefs.
 *   
 * Returns:
 *    NULL if no interfaces are configured for LoginWindow mode,
 *    non-NULL CFDictionary of (CFString, CFArray[EAPOLClientProfile])
 *    otherwise.
 */
CFDictionaryRef /* of (CFString, CFArray[EAPOLClientProfile]) */
EAPOLClientConfigurationCopyAllLoginWindowProfiles(EAPOLClientConfigurationRef cfg)
{
    return (copy_profiles_for_mode(cfg, kEAPOLControlModeLoginWindow));
}


/**
 ** Private SPI
 **/

/*
 * Function: EAPOLClientConfigurationGetDefaultAuthenticationProperties
 *
 * Purpose:
 *   Get the default authentication properties.   Used by the authentication
 *   client when there is no profile specified.
 *
 * Returns:
 *   CFDictionaryRef containing the default authentication properties.
 */
CFDictionaryRef
EAPOLClientConfigurationGetDefaultAuthenticationProperties(EAPOLClientConfigurationRef cfg)
{
    return (cfg->def_auth_props);
}
			     
/*
 * Function: EAPOLClientConfigurationSetDefaultAuthenticationProperties
 *
 * Purpose:
 *   Set the default authentication properties.
 *
 *   If 'auth_props' is NULL, resets the value to the default.
 *
 * Returns:
 *   FALSE if the auth_props dictionary is invalid, TRUE otherwise.
 *
 * Note:
 *   You must call EAPOLClientConfigurationSave() to make the change permanent.
 */
Boolean
EAPOLClientConfigurationSetDefaultAuthenticationProperties(EAPOLClientConfigurationRef cfg,
							   CFDictionaryRef auth_props)
{
    CFArrayRef		accept_types;

    my_CFRelease(&cfg->def_auth_props);
    if (auth_props == NULL) {
	cfg->def_auth_props = copy_def_auth_props(cfg->eap_prefs);
	cfg->def_auth_props_changed = TRUE;
	return (TRUE);
    }
    accept_types = CFDictionaryGetValue(auth_props,
					kEAPClientPropAcceptEAPTypes);
    if (accept_types_valid(accept_types) == FALSE) {
	return (FALSE);
    }
    cfg->def_auth_props = CFRetain(auth_props);
    cfg->def_auth_props_changed = TRUE;
    return (TRUE);
}
			     

/**
 ** Internal SPI
 **/
PRIVATE_EXTERN void
EAPOLClientConfigurationSetProfileForSSID(EAPOLClientConfigurationRef cfg,
					  CFDataRef ssid,
					  EAPOLClientProfileRef profile)
{
    if (profile == NULL) {
	CFDictionaryRemoveValue(cfg->ssids, ssid);
    }
    else {
	CFDictionarySetValue(cfg->ssids, ssid,
			     EAPOLClientProfileGetID(profile));
    }
    return;
}

PRIVATE_EXTERN AuthorizationExternalForm *
EAPOLClientConfigurationGetAuthorizationExternalForm(EAPOLClientConfigurationRef cfg)
{
    return (cfg->auth_ext_p);
}
