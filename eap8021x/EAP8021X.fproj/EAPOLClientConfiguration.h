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

#ifndef _EAP8021X_EAPOLCLIENTCONFIGURATION_H
#define _EAP8021X_EAPOLCLIENTCONFIGURATION_H

/*
 * EAPOLClientConfiguration.h
 * - EAPOL client configuration API
 */

#include <stdint.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFPropertyList.h>
#include <Security/SecIdentity.h>
#include <Security/Authorization.h>
#include <TargetConditionals.h>
#include <sys/cdefs.h>

typedef struct __EAPOLClientConfiguration * EAPOLClientConfigurationRef;
typedef struct __EAPOLClientProfile * EAPOLClientProfileRef;
typedef struct __EAPOLClientItemID * EAPOLClientItemIDRef;

enum {
    kEAPOLClientDomainUser = 1,
    kEAPOLClientDomainSystem = 3
};
typedef uint32_t EAPOLClientDomain;

extern const CFStringRef	kEAPOLClientProfileWLANSecurityTypeWEP;
extern const CFStringRef	kEAPOLClientProfileWLANSecurityTypeWPA;
extern const CFStringRef	kEAPOLClientProfileWLANSecurityTypeWPA2;
extern const CFStringRef	kEAPOLClientProfileWLANSecurityTypeAny;

__BEGIN_DECLS

/* CFType introspection */
CFTypeID
EAPOLClientConfigurationGetTypeID(void);

CFTypeID
EAPOLClientProfileGetTypeID(void);

CFTypeID
EAPOLClientItemIDGetTypeID(void);

/**
 ** EAPOLClientItemID APIs
 **/

/*
 * Function: EAPOLClientItemIDCreateWithProfileID
 *
 * Purpose:
 *   Create an EAPOLClientItemID instance based on the supplied profileID
 */
EAPOLClientItemIDRef
EAPOLClientItemIDCreateWithProfileID(CFStringRef profileID);

/*
 * Function: EAPOLClientItemIDCreateWithWLANSSID
 *
 * Purpose:
 *   Create an EAPOLClientItemID instance based on the supplied WLAN SSID.
 */
EAPOLClientItemIDRef
EAPOLClientItemIDCreateWithWLANSSID(CFDataRef ssid);

/*
 * Function: EAPOLClientItemIDCreateWithProfile
 *
 * Purpose:
 *   Create an EAPOLClientItemID instance based on the supplied
 *   EAPOLClientProfileRef.
 */
EAPOLClientItemIDRef
EAPOLClientItemIDCreateWithProfile(EAPOLClientProfileRef profile);

/*
 * Function: EAPOLClientItemIDCreateDefault
 *
 * Purpose:
 *   Create an EAPOLClientItemID instance to indicate that the default
 *   authentication parameters and keychain items are to be used.
 */
EAPOLClientItemIDRef
EAPOLClientItemIDCreateDefault(void);

/*
 * Function: EAPOLClientItemIDCopyPasswordItem
 *
 * Purpose:
 *   Retrieve the password item from secure storage for the particular itemID
 *   in the specified domain.
 *
 * Returns:
 *   FALSE if no such item exists, and both *name_p and *password_p
 *   are set to NULL.
 *
 *   TRUE if an item exists, and either of both *name_p and *password_p
 *   are set to a non-NULL value.
 *
 * Note:
 *   If the specified "domain" is kEAPOLClientDomainSystem, you must be
 *   root to call this function successfully. If the EAPOLClientItemIDRef has
 *   access to an AuthorizationRef (see Note below for
 *   EAPOLClientItemIDSetPasswordItem()), this routine will return the name
 *   but the value returned in the password_p will be fabricated.  Used in this
 *   way, this function can only be used to test for password existence, not to
 *   actually get the password value.
 */
Boolean
EAPOLClientItemIDCopyPasswordItem(EAPOLClientItemIDRef itemID, 
				  EAPOLClientDomain domain,
				  CFDataRef * name_p,
				  CFDataRef * password_p);
/*
 * Function: EAPOLClientItemIDSetPasswordItem
 *
 * Purpose:
 *   Set the password item in secure storage for the specified itemID
 *   in the specified domain.
 *
 *   Passing NULL for 'name' or 'password' means that the corresponding
 *   item attribute is left alone.  If both 'name" and 'password' are
 *   NULL, the call has no effect, but TRUE is still returned.
 *
 *   Passing non-NULL, non-empty 'name' or 'password' sets the
 *   corresponding item attribute to the specified value.   If the item
 *   does not exist, it will be created.
 *
 *   Specifying a non-NULL but zero-length CFDataRef for 'name' or 'password'
 *   will eliminate the corresponding attribute.
 *
 * Returns:
 *   FALSE if the operation did not succeed, TRUE otherwise.
 *
 * Note:
 *   If the specified "domain" is kEAPOLClientDomainSystem, you must be
 *   root to call this function successfully, or have instantiated the
 *   EAPOLClientItemIDRef from an EAPOLClientProfileRef that came from
 *   an EAPOLClientConfigurationRef created by calling
 *   EAPOLClientConfigurationCreateWithAuthorization().
 *
 *   For example:
 *
 *     (EAPOLClientConfigurationRef)
 *       EAPOLClientConfigurationCreateWithAuthorization()
 *             |
 *             v
 *     (EAPOLClientProfileRef) EAPOLClientProfileCreate()
 *             |
 *             v
 *     (EAPOLClientItemIDRef) EAPOLClientItemIDCreateWithProfile()
 *
 */
Boolean
EAPOLClientItemIDSetPasswordItem(EAPOLClientItemIDRef itemID,
				 EAPOLClientDomain domain,
				 CFDataRef name, CFDataRef password);

/*
 * Function: EAPOLClientItemIDRemovePasswordItem
 *
 * Purpose:
 *   Remove the password item in secure storage for the specified itemID
 *   in the specified domain.
 *
 * Returns:
 *   FALSE if the operation did not succeed, TRUE otherwise.
 */
Boolean
EAPOLClientItemIDRemovePasswordItem(EAPOLClientItemIDRef itemID,
				    EAPOLClientDomain domain);

/*
 * Function: EAPOLClientItemIDCopyIdentity
 *
 * Purpose:
 *   Retrieve the identity associated with the particular itemID
 *   in the specified domain.
 *
 * Returns:
 *   non-NULL SecIdentityRef when match can be found for the item ID, 
 *   NULL otherwise.
 */
SecIdentityRef
EAPOLClientItemIDCopyIdentity(EAPOLClientItemIDRef itemID, 
			      EAPOLClientDomain domain);

/*
 * Function: EAPOLClientItemIDSetIdentity
 *
 * Purpose:
 *   Associate an identity with the specified itemID in the specified
 *   domain.
 *
 *   If the identity is NULL, the identity preference is removed.
 *
 * Returns:
 *   FALSE if the operation did not succeed, TRUE otherwise.
 *
 * Note:
 *   If the specified "domain" is kEAPOLClientDomainSystem, you must be
 *   root to call this function successfully, or have instantiated the
 *   EAPOLClientItemIDRef from an EAPOLClientProfileRef that came from
 *   an EAPOLClientConfigurationRef created by calling
 *   EAPOLClientConfigurationCreateWithAuthorization().
 *
 *   For example:
 *
 *     (EAPOLClientConfigurationRef)
 *       EAPOLClientConfigurationCreateWithAuthorization()
 *             |
 *             v
 *     (EAPOLClientProfileRef) EAPOLClientProfileCreate()
 *             |
 *             v
 *     (EAPOLClientItemIDRef) EAPOLClientItemIDCreateWithProfile()
 *
 */
Boolean
EAPOLClientItemIDSetIdentity(EAPOLClientItemIDRef itemID,
			     EAPOLClientDomain domain,
			     SecIdentityRef identity);

/**
 ** EAPOLClientConfiguration APIs
 **/

/*
 * Const: kEAPOLClientConfigurationChangedNotifyKey
 *
 * Purpose:
 *   String used with notify(3) API to indicate that the
 *   EAPOLClientConfiguration has changed.
 *   
 *   notify_post(kEAPOLClientConfigurationChangedNotifyKey) is called
 *   in EAPOLClientConfigurationSave() when configuration changes have been
 *   made and require saving.
 */
extern const char * 	kEAPOLClientConfigurationChangedNotifyKey;

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
EAPOLClientConfigurationCreate(CFAllocatorRef allocator);

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
						AuthorizationRef auth);

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
EAPOLClientConfigurationSave(EAPOLClientConfigurationRef cfg);

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
EAPOLClientConfigurationCopyProfiles(EAPOLClientConfigurationRef cfg);

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
					 CFStringRef profileID);

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
					       CFDataRef ssid);

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
				      EAPOLClientProfileRef profile);

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
				   EAPOLClientProfileRef profile);

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
					     EAPOLClientProfileRef profile);

#if ! TARGET_OS_EMBEDDED
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
						CFStringRef if_name);


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
					       CFArrayRef profiles);
/*
 * Function: EAPOLClientConfigurationGetSystemProfile
 *
 * Purpose:
 *   Return the profile configured for System mode on the
 *   specified BSD network interface (e.g. "en0", "en1").
 *
 * Note:
 *   This API only works on non-802.11 interfaces.
 *
 * Returns:
 *   NULL if no such profile is defined, non-NULL profile otherwise.
 */
EAPOLClientProfileRef
EAPOLClientConfigurationGetSystemProfile(EAPOLClientConfigurationRef cfg,
					 CFStringRef if_name);

/*
 * Function: EAPOLClientConfigurationSetSystemProfile
 *
 * Purpose:
 *   Set the profile configured for System mode on the specified
 *   BSD network interface (e.g. "en0", "en1").
 *
 *   Pass NULL for the "profile" argument to clear the association of the
 *   profile with System mode.
 *
 * Note:
 * 1. This API only works on non-802.11 interfaces.
 * 2. If the profile argument is non-NULL, and the configuration is saved
 *    by calling EAPOLClientConfigurationSave(), an authentication session
 *    is started on the specified interface.
 *
 * Returns:
 *    TRUE if the configuration was set, FALSE otherwise.
 */
Boolean
EAPOLClientConfigurationSetSystemProfile(EAPOLClientConfigurationRef cfg,
					 CFStringRef if_name,
					 EAPOLClientProfileRef profile);

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
EAPOLClientConfigurationCopyAllSystemProfiles(EAPOLClientConfigurationRef cfg);

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
EAPOLClientConfigurationCopyAllLoginWindowProfiles(EAPOLClientConfigurationRef
						   cfg);


#endif /* ! TARGET_OS_EMBEDDED */

/*
 * Function: EAPOLClientConfigurationGetDefaultAuthenticationProperties
 *
 * Purpose:
 *   Get the default authentication properties.   Used by the authentication
 *   client when there is no profile specified, and to instantiate new default
 *   profiles.
 *
 * Returns:
 *   CFDictionaryRef containing the default authentication properties.
 */
CFDictionaryRef
EAPOLClientConfigurationGetDefaultAuthenticationProperties(EAPOLClientConfigurationRef cfg);
			     
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
							   CFDictionaryRef auth_props);
			     
/**
 ** EAPOLClientProfile APIs
 **/

/*
 * Function: EAPOLClientProfileCreate
 *
 * Purpose:
 *   Instantiate a new profile to be filled in by calling the various
 *   "setter" functions:
 *	EAPOLClientProfileSetUserDefinedName
 *	EAPOLClientProfileSetAuthenticationProperties
 *	EAPOLClientProfileSetWLANSSIDAndSecurityType
 *	EAPOLClientProfileSetInformation
 *	
 */
EAPOLClientProfileRef
EAPOLClientProfileCreate(EAPOLClientConfigurationRef cfg);

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
EAPOLClientProfileGetUserDefinedName(EAPOLClientProfileRef profile);

/*
 * Function: EAPOLClientProfileSetUserDefinedName
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
				     CFStringRef user_defined_name);

/*
 * Function: EAPOLClientProfileGetID
 *
 * Purpose:
 *   Get the unique identifier for the profile.
 */	   
CFStringRef
EAPOLClientProfileGetID(EAPOLClientProfileRef profile);

/* 
 * Function: EAPOLClientProfileGetAuthenticationProperties
 *
 * Purpose:
 *   Returns the EAP client authentication properties for the profile.
 *   The individual keys in the dictionary are defined in
 *   <EAP8021X/EAPClientProperties.h>.
 */
CFDictionaryRef
EAPOLClientProfileGetAuthenticationProperties(EAPOLClientProfileRef profile);

/*
 * Function: EAPOLClientProfileSetAuthenticationProperties
 * Purpose:
 *   Set the EAP client authentication properties for the profile.
 *   The individual keys in the dictionary are defined in
 *   <EAP8021X/EAPClientProperties.h>.
 */
void
EAPOLClientProfileSetAuthenticationProperties(EAPOLClientProfileRef profile,
					      CFDictionaryRef auth_props);

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
					     CFStringRef * ret_security_type);

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
					     CFStringRef security_type);

/*
 * Function: EAPOLClientProfileSetInformation
 *
 * Purpose:
 *   Associate additional information with the profile using the given
 *   application identifier.
 *   
 *   If info is NULL, the information for the particular application is cleared.
 *
 * Returns:
 *   FALSE if the applicationID or info are not valid, TRUE otherwise.
 *
 * Note:
 *   applicationID must be an application identifier e.g. "com.mycompany.myapp".
 */
Boolean
EAPOLClientProfileSetInformation(EAPOLClientProfileRef profile,
				 CFStringRef applicationID,
				 CFDictionaryRef info);

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
				 CFStringRef applicationID);

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
EAPOLClientProfileCreatePropertyList(EAPOLClientProfileRef profile);

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
EAPOLClientProfileCreateWithPropertyList(CFPropertyListRef external_format);

__END_DECLS

#endif /* _EAP8021X_EAPOLCLIENTCONFIGURATION_H */
