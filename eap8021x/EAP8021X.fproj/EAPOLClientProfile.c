/*
 * Copyright (c) 2009-2013 Apple Inc. All rights reserved.
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
 * EAPOLClientProfile.c
 * - implementation of the EAPOLClientProfile CF object
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
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <pthread.h>
#include "EAPClientProperties.h"
#include "EAPOLClientConfigurationInternal.h"
#include "symbol_scope.h"
#include "myCFUtil.h"

/**
 ** Schema keys
 **/
#define kProfileKeyProfileID 		CFSTR("ProfileID")
#define kProfileKeyUserDefinedName 	CFSTR("UserDefinedName")
#define kProfileKeyAuthenticationProperties CFSTR("AuthenticationProperties")
#define kProfileKeyInformation	 	CFSTR("Information")
#define kProfileKeyWLAN 		CFSTR("WLAN")
#define kProfileKeyWLANSSID 		CFSTR("SSID")
#define kProfileKeyWLANSecurityType 	CFSTR("SecurityType")
#define kProfileKeyWLANDomain 		CFSTR("Domain")

/**
 ** Utility Functions
 **/

PRIVATE_EXTERN Boolean
accept_types_valid(CFArrayRef accept)
{
    int			count;
    int			i;

    if (isA_CFArray(accept) == NULL) {
	return (FALSE);
    }
    count = CFArrayGetCount(accept);
    if (count == 0) {
	return (FALSE);
    }
    for (i = 0; i < count; i++) {
	CFNumberRef		type = CFArrayGetValueAtIndex(accept, i);

	if (isA_CFNumber(type) == NULL) {
	    return (FALSE);
	}
    }
    return (TRUE);
}

STATIC Boolean
applicationID_is_valid(CFStringRef applicationID)
{
    CFRange	r;

    if (CFStringGetLength(applicationID) < 3) {
	return (FALSE);
    }
    r = CFStringFind(applicationID, CFSTR("."), 0);
    if (r.location == kCFNotFound) {
	return (FALSE);
    }
    return (TRUE);
}

/**
 ** CF object glue code
 **/
STATIC CFStringRef	__EAPOLClientProfileCopyDebugDesc(CFTypeRef cf);
STATIC void		__EAPOLClientProfileDeallocate(CFTypeRef cf);
STATIC Boolean		__EAPOLClientProfileEqual(CFTypeRef cf1, CFTypeRef cf2);
STATIC CFHashCode	__EAPOLClientProfileHash(CFTypeRef cf);

STATIC CFTypeID __kEAPOLClientProfileTypeID = _kCFRuntimeNotATypeID;

STATIC const CFRuntimeClass __EAPOLClientProfileClass = {
    0,					/* version */
    "EAPOLClientProfile",		/* className */
    NULL,				/* init */
    NULL,				/* copy */
    __EAPOLClientProfileDeallocate,	/* deallocate */
    __EAPOLClientProfileEqual,		/* equal */
    __EAPOLClientProfileHash,		/* hash */
    NULL,				/* copyFormattingDesc */
    __EAPOLClientProfileCopyDebugDesc	/* copyDebugDesc */
};

STATIC CFStringRef
__EAPOLClientProfileCopyDebugDesc(CFTypeRef cf)
{
    CFAllocatorRef		allocator = CFGetAllocator(cf);
    EAPOLClientProfileRef	profile = (EAPOLClientProfileRef)cf;
    CFMutableStringRef		result;

    result = CFStringCreateMutable(allocator, 0);
    CFStringAppendFormat(result, NULL, 
			 CFSTR("<EAPOLClientProfile %p [%p]> {"),
			 cf, allocator);
    
    CFStringAppendFormat(result, NULL,
			 CFSTR("ProfileID = %@"),
			 profile->uuid);
    if (profile->user_defined_name != NULL) {
	CFStringAppendFormat(result, NULL,
			     CFSTR(" Name = '%@'"),
			     profile->user_defined_name);
    }
    if (profile->WLAN.ssid != NULL) {
	CFStringRef ssid_str = my_CFStringCreateWithData(profile->WLAN.ssid);

	CFStringAppendFormat(result, NULL,
			     CFSTR(", WLAN SSID %@ [%@]"),
			     ssid_str, profile->WLAN.security_type);
	CFRelease(ssid_str);
    }
    if (profile->auth_props != NULL) {
	CFStringAppendFormat(result, NULL,
			     CFSTR(", auth_props = %@"),
			     profile->auth_props);
    }
    if (profile->information != NULL
	&& CFDictionaryGetCount(profile->information) != 0) {
	CFStringAppendFormat(result, NULL,
			     CFSTR(", info = %@"),
			     profile->information);
    }
    CFStringAppendFormat(result, NULL, CFSTR("}"));
    return result;
}


STATIC void
__EAPOLClientProfileDeallocate(CFTypeRef cf)
{
    EAPOLClientProfileRef profile = (EAPOLClientProfileRef)cf;

    my_CFRelease(&profile->uuid);
    my_CFRelease(&profile->auth_props);
    my_CFRelease(&profile->user_defined_name);
    my_CFRelease(&profile->WLAN.ssid);
    my_CFRelease(&profile->WLAN.security_type);
    my_CFRelease(&profile->information);
    return;
}


STATIC Boolean
__EAPOLClientProfileEqual(CFTypeRef cf1, CFTypeRef cf2)
{
    EAPOLClientProfileRef 	prof1 = (EAPOLClientProfileRef)cf1;
    EAPOLClientProfileRef	prof2 = (EAPOLClientProfileRef)cf2;

    if (CFEqual(prof1->uuid, prof2->uuid) == FALSE) {
	return (FALSE);
    }
    if (my_CFEqual(prof1->auth_props, prof2->auth_props) == FALSE) {
	return (FALSE);
    }
    if (my_CFEqual(prof1->user_defined_name,
		   prof2->user_defined_name) == FALSE) {
	return (FALSE);
    }
    if (my_CFEqual(prof1->WLAN.ssid, prof2->WLAN.ssid) == FALSE) {
	return (FALSE);
    }
    if (my_CFEqual(prof1->WLAN.security_type, prof2->WLAN.security_type)
	== FALSE) {
	return (FALSE);
    }
    if (my_CFEqual(prof1->information, prof2->information) == FALSE) {
	return (FALSE);
    }
    return (TRUE);
}

STATIC CFHashCode
__EAPOLClientProfileHash(CFTypeRef cf)
{
    EAPOLClientProfileRef 	profile = (EAPOLClientProfileRef)cf;

    return (CFHash(profile->uuid));
}


STATIC void
__EAPOLClientProfileInitialize(void)
{
    /* initialize runtime */
    __kEAPOLClientProfileTypeID 
	= _CFRuntimeRegisterClass(&__EAPOLClientProfileClass);
    return;
}

STATIC void
__EAPOLClientProfileRegisterClass(void)
{
    STATIC pthread_once_t	initialized = PTHREAD_ONCE_INIT;

    pthread_once(&initialized, __EAPOLClientProfileInitialize);
    return;
}

STATIC EAPOLClientProfileRef
__EAPOLClientProfileAllocate(CFAllocatorRef allocator)
{
    EAPOLClientProfileRef	profile;
    int				size;

    __EAPOLClientProfileRegisterClass();

    size = sizeof(*profile) - sizeof(CFRuntimeBase);
    profile = (EAPOLClientProfileRef)
	_CFRuntimeCreateInstance(allocator,
				 __kEAPOLClientProfileTypeID, size, NULL);
    bzero(((void *)profile) + sizeof(CFRuntimeBase), size);
    return (profile);
}

/**
 ** EAPOLClientProfile APIs
 **/

CFTypeID
EAPOLClientProfileGetTypeID(void)
{
    __EAPOLClientProfileRegisterClass();
    return (__kEAPOLClientProfileTypeID);
}

/*
 * Function: EAPOLClientProfileCreate
 *
 * Purpose:
 *   Instantiate a new profile to be filled in by calling the various
 *   "setter" functions:
 *	EAPOLClientProfileSetUserDefinedName
 *	EAPOLClientProfileSetAuthenticationProperties
 *	EAPOLClientProfileSetWLANSSIDAndSecurityType
 *	EAPOLClientProfileSetWLANDomain
 *	EAPOLClientProfileSetInformation
 *	
 */
EAPOLClientProfileRef
EAPOLClientProfileCreate(EAPOLClientConfigurationRef cfg)
{
    CFAllocatorRef			alloc = CFGetAllocator(cfg);
    EAPOLClientProfileRef		profile;

    profile = __EAPOLClientProfileAllocate(alloc);
    if (profile == NULL) {
	return (NULL);
    }
    profile->uuid = my_CFUUIDStringCreate(alloc);
    if (EAPOLClientConfigurationAddProfile(cfg, profile) == FALSE) {
	/* this should not happen */
	my_CFRelease(&profile);
    }
    return (profile);
}

/*
 * Function: EAPOLClientProfileGetUserDefinedName
 *
 * Purpose:
 *   Retrieve the user defined string associated with the profile.
 *
 * Returns:
 *   NULL if no user defined name is set, non-NULL otherwise.
 */	   
CFStringRef
EAPOLClientProfileGetUserDefinedName(EAPOLClientProfileRef profile)
{
    return (profile->user_defined_name);
}


/*
 * Function: EAPOLClientProfileGetUserDefinedName
 *
 * Purpose:
 *   Set the user defined string associated with the profile.
 *
 * Notes:
 *   If user_defined_name is NULL, the user visible name is removed,
 *   otherwise it is set to the value passed.
 */	   
void
EAPOLClientProfileSetUserDefinedName(EAPOLClientProfileRef profile,
				     CFStringRef user_defined_name)
{
    if (user_defined_name != NULL) {
	CFRetain(user_defined_name);
    }
    if (profile->user_defined_name != NULL) {
	CFRelease(profile->user_defined_name);
    }
    profile->user_defined_name = user_defined_name;
    return;
}

/*
 * Function: EAPOLClientProfileGetID
 *
 * Purpose:
 *   Get the unique identifier for the profile.
 */	   
CFStringRef
EAPOLClientProfileGetID(EAPOLClientProfileRef profile)
{
    return (profile->uuid);
}

/* 
 * Function: EAPOLClientProfileGetAuthenticationProperties
 *
 * Purpose:
 *   Returns the EAP client authentication properties for the profile.
 *   The individual keys in the dictionary are defined in
 *   <EAP8021X/EAPClientProperties.h>.
 */
CFDictionaryRef
EAPOLClientProfileGetAuthenticationProperties(EAPOLClientProfileRef profile)
{
    return (profile->auth_props);
}

/*
 * Function: EAPOLClientProfileSetAuthenticationProperties
 * Purpose:
 *   Set the EAP client authentication properties for the profile.
 *   The individual keys in the dictionary are defined in
 *   <EAP8021X/EAPClientProperties.h>.
 */
void
EAPOLClientProfileSetAuthenticationProperties(EAPOLClientProfileRef profile,
					      CFDictionaryRef auth_props)
{
    if (auth_props != NULL) {
	CFRetain(auth_props);
    }
    if (profile->auth_props != NULL) {
	CFRelease(profile->auth_props);
    }
    profile->auth_props = auth_props;
    return;
}

/*
 * Function: EAPOLClientProfileGetWLANSSIDAndSecurityType
 *
 * Purpose:
 *   Get the SSID and security type associated with the profile.
 *
 * Returns:
 *   non-NULL SSID and security type if the profile is bound to a WLAN SSID,
 *   NULL otherwise.	
 */
CFDataRef
EAPOLClientProfileGetWLANSSIDAndSecurityType(EAPOLClientProfileRef profile, 
					     CFStringRef * ret_security_type)
{
    if (profile->WLAN.ssid != NULL) {
	if (ret_security_type != NULL) {
	    *ret_security_type = profile->WLAN.security_type;
	}
	return (profile->WLAN.ssid);
    }
    if (ret_security_type != NULL) {
	*ret_security_type = NULL;
    }
    return (NULL);
}

/*
 * Function: EAPOLClientProfileSetWLANSSIDAndSecurityType
 *
 * Purpose:
 *   Bind the profile to a particular SSID, and specify the expected
 *   security type i.e. WEP, WPA, WPA2 or any.  Only a single profile can be 
 *   associated with a particular SSID.
 *
 *   To un-bind the profile from its SSID, set the ssid argument to NULL.
 *   In that case, the security_type argument will be ignored.
 *
 * Returns:
 *    FALSE if there's an existing profile with the same SSID, or the
 *    security_type is not one of the defined strings above, 
 *    TRUE otherwise.
 */
Boolean
EAPOLClientProfileSetWLANSSIDAndSecurityType(EAPOLClientProfileRef profile,
					     CFDataRef ssid,
					     CFStringRef security_type)
{
    EAPOLClientProfileRef	existing_profile;

    if (ssid != NULL) {
	if (profile->WLAN.domain != NULL) {
	    /* can't specify an SSID when domain is already specified */
	    return (FALSE);
	}
	if (security_type == NULL) {
	    /* both SSID and security_type must be specified */
	    return (FALSE);
	}
	if (profile->cfg != NULL) {
	    existing_profile
		= EAPOLClientConfigurationGetProfileWithWLANSSID(profile->cfg,
								 ssid);
	    if (existing_profile != NULL && existing_profile != profile) {
		/* some other profile has this SSID already */
		return (FALSE);
	    }
	}
    }
    else if (security_type != NULL) {
	/* SSID is NULL, so security_type must also be NULL */
	return (FALSE);
    }

    /* give up the existing SSID */
    if (profile->WLAN.ssid != NULL && profile->cfg != NULL) {
	/* clear the old binding */
	EAPOLClientConfigurationSetProfileForSSID(profile->cfg,
						  profile->WLAN.ssid,
						  NULL);
    }

    /* claim the new SSID */
    if (ssid != NULL) {
	CFRetain(ssid);
	if (profile->cfg != NULL) {
	    EAPOLClientConfigurationSetProfileForSSID(profile->cfg,
						      ssid,
						      profile);
	}
    }
    if (security_type != NULL) {
	CFRetain(security_type);
    }
    if (profile->WLAN.ssid != NULL) {
	CFRelease(profile->WLAN.ssid);
    }
    if (profile->WLAN.security_type != NULL) {
	CFRelease(profile->WLAN.security_type);
    }
    profile->WLAN.ssid = ssid;
    profile->WLAN.security_type = security_type;
    return (TRUE);
}

/*
 * Function: EAPOLClientProfileGetWLANDomain
 *
 * Purpose:
 *   Get the WLAN Hotspot 2.0 domain name associated with the profile.
 *
 * Returns:
 *   non-NULL domain name if the profile is bound to a Hotspot 2.0 WLAN 
 *   domain name, NULL otherwise.	
 */
CFStringRef
EAPOLClientProfileGetWLANDomain(EAPOLClientProfileRef profile)
{
    return (profile->WLAN.domain);
}

/*
 * Function: EAPOLClientProfileSetWLANDomain
 *
 * Purpose:
 *   Bind the profile to a Hotspot 2.0 domain name.
 *
 *   Only a single profile can be associated with a particular domain name.
 *
 *   To un-bind the profile from the domain name, set the domain
 *   argument to NULL.
 *
 * Returns:
 *    FALSE if there's an existing profile with the same domain name,
 *    TRUE otherwise.
 *
 * Note:
 *    EAPOLClientProfileSetWLANSSIDAndSecurityType() and
 *    EAPOLClientProfileSetWLANDomain() are mutally exclusive.
 *    A given profile can only be associated with a WLAN SSID *or* a
 *    WLAN domain and not both.
 */
Boolean
EAPOLClientProfileSetWLANDomain(EAPOLClientProfileRef profile,
				CFStringRef domain)
{
    EAPOLClientProfileRef	existing_profile;

    if (domain != NULL) {
	if (profile->WLAN.ssid != NULL) {
	    /* can't specify a domain when ssid is already specified */
	    return (FALSE);
	}
	if (profile->cfg != NULL) {
	    existing_profile
		= EAPOLClientConfigurationGetProfileWithWLANDomain(profile->cfg,
								   domain);
	    if (existing_profile != NULL && existing_profile != profile) {
		/* some other profile has this domain already */
		return (FALSE);
	    }
	}
    }

    /* give up the existing domain */
    if (profile->WLAN.domain != NULL && profile->cfg != NULL) {
	/* clear the old binding */
	EAPOLClientConfigurationSetProfileForWLANDomain(profile->cfg,
							profile->WLAN.domain,
							NULL);
    }

    /* claim the domain */
    if (domain != NULL) {
	CFRetain(domain);
	if (profile->cfg != NULL) {
	    EAPOLClientConfigurationSetProfileForWLANDomain(profile->cfg,
							    domain,
							    profile);
	}
    }
    if (profile->WLAN.domain != NULL) {
	CFRelease(profile->WLAN.domain);
    }
    profile->WLAN.domain = domain;
    return (TRUE);
}

/*
 * Function: EAPOLClientProfileSetInformation
 *
 * Purpose:
 *   Associate additional information with the profile using the given
 *   application identifier.
 *   
 *   If info is NULL, the information for the particular application is cleared.
 *
 * Note:
 *   applicationID must be an application identifier e.g. "com.mycompany.myapp".
 */
Boolean
EAPOLClientProfileSetInformation(EAPOLClientProfileRef profile,
				 CFStringRef applicationID,
				 CFDictionaryRef information)
{
    if (applicationID_is_valid(applicationID) == FALSE) {
	return (FALSE);
    }
    if (information == NULL) {
	if (profile->information != NULL) {
	    CFDictionaryRemoveValue(profile->information, applicationID);
	}
    }
    else {
	if (isA_CFDictionary(information) == NULL) {
	    return (FALSE);
	}
	if (profile->information == NULL) {
	    profile->information
		= CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	}
	CFDictionarySetValue(profile->information, applicationID, information);
    }
    return (TRUE);
}

/*
 * Function: EAPOLClientProfileGetInformation
 *
 * Purpose:
 *   Retrieve the additional information associated with the profile using 
 *   the given application identifier.
 *
 * Returns:
 *   NULL if no such application identifier exists,
 *   non-NULL dictionary otherwise.
 *
 * Note:
 *   applicationID must be an application identifier e.g. "com.mycompany.myapp".
 */
CFDictionaryRef
EAPOLClientProfileGetInformation(EAPOLClientProfileRef profile,
				 CFStringRef applicationID)
{
    CFDictionaryRef	dict;

    if (profile->information == NULL) {
	return (NULL);
    }
    if (applicationID_is_valid(applicationID) == FALSE) {
	return (NULL);
    }
    dict = CFDictionaryGetValue(profile->information, applicationID);
    return (isA_CFDictionary(dict));
}

/*
 * Function: EAPOLClientProfileCreatePropertyList
 *
 * Purpose:
 *   Create an "external" format for a profile.
 *   
 * Returns:
 *   NULL if the profile could not be externalized, non-NULL otherwise.
 */
CFPropertyListRef
EAPOLClientProfileCreatePropertyList(EAPOLClientProfileRef profile)
{
    CFMutableDictionaryRef	dict;
    CFStringRef			profileID;

    dict = EAPOLClientProfileCreateDictAndProfileID(profile, &profileID);
    if (dict == NULL) {
	return (NULL);
    }
    CFDictionarySetValue(dict, kProfileKeyProfileID, profileID);
    return ((CFPropertyListRef)dict);
}

/*
 * Function: EAPOLClientProfileCreateWithPropertyList
 *
 * Purpose:
 *   Create a profile using the supplied "external_format".  The profile
 *   is not tied to the configuration until the function
 *   EAPOLClientConfigurationAddProfile() is invoked successfully.
 *   Use EAPOLClientConfigurationGetMatchingProfiles() to check for conflicts
 *   if calling that function fails.
 *
 * Returns:
 *   NULL if the "external_format" property list was not a valid format,
 *   non-NULL EAPOLClientProfileRef otherwise.
 */
EAPOLClientProfileRef
EAPOLClientProfileCreateWithPropertyList(CFPropertyListRef external_format)
{
    CFDictionaryRef		dict = (CFDictionaryRef)external_format;
    CFStringRef			profileID;

    if (isA_CFDictionary(dict) == NULL) {
	return (NULL);
    }
    profileID = CFDictionaryGetValue(dict, kProfileKeyProfileID);
    if (isA_CFString(profileID) == NULL) {
	return (NULL);
    }
    return (EAPOLClientProfileCreateWithDictAndProfileID(dict, profileID));
}

/**
 ** Internal API
 **/

PRIVATE_EXTERN CFMutableDictionaryRef
EAPOLClientProfileCreateDictAndProfileID(EAPOLClientProfileRef profile,
					 CFStringRef * ret_profileID)
{
    CFArrayRef			accept_types = NULL;
    CFMutableDictionaryRef	dict;

    if (profile->auth_props != NULL) {
	accept_types = CFDictionaryGetValue(profile->auth_props,
					    kEAPClientPropAcceptEAPTypes);
    }
    if (accept_types_valid(accept_types) == FALSE) {
	SCLog(TRUE, LOG_NOTICE, 
	      CFSTR("EAPOLClientConfiguration: profile %@"
		    " missing/invalid AuthenticationProperties"),
	      EAPOLClientProfileGetID(profile));
	return (NULL);
    }
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (ret_profileID != NULL) {
	*ret_profileID = CFRetain(profile->uuid);
    }
    CFDictionarySetValue(dict, 
			 kProfileKeyAuthenticationProperties,
			 profile->auth_props);
    if (profile->user_defined_name != NULL) {
	CFDictionarySetValue(dict, 
			     kProfileKeyUserDefinedName,
			     profile->user_defined_name);
    }
    if (profile->information != NULL
	&& CFDictionaryGetCount(profile->information) != 0) {
	CFDictionarySetValue(dict,
			     kProfileKeyInformation,
			     profile->information);
    }
    if (profile->WLAN.ssid != NULL || profile->WLAN.domain != NULL) {
	int			count;
	const void *		keys[2];
	CFDictionaryRef		WLAN;
	const void *		values[2];

	if (profile->WLAN.ssid != NULL) {
	    keys[0] = kProfileKeyWLANSSID;
	    values[0] = profile->WLAN.ssid;
	    keys[1] = kProfileKeyWLANSecurityType;
	    values[1] = profile->WLAN.security_type;
	    count = 2;
	}
	else {
	    keys[0] = kProfileKeyWLANDomain;
	    values[0] = profile->WLAN.domain;
	    count = 1;
	}
	WLAN = CFDictionaryCreate(NULL, keys, values, count, 
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(dict, kProfileKeyWLAN, WLAN);
	CFRelease(WLAN);
    }
    return (dict);
}

PRIVATE_EXTERN EAPOLClientProfileRef
EAPOLClientProfileCreateWithDictAndProfileID(CFDictionaryRef dict,
					     CFStringRef profileID)
{
    CFAllocatorRef		alloc = CFGetAllocator(dict);
    CFArrayRef			accept_types;
    CFDictionaryRef		information;
    CFStringRef			domain = NULL;
    CFDictionaryRef		eap_config;
    EAPOLClientProfileRef	profile;
    CFStringRef			security_type = NULL;
    CFStringRef			user_defined_name;
    CFDataRef			ssid = NULL;
    CFDictionaryRef		wlan;

    eap_config = CFDictionaryGetValue(dict,
				      kProfileKeyAuthenticationProperties);
    if (isA_CFDictionary(eap_config) == NULL
	|| CFDictionaryGetCount(eap_config) == 0) {
	SCLog(TRUE, LOG_NOTICE, 
	      CFSTR("EAPOLClientConfiguration: profile %@"
		    " missing/invalid %@ property"),
	      profileID, kProfileKeyAuthenticationProperties);
	return (NULL);
    }
    accept_types = CFDictionaryGetValue(eap_config,
					kEAPClientPropAcceptEAPTypes);
    if (accept_types_valid(accept_types) == FALSE) {
	SCLog(TRUE, LOG_NOTICE, 
	      CFSTR("EAPOLClientConfiguration: profile %@"
		    " missing/invalid %@ property in %@"),
	      profileID, kEAPClientPropAcceptEAPTypes,
	      kProfileKeyAuthenticationProperties);
	return (NULL);
    }
    wlan = CFDictionaryGetValue(dict, kProfileKeyWLAN);
    if (wlan != NULL) {
	if (isA_CFDictionary(wlan) == NULL) {
	    SCLog(TRUE, LOG_NOTICE, 
		  CFSTR("EAPOLClientConfiguration: profile %@"
			" invalid %@ property"),
		  profileID, kProfileKeyWLAN);
	    return (NULL);
	}
	ssid = CFDictionaryGetValue(wlan, kProfileKeyWLANSSID);
	domain = CFDictionaryGetValue(wlan, kProfileKeyWLANDomain);
	if (isA_CFData(ssid) == NULL && isA_CFString(domain) == NULL) {
	    SCLog(TRUE, LOG_NOTICE, 
		  CFSTR("EAPOLClientConfiguration: profile %@"
			" invalid/missing property (%@ or %@) in %@"),
		  profileID, kProfileKeyWLANSSID, kProfileKeyWLANDomain,
		  kProfileKeyWLAN);
	    return (NULL);
	}
	if (ssid != NULL) {
	    security_type = CFDictionaryGetValue(wlan,
						 kProfileKeyWLANSecurityType);
	    if (isA_CFString(security_type) == NULL) {
		SCLog(TRUE, LOG_NOTICE, 
		      CFSTR("EAPOLClientConfiguration: profile %@"
			    " invalid/missing %@ property in %@"),
		      profileID, kProfileKeyWLANSecurityType, kProfileKeyWLAN);
		return (NULL);
	    }
	}
    }
    user_defined_name = CFDictionaryGetValue(dict, kProfileKeyUserDefinedName);
    if (user_defined_name != NULL && isA_CFString(user_defined_name) == NULL) {
	SCLog(TRUE, LOG_NOTICE, 
	      CFSTR("EAPOLClientConfiguration: profile %@"
		    " invalid %@ property"),
	      profileID, kProfileKeyUserDefinedName);
	return (NULL);
    }
    information = CFDictionaryGetValue(dict, kProfileKeyInformation);
    if (information != NULL && isA_CFDictionary(information) == NULL) {
	SCLog(TRUE, LOG_NOTICE, 
	      CFSTR("EAPOLClientConfiguration: profile %@"
		    " invalid %@ property"),
	      profileID, kProfileKeyInformation);
	return (NULL);
    }

    /* allocate/set an EAPOLClientProfileRef */
    profile = __EAPOLClientProfileAllocate(alloc);
    if (profile == NULL) {
	return (NULL);
    }
    profile->uuid = CFRetain(profileID);
    EAPOLClientProfileSetUserDefinedName(profile, user_defined_name);
    EAPOLClientProfileSetAuthenticationProperties(profile, eap_config);
    if (ssid != NULL) {
	EAPOLClientProfileSetWLANSSIDAndSecurityType(profile, ssid,
						     security_type);
    }
    else if (domain != NULL) {
	EAPOLClientProfileSetWLANDomain(profile, domain);
    }
    if (information != NULL) {
	profile->information
	    = CFDictionaryCreateMutableCopy(NULL, 0, information);
    }
    return (profile);
}

PRIVATE_EXTERN void
EAPOLClientProfileSetConfiguration(EAPOLClientProfileRef profile,
				   EAPOLClientConfigurationRef cfg)
{
    /* we can't retain the configuration, since it retains us */
    profile->cfg = cfg;
    return;
}

PRIVATE_EXTERN EAPOLClientConfigurationRef
EAPOLClientProfileGetConfiguration(EAPOLClientProfileRef profile)
{
    return (profile->cfg);
}
