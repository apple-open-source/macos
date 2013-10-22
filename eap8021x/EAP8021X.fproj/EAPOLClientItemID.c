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
 * EAPOLClientItemID.c
 * - implementation of the EAPOLClientItemID CF object
 */

/* 
 * Modification History
 *
 * December 2, 2009	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <string.h>
#include <syslog.h>
#include <servers/bootstrap.h>
#include <bootstrap_priv.h>
#include <TargetConditionals.h>
#include <CoreFoundation/CFString.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecTrustedApplication.h>
#include <Security/SecTrustedApplicationPriv.h>
#include <SystemConfiguration/SCValidation.h>
#include <pthread.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include "EAPCertificateUtil.h"
#include "EAPOLClientConfigurationInternal.h"
#include "EAPOLClientConfigurationPrivate.h"
#include "EAPSecurity.h"
#include "EAPKeychainUtil.h"
#include "EAPKeychainUtilInternal.h"
#include "eapolcfg_auth.h"
#include "symbol_scope.h"
#include "myCFUtil.h"
#include "EAPLog.h"

/**
 ** Utility functions
 **/
STATIC CFTypeRef
my_CFDictionaryCopyValue(CFDictionaryRef dict, CFStringRef key)
{
    CFTypeRef	value;

    value = CFDictionaryGetValue(dict, key);
    if (value != NULL) {
	CFRetain(value);
    }
    return (value);
}

#define kEAPOLControllerPath		"/System/Library/SystemConfiguration/EAPOLController.bundle"
#define keapolclientPath		"eapolclient"
#define kAirPortApplicationGroup	"AirPort"
#define kSystemUIServerPath 		"/System/Library/CoreServices/SystemUIServer.app"

STATIC SecTrustedApplicationRef
create_trusted_app_from_bundle_resource(CFStringRef bundle_path,
					CFStringRef resource_path)
{
    CFBundleRef			bundle;
    CFURLRef			bundle_url;
    CFURLRef			eapolclient_url = NULL;
    char			path[MAXPATHLEN];
    Boolean			success = FALSE;
    SecTrustedApplicationRef	trusted_app = NULL;

    bundle_url = CFURLCreateWithFileSystemPath(NULL,
					       bundle_path,
					       kCFURLPOSIXPathStyle, FALSE);
    if (bundle_url == NULL) {
	goto done;
    }
    bundle = CFBundleCreate(NULL, bundle_url);
    CFRelease(bundle_url);
    if (bundle == NULL) {
	goto done;
    }
    eapolclient_url 
	= CFBundleCopyResourceURL(bundle, CFSTR(keapolclientPath),
				  NULL, NULL);
    CFRelease(bundle);
    if (eapolclient_url == NULL) {
	goto done;
    }
    success = CFURLGetFileSystemRepresentation(eapolclient_url,
					       TRUE,
					       (UInt8 *)path, sizeof(path));
    CFRelease(eapolclient_url);
    if (success) {
	OSStatus	status;

	status = SecTrustedApplicationCreateFromPath(path,
						     &trusted_app);
	if (status != noErr) {
	    fprintf(stderr,
		    "SecTrustedApplicationCreateFromPath(%s) failed, %d\n",
		    path, (int)status);
	}
    }

 done:
    return (trusted_app);
}

STATIC CFArrayRef
copy_trusted_applications(bool eapolclient_only)
{
    CFMutableArrayRef		array;
    SecTrustedApplicationRef	trusted_app;
    OSStatus			status;

    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    /* eapolclient */
    trusted_app
	= create_trusted_app_from_bundle_resource(CFSTR(kEAPOLControllerPath),
						  CFSTR(keapolclientPath));
    if (trusted_app != NULL) {
	CFArrayAppendValue(array, trusted_app);
	CFRelease(trusted_app);
    }

    /* AirPort Application Group */
    status 
	= SecTrustedApplicationCreateApplicationGroup(kAirPortApplicationGroup,
						      NULL, &trusted_app);
    if (status != noErr) {
	fprintf(stderr,
		"SecTrustedApplicationCreateApplicationGroup("
		kAirPortApplicationGroup ") failed, %d\n",
		(int)status);
    }
    else {
	CFArrayAppendValue(array, trusted_app);
	CFRelease(trusted_app);
    }
    if (eapolclient_only) {
	goto done;
    }

    /* this executable */
    status = SecTrustedApplicationCreateFromPath(NULL, &trusted_app);
    if (status != noErr) {
	fprintf(stderr,
		"SecTrustedApplicationCreateFromPath(NULL) failed, %d\n",
		(int)status);
    }
    else {
	CFArrayAppendValue(array, trusted_app);
	CFRelease(trusted_app);
    }

    /* SystemUIServer */
    status = SecTrustedApplicationCreateFromPath(kSystemUIServerPath, 
						 &trusted_app);
    if (status != noErr) {
	fprintf(stderr,
		"SecTrustedApplicationCreateFromPath(%s) failed, %d\n",
		kSystemUIServerPath,
		(int)status);
    }
    else {
	CFArrayAppendValue(array, trusted_app);
	CFRelease(trusted_app);
    }

 done:
    if (CFArrayGetCount(array) == 0) {
	my_CFRelease(&array);
    }
    return (array);
}

OSStatus
EAPOLClientSetACLForIdentity(SecIdentityRef identity)
{
    SecKeyRef		private_key = NULL;
    CFArrayRef		trusted_apps = NULL;
    OSStatus		status;

    status = SecIdentityCopyPrivateKey(identity, &private_key);
    if (status != noErr) {
	goto done;
    }
    trusted_apps = copy_trusted_applications(TRUE);
    if (trusted_apps == NULL) {
	status = errSecParam;
	goto done;
    }
    status 
	= EAPSecKeychainItemSetAccessForTrustedApplications((SecKeychainItemRef)
							    private_key,
							    trusted_apps);
 done:
    my_CFRelease(&private_key);
    my_CFRelease(&trusted_apps);
    return (status);
}

STATIC CFDataRef
itemID_copy_data(EAPOLClientItemIDRef itemID)
{
    CFDataRef		data;
    CFDictionaryRef	dict;

    dict = EAPOLClientItemIDCopyDictionary(itemID);
    data = CFPropertyListCreateXMLData(NULL, dict);
    if (dict != NULL) {
        CFRelease(dict);
    }
    return (data);
}

STATIC mach_port_t
eapolcfg_auth_server_port(void)
{
    kern_return_t	kret;
    mach_port_t		server;

#ifdef BOOTSTRAP_PRIVILEGED_SERVER
    kret = bootstrap_look_up2(bootstrap_port,
			      EAPOLCFG_AUTH_SERVER,
			      &server,
			      0,
			      BOOTSTRAP_PRIVILEGED_SERVER);

#else /* BOOTSTRAP_PRIVILEGED_SERVER */
    kret = bootstrap_look_up(bootstrap_port, EAPOLCFG_AUTH_SERVER, server_p);

#endif /* BOOTSTRAP_PRIVILEGED_SERVER */

    if (kret != BOOTSTRAP_SUCCESS) {
	/* just to make sure */
	server = MACH_PORT_NULL;
	EAPLOG(LOG_NOTICE, "EAPOLClientItemID: can't lookup eapolcfg_auth");
    }
    return (server);
}

/**
 ** interface to eapolcfg_auth MiG routines
 **/
STATIC Boolean
authEAPOLClientItemIDSetIdentity(EAPOLClientItemIDRef itemID,
				 SecIdentityRef identity)
{
    AuthorizationExternalForm *	auth_ext_p;
    CFDataRef			id_data = NULL;
    OOBData_t			id_handle;
    mach_msg_type_number_t	id_handle_length;
    CFDataRef			itemID_data = NULL;
    kern_return_t 		kret;
    int				result = ENXIO;
    mach_port_t			server;

    server = eapolcfg_auth_server_port();
    if (server == MACH_PORT_NULL) {
	return (FALSE);
    }
    auth_ext_p = EAPOLClientItemIDGetAuthorizationExternalForm(itemID);
    if (identity != NULL) {
	id_data = EAPSecIdentityHandleCreate(identity);
	if (id_data == NULL) {
	    goto done;
	}
	id_handle = (OOBData_t)CFDataGetBytePtr(id_data);
	id_handle_length = CFDataGetLength(id_data);
    }
    else {
	id_handle = NULL;
	id_handle_length = 0;
    }
    itemID_data = itemID_copy_data(itemID);
    kret = eapolclientitemid_set_identity(server,
					  auth_ext_p->bytes,
					  sizeof(auth_ext_p->bytes),
					  (xmlData_t)
					  CFDataGetBytePtr(itemID_data),
					  CFDataGetLength(itemID_data),
					  id_handle,
					  id_handle_length,
					  &result);
    if (kret != KERN_SUCCESS) {
	EAPLOG(LOG_ERR, "eapolclientitemid_set_identity failed %d",
	       kret);
    }
    if (result != 0) {
	EAPLOG(LOG_NOTICE, "eapolclientitemid_set_identity() returned %d",
	       result);
    }
 done:
    my_CFRelease(&itemID_data);
    my_CFRelease(&id_data);
    return (result == 0);
}

STATIC Boolean
authEAPOLClientItemIDSetPasswordItem(EAPOLClientItemIDRef itemID,
				     CFDataRef name_data,
				     CFDataRef password_data)
{
    AuthorizationExternalForm *	auth_ext_p;
    CFDataRef			itemID_data;
    uint32_t			flags = 0;
    kern_return_t 		kret;
    OOBData_t			name;
    mach_msg_type_number_t	name_length;
    OOBData_t			password;
    mach_msg_type_number_t	password_length;
    int				result = ENXIO;
    mach_port_t			server;

    server = eapolcfg_auth_server_port();
    if (server == MACH_PORT_NULL) {
	return (FALSE);
    }
    auth_ext_p = EAPOLClientItemIDGetAuthorizationExternalForm(itemID);
    if (name_data != NULL) {
	flags |= keapolcfg_auth_set_name;
	name = (OOBData_t)CFDataGetBytePtr(name_data);
	name_length = CFDataGetLength(name_data);
    }
    else {
	name = NULL;
	name_length = 0;
    }
    if (password_data != NULL) {
	flags |= keapolcfg_auth_set_password;
	password = (OOBData_t)CFDataGetBytePtr(password_data);
	password_length = CFDataGetLength(password_data);
    }
    else {
	password = NULL;
	password_length = 0;
    }
    itemID_data = itemID_copy_data(itemID);
    kret = eapolclientitemid_set_password(server,
					  auth_ext_p->bytes,
					  sizeof(auth_ext_p->bytes),
					  (xmlData_t)
					  CFDataGetBytePtr(itemID_data),
					  CFDataGetLength(itemID_data),
					  flags,
					  name, name_length,
					  password, password_length,
					  &result);
    if (kret != KERN_SUCCESS) {
	EAPLOG(LOG_ERR, "eapolclientitemid_set_password failed %d",
	       kret);
    }
    if (result != 0) {
	EAPLOG(LOG_NOTICE, "eapolclientitemid_set_password() returned %d",
	       result);
    }
    my_CFRelease(&itemID_data);
    return (result == 0);
}

STATIC Boolean
authEAPOLClientItemIDRemovePasswordItem(EAPOLClientItemIDRef itemID)
{
    AuthorizationExternalForm *	auth_ext_p;
    CFDataRef			itemID_data;
    kern_return_t 		kret;
    int				result = ENXIO;
    mach_port_t			server;

    server = eapolcfg_auth_server_port();
    if (server == MACH_PORT_NULL) {
	return (FALSE);
    }
    auth_ext_p = EAPOLClientItemIDGetAuthorizationExternalForm(itemID);
    itemID_data = itemID_copy_data(itemID);
    kret = eapolclientitemid_remove_password(server,
					     auth_ext_p->bytes,
					     sizeof(auth_ext_p->bytes),
					     (xmlData_t)
					     CFDataGetBytePtr(itemID_data),
					     CFDataGetLength(itemID_data),
					     &result);
    if (kret != KERN_SUCCESS) {
	EAPLOG(LOG_ERR, "eapolclientitemid_remove_password failed %d",
	       kret);
    }
    if (result != 0) {
	EAPLOG(LOG_DEBUG, "eapolclientitemid_remove_password() returned %d",
	       result);
    }
    my_CFRelease(&itemID_data);
    return (result == 0);
}

STATIC Boolean
authEAPOLClientItemIDCopyPasswordItem(EAPOLClientItemIDRef itemID,
				      CFDataRef * name_data_p,
				      CFDataRef * password_data_p)
{
    AuthorizationExternalForm *	auth_ext_p;
    CFDataRef			itemID_data;
    kern_return_t 		kret;
    OOBDataOut_t		name = NULL;
    mach_msg_type_number_t	name_length = 0;
    boolean_t			password_set = FALSE;
    int				result = ENXIO;
    mach_port_t			server;

    server = eapolcfg_auth_server_port();
    if (server == MACH_PORT_NULL) {
	return (FALSE);
    }
    auth_ext_p = EAPOLClientItemIDGetAuthorizationExternalForm(itemID);
    itemID_data = itemID_copy_data(itemID);
    kret = eapolclientitemid_check_password(server,
					    auth_ext_p->bytes,
					    sizeof(auth_ext_p->bytes),
					    (xmlData_t)
					    CFDataGetBytePtr(itemID_data),
					    CFDataGetLength(itemID_data),
					    &name, &name_length,
					    &password_set,
					    &result);
    if (kret != KERN_SUCCESS) {
	EAPLOG(LOG_ERR, "eapolclientitemid_check_password failed %d",
	       kret);
    }
    if (result != 0) {
	EAPLOG(LOG_DEBUG, "eapolclientitemid_check_password() returned %d",
	       result);
    }

    if (name_data_p != NULL) {
	if (name == NULL) {
	    *name_data_p = NULL;
	}
	else {
	    *name_data_p = CFDataCreate(NULL, (const UInt8 *)name,
					name_length);
	}
    }
    if (password_data_p != NULL) {
	if (password_set == FALSE) {
	    *password_data_p = NULL;
	}
	else {
	    /* don't return the actual password, return a fake password */
#define FAKE_PASSWORD		"XXXXXXXX"
#define FAKE_PASSWORD_LENGTH	(sizeof(FAKE_PASSWORD) - 1)	
	    *password_data_p = CFDataCreate(NULL,
					    (const UInt8 *)FAKE_PASSWORD,
					    FAKE_PASSWORD_LENGTH);
	}
    }
    if (name != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)name,
			    name_length);
    }
    my_CFRelease(&itemID_data);
    return (result == 0);
}

/**
 ** EAPOLClientItemIDCopyUniqueString
 ** - get the unique string for the itemID
 **/

/* for password/name Item */
#define WLAN_SSID_STR	"wlan.ssid"
#define WLAN_DOMAIN_STR	"wlan.domain"
#define PROFILEID_STR	"profileid"
#define DEFAULT_STR	"default"

STATIC const char	kItemDescription[] = "802.1X Password";
STATIC int		kItemDescriptionLength = sizeof(kItemDescription) - 1;

#define EAP_PREFIX_STR	"com.apple.network.eap.%s.%s.%s"

INLINE CFStringRef
create_item_format(const char * domain, const char * type, const char * unique,
		   CFStringRef value)
{
    CFStringRef		str;

    if (value != NULL) {
	str = CFStringCreateWithFormat(NULL, NULL, 
				       CFSTR(EAP_PREFIX_STR ".%@"),
				       domain, type, unique, value);
    }
    else {
	str = CFStringCreateWithFormat(NULL, NULL, 
				       CFSTR(EAP_PREFIX_STR),
				       domain, type, unique);
    }
    return (str);
}

STATIC CFStringRef
EAPOLClientItemIDCopyUniqueString(EAPOLClientItemIDRef itemID,
				  EAPOLClientDomain domain, bool is_item)
{
    const char *	domain_str;
    CFStringRef		result = NULL;
    CFStringRef		profileID;
    CFDataRef		ssid;
    CFStringRef		ssid_str;
    const char *	type_str;

    type_str = is_item ? "item" : "identity";
    domain_str = (domain == kEAPOLClientDomainSystem) ? "system" : "user";
    switch (itemID->type) {
    case kEAPOLClientItemIDTypeWLANSSID:
	ssid_str = my_CFStringCreateWithData(itemID->u.ssid);
	result = create_item_format(domain_str, type_str, WLAN_SSID_STR,
				    ssid_str);
	if (ssid_str != NULL) {
	    CFRelease(ssid_str);
	}
	break;
    case kEAPOLClientItemIDTypeWLANDomain:
	result = create_item_format(domain_str, type_str,
				    WLAN_DOMAIN_STR, itemID->u.domain);
	break;
    case kEAPOLClientItemIDTypeProfileID:
	result = create_item_format(domain_str, type_str, PROFILEID_STR,
				    itemID->u.profileID);
	break;
    case kEAPOLClientItemIDTypeProfile:
	ssid = EAPOLClientProfileGetWLANSSIDAndSecurityType(itemID->u.profile,
							    NULL);
	if (ssid != NULL) {
	    ssid_str = my_CFStringCreateWithData(ssid);
	    result = create_item_format(domain_str, type_str, WLAN_SSID_STR,
					ssid_str);
	    if (ssid_str != NULL) {
		CFRelease(ssid_str);
	    }
	}
	else {
	    CFStringRef		wlan_domain;

	    wlan_domain = EAPOLClientProfileGetWLANDomain(itemID->u.profile);
	    if (wlan_domain != NULL) {
		result = create_item_format(domain_str, type_str,
					    WLAN_DOMAIN_STR, wlan_domain);
	    }
	    else {
		profileID = EAPOLClientProfileGetID(itemID->u.profile);
		result = create_item_format(domain_str, type_str, PROFILEID_STR,
					    profileID);
	    }
	}
	break;
    case kEAPOLClientItemIDTypeDefault:
	result = create_item_format(domain_str, type_str, DEFAULT_STR, NULL);
	break;
    default:
	break;
    }
    return (result);
}

/**
 ** CF object glue code
 **/
STATIC CFStringRef	__EAPOLClientItemIDCopyDebugDesc(CFTypeRef cf);
STATIC void		__EAPOLClientItemIDDeallocate(CFTypeRef cf);
STATIC Boolean		__EAPOLClientItemIDEqual(CFTypeRef cf1, CFTypeRef cf2);
STATIC CFHashCode	__EAPOLClientItemIDHash(CFTypeRef cf);

STATIC CFTypeID __kEAPOLClientItemIDTypeID = _kCFRuntimeNotATypeID;

STATIC const CFRuntimeClass __EAPOLClientItemIDClass = {
    0,					/* version */
    "EAPOLClientItemID",		/* className */
    NULL,				/* init */
    NULL,				/* copy */
    __EAPOLClientItemIDDeallocate,	/* deallocate */
    __EAPOLClientItemIDEqual,		/* equal */
    __EAPOLClientItemIDHash,		/* hash */
    NULL,				/* copyFormattingDesc */
    __EAPOLClientItemIDCopyDebugDesc	/* copyDebugDesc */
};

STATIC CFStringRef
__EAPOLClientItemIDCopyDebugDesc(CFTypeRef cf)
{
    CFAllocatorRef		allocator = CFGetAllocator(cf);
    EAPOLClientItemIDRef	itemID = (EAPOLClientItemIDRef)cf;
    CFStringRef			profileID;
    CFMutableStringRef		result;
    CFStringRef			ssid_str;

    result = CFStringCreateMutable(allocator, 0);
    CFStringAppendFormat(result, NULL, 
			 CFSTR("<EAPOLClientItemID %p [%p]> {"), cf, allocator);
    switch (itemID->type) {
    case kEAPOLClientItemIDTypeWLANSSID:
	ssid_str = my_CFStringCreateWithData(itemID->u.ssid);
	CFStringAppendFormat(result, NULL, CFSTR("WLAN SSID = %@"),
			     ssid_str);
	CFRelease(ssid_str);
	break;
    case kEAPOLClientItemIDTypeWLANDomain:
	CFStringAppendFormat(result, NULL, CFSTR("WLAN domain = %@"),
			     itemID->u.domain);
	break;
    case kEAPOLClientItemIDTypeProfileID:
	CFStringAppendFormat(result, NULL, CFSTR("ProfileID = %@"),
			     itemID->u.profileID);
	break;
    case kEAPOLClientItemIDTypeProfile:
	profileID = EAPOLClientProfileGetID(itemID->u.profile);
	CFStringAppendFormat(result, NULL, CFSTR("Profile = %@"),
			     profileID);
	break;
    case kEAPOLClientItemIDTypeDefault:
	CFStringAppend(result, CFSTR("Default"));
	break;
    default:
	break;
    }
    CFStringAppend(result, CFSTR("}"));
    return result;
}


STATIC void
__EAPOLClientItemIDDeallocate(CFTypeRef cf)
{
    EAPOLClientItemIDRef itemID	= (EAPOLClientItemIDRef)cf;

    switch (itemID->type) {
    case kEAPOLClientItemIDTypeWLANSSID:
	CFRelease(itemID->u.ssid);
	break;
    case kEAPOLClientItemIDTypeWLANDomain:
	CFRelease(itemID->u.domain);
	break;
    case kEAPOLClientItemIDTypeProfileID:
	CFRelease(itemID->u.profileID);
	break;
    case kEAPOLClientItemIDTypeProfile:
	CFRelease(itemID->u.profile);
	break;
    default:
	break;
    }
    return;
}


STATIC Boolean
__EAPOLClientItemIDEqual(CFTypeRef cf1, CFTypeRef cf2)
{
    EAPOLClientItemIDRef 	id1 = (EAPOLClientItemIDRef)cf1;
    EAPOLClientItemIDRef	id2 = (EAPOLClientItemIDRef)cf2;

    if (id1->type != id2->type) {
	return (FALSE);
    }
    return (CFEqual(id1->u.ptr, id2->u.ptr));
}

STATIC CFHashCode
__EAPOLClientItemIDHash(CFTypeRef cf)
{
    EAPOLClientItemIDRef 	itemID = (EAPOLClientItemIDRef)cf;

    return (CFHash(itemID->u.ptr));
}


STATIC void
__EAPOLClientItemIDInitialize(void)
{
    /* initialize runtime */
    __kEAPOLClientItemIDTypeID 
	= _CFRuntimeRegisterClass(&__EAPOLClientItemIDClass);
    return;
}

STATIC void
__EAPOLClientItemIDRegisterClass(void)
{
    STATIC pthread_once_t	initialized = PTHREAD_ONCE_INIT;

    pthread_once(&initialized, __EAPOLClientItemIDInitialize);
    return;
}


STATIC EAPOLClientItemIDRef
__EAPOLClientItemIDAllocate(CFAllocatorRef allocator)
{
    EAPOLClientItemIDRef	itemID;

    __EAPOLClientItemIDRegisterClass();

    itemID = (EAPOLClientItemIDRef)
	_CFRuntimeCreateInstance(allocator,
				 __kEAPOLClientItemIDTypeID,
				 sizeof(*itemID) - sizeof(CFRuntimeBase),
				 NULL);
    return (itemID);
}

/**
 ** EAPOLClientItemID APIs
 **/

CFTypeID
EAPOLClientItemIDGetTypeID(void)
{
    __EAPOLClientItemIDRegisterClass();
    return (__kEAPOLClientItemIDTypeID);
}

/*
 * Function: EAPOLClientItemIDCreateWithProfileID
 *
 * Purpose:
 *   Create an EAPOLClientItemID instance based on the supplied profileID
 */
EAPOLClientItemIDRef
EAPOLClientItemIDCreateWithProfileID(CFStringRef profileID)
{
    EAPOLClientItemIDRef	itemID;

    itemID = __EAPOLClientItemIDAllocate(CFGetAllocator(profileID));
    if (itemID == NULL) {
	return (NULL);
    }
    itemID->type = kEAPOLClientItemIDTypeProfileID;
    itemID->u.profileID = CFRetain(profileID);
    return (itemID);
}

/*
 * Function: EAPOLClientItemIDCreateWithWLANSSID
 *
 * Purpose:
 *   Create an EAPOLClientItemID instance based on the supplied WLAN SSID.
 */
EAPOLClientItemIDRef
EAPOLClientItemIDCreateWithWLANSSID(CFDataRef ssid)
{
    EAPOLClientItemIDRef	itemID;

    itemID = __EAPOLClientItemIDAllocate(CFGetAllocator(ssid));
    if (itemID == NULL) {
	return (NULL);
    }
    itemID->type = kEAPOLClientItemIDTypeWLANSSID;
    itemID->u.ssid = CFRetain(ssid);
    return (itemID);
}

/*
 * Function: EAPOLClientItemIDCreateWithWLANDomain
 *
 * Purpose:
 *   Create an EAPOLClientItemID instance based on the supplied WLAN 
 *   Hotspot 2.0 domain name.
 */
EAPOLClientItemIDRef
EAPOLClientItemIDCreateWithWLANDomain(CFStringRef domain)
{
    EAPOLClientItemIDRef	itemID;

    itemID = __EAPOLClientItemIDAllocate(CFGetAllocator(domain));
    if (itemID == NULL) {
	return (NULL);
    }
    itemID->type = kEAPOLClientItemIDTypeWLANDomain;
    itemID->u.domain = CFRetain(domain);
    return (itemID);
}

/*
 * Function: EAPOLClientItemIDCreateWithProfile
 *
 * Purpose:
 *   Create an EAPOLClientItemID instance based on the supplied
 *   EAPOLClientProfileRef.
 */
EAPOLClientItemIDRef
EAPOLClientItemIDCreateWithProfile(EAPOLClientProfileRef profile)
{
    EAPOLClientItemIDRef	itemID;

    itemID = __EAPOLClientItemIDAllocate(CFGetAllocator(profile));
    if (itemID == NULL) {
	return (NULL);
    }
    itemID->type = kEAPOLClientItemIDTypeProfile;
    CFRetain(profile);
    itemID->u.profile = profile;
    return (itemID);
}

/*
 * Function: EAPOLClientItemIDCreateDefault
 *
 * Purpose:
 *   Create an EAPOLClientItemID instance that indicates that the default
 *   authentication parameters and default keychain items are to be used.
 */
EAPOLClientItemIDRef
EAPOLClientItemIDCreateDefault(void)
{
    EAPOLClientItemIDRef	itemID;

    itemID = __EAPOLClientItemIDAllocate(NULL);
    if (itemID == NULL) {
	return (NULL);
    }
    itemID->type = kEAPOLClientItemIDTypeDefault;
    return (itemID);
}

/*
 * Function: EAPOLClientItemIDCopyPasswordItem
 *
 * Purpose:
 *   Retrieve the password item from secure storage for the particular itemID
 *   in the specified domain.
 *
 * Returns:
 *   FALSE if no such item exists, and both *username_p and *password_p
 *   are set to NULL.
 *
 *   TRUE if an item exists, and either of both *username_p and *password_p
 *   are set to a non-NULL value.
 */
Boolean
EAPOLClientItemIDCopyPasswordItem(EAPOLClientItemIDRef itemID, 
				  EAPOLClientDomain domain,
				  CFDataRef * username_p,
				  CFDataRef * password_p)
{
    CFDictionaryRef	attrs = NULL;
    int			count;
    SecKeychainRef	keychain = NULL;
    const void *	keys[2];
    CFArrayRef		req_props = NULL;
    OSStatus		status = errSecParam;
    CFStringRef		unique_string;

    count = 0;
    if (username_p == NULL && password_p == NULL) {
	return (FALSE);
    }
    switch (domain) {
    case kEAPOLClientDomainUser:
	status = SecKeychainCopyDomainDefault(kSecPreferencesDomainUser,
					      &keychain);
	if (status != noErr) {
	    fprintf(stderr, "EAPOLClientItemIDCopyPasswordItem can't get"
		    " User keychain\n");
	    return (FALSE);
	}
	break;
    case kEAPOLClientDomainSystem:
	if (EAPOLClientItemIDGetAuthorizationExternalForm(itemID) != NULL) {
	    return (authEAPOLClientItemIDCopyPasswordItem(itemID, username_p,
							  password_p));
	}
	status = SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem,
					      &keychain);
	if (status != noErr) {
	    fprintf(stderr, "EAPOLClientItemIDCopyPasswordItem can't get"
		    " System keychain\n");
	    return (FALSE);
	}
	break;
    default:
	return (FALSE);
    }
    if (username_p != NULL) {
	keys[count] = kEAPSecKeychainPropAccount;
	count++;
	*username_p = NULL;
    }
    if (password_p != NULL) {
	keys[count] = kEAPSecKeychainPropPassword;
	count++;
	*password_p = NULL;
    }
    req_props = CFArrayCreate(NULL, keys, count, 
			      &kCFTypeArrayCallBacks);
    unique_string = EAPOLClientItemIDCopyUniqueString(itemID, domain, TRUE);
    status = EAPSecKeychainPasswordItemCopy2(keychain,
					     unique_string,
					     req_props,
					     &attrs);
    if (status != noErr) {
	goto done;
    }
    if (username_p != NULL) {
	*username_p
	    = my_CFDictionaryCopyValue(attrs, kEAPSecKeychainPropAccount);
    }
    if (password_p != NULL) {
	*password_p
	    = my_CFDictionaryCopyValue(attrs, kEAPSecKeychainPropPassword);

    }

 done:
    my_CFRelease(&unique_string);
    my_CFRelease(&req_props);
    my_CFRelease(&attrs);
    my_CFRelease(&keychain);
    return (status == noErr);
}

/*
 * Function: EAPOLClientItemIDSetPasswordItem
 *
 * Purpose:
 *   Set the password item in secure storage for the specified itemID
 *   in the specified domain.
 *
 *   Passing an empty 'username' or 'password' removes the corresponding
 *   attribute.   If both 'username' and 'password' are empty, the item is
 *   also removed.  An empty value is a non-NULL CFDataRef of length 0.
 *
 *   Passing NULL for 'username' or 'password' means that the corresponding
 *   item attribute is left alone.  If both 'username" and 'password' are
 *   NULL, the call has no effect, but TRUE is still returned.
 *
 *   Passing non-NULL, non-empty 'username' or 'password' sets the
 *   corresponding item attribute to the specified value.   If the item
 *   does not exist, it will be created.
 *
 * Returns:
 *   FALSE if the operation did not succeed, TRUE otherwise.
 */
Boolean
EAPOLClientItemIDSetPasswordItem(EAPOLClientItemIDRef itemID,
				 EAPOLClientDomain domain,
				 CFDataRef name, CFDataRef password)
{
    CFMutableDictionaryRef	attrs = NULL;
    CFDataRef			data;
    SecKeychainRef		keychain = NULL;
    CFDataRef			ssid;
    OSStatus			status = errSecParam;
    CFStringRef			unique_string;

    if (name == NULL && password == NULL) {
	return (TRUE);
    }
    switch (domain) {
    case kEAPOLClientDomainUser:
	status = SecKeychainCopyDomainDefault(kSecPreferencesDomainUser,
					      &keychain);
	if (status != noErr) {
	    fprintf(stderr, "EAPOLClientItemIDSetPasswordItem can't get"
		    " User keychain,  %s (%d)\n",
		    EAPSecurityErrorString(status), (int)status);
	    return (FALSE);
	}
	break;
    case kEAPOLClientDomainSystem:
	if (EAPOLClientItemIDGetAuthorizationExternalForm(itemID) != NULL) {
	    return (authEAPOLClientItemIDSetPasswordItem(itemID, name,
							 password));
	}
	status = SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem,
					      &keychain);
	if (status != noErr) {
	    fprintf(stderr, "EAPOLClientItemIDSetPasswordItem can't get"
		    " System keychain,  %s (%d)\n",
		    EAPSecurityErrorString(status), (int)status);
	    return (FALSE);
	}
	break;
    default:
	return (FALSE);
    }

    /* populate an attributes dictionary */
    attrs = CFDictionaryCreateMutable(NULL, 0,
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);

    /* Description */
    data = CFDataCreate(NULL, (UInt8 *)kItemDescription,
			kItemDescriptionLength);
    CFDictionarySetValue(attrs, kEAPSecKeychainPropDescription, data);
    CFRelease(data);

    /* Label */
    switch (itemID->type) {
    case kEAPOLClientItemIDTypeWLANSSID:
	CFDictionarySetValue(attrs, kEAPSecKeychainPropLabel,
			     itemID->u.ssid);
	break;
    case kEAPOLClientItemIDTypeWLANDomain: {
	CFDataRef		label_data;

	label_data = my_CFDataCreateWithString(itemID->u.domain);
	CFDictionarySetValue(attrs, kEAPSecKeychainPropLabel,
			     label_data);
	CFRelease(label_data);
	break;
    }
    case kEAPOLClientItemIDTypeProfileID: {
	CFDataRef		label_data;

	label_data = my_CFDataCreateWithString(itemID->u.profileID);
	CFDictionarySetValue(attrs, kEAPSecKeychainPropLabel, label_data);
	CFRelease(label_data);
	break;
    }
    case kEAPOLClientItemIDTypeProfile:
	ssid = EAPOLClientProfileGetWLANSSIDAndSecurityType(itemID->u.profile,
							    NULL);
	if (ssid != NULL) {
	    CFDictionarySetValue(attrs, kEAPSecKeychainPropLabel,
				 ssid);
	}
	else {
	    CFStringRef		label;
	    CFDataRef		label_data;
    
	    label = EAPOLClientProfileGetUserDefinedName(itemID->u.profile);
	    if (label == NULL) {
		label = EAPOLClientProfileGetWLANDomain(itemID->u.profile);
		if (label == NULL) {
		    label = EAPOLClientProfileGetID(itemID->u.profile);
		}
	    }
	    label_data = my_CFDataCreateWithString(label);
	    CFDictionarySetValue(attrs, kEAPSecKeychainPropLabel, label_data);
	    CFRelease(label_data);
	}
	break;
    case kEAPOLClientItemIDTypeDefault: {
	CFDataRef		label_data;

	/* XXX localize? */
	label_data = my_CFDataCreateWithString(CFSTR("Default"));
	CFDictionarySetValue(attrs, kEAPSecKeychainPropLabel, label_data);
	CFRelease(label_data);
	break;
    }
    default:
	goto done;
    }
    
    /* Account */
    if (name != NULL) {
	CFDictionarySetValue(attrs, kEAPSecKeychainPropAccount, name);
    }
    
    /* Password */
    if (password != NULL) {
	CFDictionarySetValue(attrs, kEAPSecKeychainPropPassword, password);
    };

    if (domain == kEAPOLClientDomainUser) {
	CFArrayRef			trusted_apps;

	/* Trusted Applications */
	trusted_apps = copy_trusted_applications(FALSE);
	if (trusted_apps != NULL) {
	    CFDictionarySetValue(attrs,
				 kEAPSecKeychainPropTrustedApplications,
				 trusted_apps);
	    CFRelease(trusted_apps);
	}
    }
    else {
	CFDictionarySetValue(attrs,
			     kEAPSecKeychainPropAllowRootAccess,
			     kCFBooleanTrue);
    }
    unique_string = EAPOLClientItemIDCopyUniqueString(itemID, domain, TRUE);
    status = EAPSecKeychainPasswordItemSet2(keychain, unique_string,
					    attrs);
    if (status == errSecItemNotFound) {
	status = EAPSecKeychainPasswordItemCreate(keychain,
						  unique_string,
						  attrs);
    }
    my_CFRelease(&unique_string);
    if (status != noErr) {
	EAPLOG(LOG_NOTICE,
	       "EAPOLClientItemID: failed to set keychain item, %d",
	       (int)status);
    }

 done:
    my_CFRelease(&attrs);
    my_CFRelease(&keychain);
    return (status == noErr);
}

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
				    EAPOLClientDomain domain)
{
    SecKeychainRef		keychain = NULL;
    OSStatus			status = errSecParam;
    CFStringRef			unique_string;

    switch (domain) {
    case kEAPOLClientDomainUser:
	status = SecKeychainCopyDomainDefault(kSecPreferencesDomainUser,
					      &keychain);
	if (status != noErr) {
	    fprintf(stderr, "EAPOLClientItemIDSetPasswordItem can't get"
		    " User keychain,  %s (%d)\n",
		    EAPSecurityErrorString(status), (int)status);
	    return (FALSE);
	}
	break;
    case kEAPOLClientDomainSystem:
	if (EAPOLClientItemIDGetAuthorizationExternalForm(itemID) != NULL) {
	    return (authEAPOLClientItemIDRemovePasswordItem(itemID));
	}
	status = SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem,
					      &keychain);
	if (status != noErr) {
	    fprintf(stderr, "EAPOLClientItemIDSetPasswordItem can't get"
		    " System keychain,  %s (%d)\n",
		    EAPSecurityErrorString(status), (int)status);
	    return (FALSE);
	}
	break;
    default:
	return (FALSE);
    }
    unique_string = EAPOLClientItemIDCopyUniqueString(itemID, domain, TRUE);
    status = EAPSecKeychainPasswordItemRemove(keychain,
					      unique_string);
    my_CFRelease(&unique_string);
    my_CFRelease(&keychain);
    return (status == noErr);
}

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
			      EAPOLClientDomain domain)
{
    SecPreferencesDomain 	current_domain;
    SecIdentityRef		identity = NULL;
    SecPreferencesDomain 	required_domain;
    OSStatus			status;
    CFStringRef			unique_string;

    switch (domain) {
    case kEAPOLClientDomainUser:
	required_domain = kSecPreferencesDomainUser;
	break;
    case kEAPOLClientDomainSystem:
	required_domain = kSecPreferencesDomainSystem;
	break;
    default:
	return (NULL);
    }
    status = SecKeychainGetPreferenceDomain(&current_domain);
    if (status != noErr) {
	return (NULL);
    }
    if (current_domain != required_domain) {
	status = SecKeychainSetPreferenceDomain(required_domain);
	if (status != noErr) {
	    return (NULL);
	}
    }
    unique_string = EAPOLClientItemIDCopyUniqueString(itemID, domain, FALSE);
    identity = SecIdentityCopyPreferred(unique_string, NULL, NULL);
    if (current_domain != required_domain) {
	(void)SecKeychainSetPreferenceDomain(current_domain);
    }
    my_CFRelease(&unique_string);
    return (identity);
}

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
 */
Boolean
EAPOLClientItemIDSetIdentity(EAPOLClientItemIDRef itemID,
			     EAPOLClientDomain domain,
			     SecIdentityRef identity)
{
    SecPreferencesDomain 	current_domain;
    SecPreferencesDomain 	required_domain;
    OSStatus			status;
    CFStringRef			unique_string;

    switch (domain) {
    case kEAPOLClientDomainUser:
	required_domain = kSecPreferencesDomainUser;
	break;
    case kEAPOLClientDomainSystem:
	if (EAPOLClientItemIDGetAuthorizationExternalForm(itemID) != NULL) {
	    return (authEAPOLClientItemIDSetIdentity(itemID, identity));
	}
	required_domain = kSecPreferencesDomainSystem;
	break;
    default:
	return (FALSE);
    }

    status = SecKeychainGetPreferenceDomain(&current_domain);
    if (status != noErr) {
	return (FALSE);
    }
    if (required_domain != current_domain) {
	status = SecKeychainSetPreferenceDomain(required_domain);
	if (status != noErr) {
	    return (FALSE);
	}
    }
    unique_string = EAPOLClientItemIDCopyUniqueString(itemID, domain, FALSE);
    status = SecIdentitySetPreferred(identity, unique_string, NULL);
    if (status != noErr) {
	fprintf(stderr, "SecIdentitySetPreference failed %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
    }

    if (current_domain != required_domain) {
	(void)SecKeychainSetPreferenceDomain(current_domain);
    }
    my_CFRelease(&unique_string);
    return (status == noErr);
}

/**
 ** Private functions
 **/


CFStringRef
EAPOLClientItemIDGetProfileID(EAPOLClientItemIDRef itemID)
{
    switch (itemID->type) {
    case kEAPOLClientItemIDTypeProfileID:
	return (itemID->u.profileID);
    case kEAPOLClientItemIDTypeProfile:
	return (EAPOLClientProfileGetID(itemID->u.profile));
    default:
	break;
    }
    return (NULL);
}

CFDataRef
EAPOLClientItemIDGetWLANSSID(EAPOLClientItemIDRef itemID)
{
    switch (itemID->type) {
    case kEAPOLClientItemIDTypeWLANSSID:
	return (itemID->u.ssid);
    case kEAPOLClientItemIDTypeProfile:
	return (EAPOLClientProfileGetWLANSSIDAndSecurityType(itemID->u.profile,
							     NULL));
    default:
	break;
    }
    return (NULL);
}

CFStringRef
EAPOLClientItemIDGetWLANDomain(EAPOLClientItemIDRef itemID)
{
    switch (itemID->type) {
    case kEAPOLClientItemIDTypeWLANDomain:
	return (itemID->u.domain);
    case kEAPOLClientItemIDTypeProfile:
	return (EAPOLClientProfileGetWLANDomain(itemID->u.profile));
    default:
	break;
    }
    return (NULL);
}

EAPOLClientProfileRef
EAPOLClientItemIDGetProfile(EAPOLClientItemIDRef itemID)
{
    switch (itemID->type) {
    case kEAPOLClientItemIDTypeProfile:
	return (itemID->u.profile);
    default:
	break;
    }
    return (NULL);
}

#define kItemProfileID		CFSTR("ProfileID")
#define kItemDomain		CFSTR("Domain")
#define kItemSSID		CFSTR("SSID")
#define kItemDefault		CFSTR("Default")

CFDictionaryRef
EAPOLClientItemIDCopyDictionary(EAPOLClientItemIDRef itemID)
{
    const void *	key;
    CFStringRef		profileID;
    const void *	value;

    profileID = EAPOLClientItemIDGetProfileID(itemID);
    if (profileID != NULL) {
	key = (const void *)kItemProfileID;
	value = (const void *)profileID;
    }
    else {
	CFDataRef		ssid = EAPOLClientItemIDGetWLANSSID(itemID);

	if (ssid != NULL) {
	    key = (const void *)kItemSSID;
	    value = (const void *)ssid;
	}
	else {
	    switch (itemID->type) {
	    case kEAPOLClientItemIDTypeWLANDomain:
		key = (const void *)kItemDomain;
		value = (const void *)itemID->u.domain;
		break;
	    case kEAPOLClientItemIDTypeDefault:
		key = (const void *)kItemDefault;
		value = (const void *)kCFBooleanTrue;
		break;
	    default:
		return (NULL);
	    }
	}
    }
    return (CFDictionaryCreate(NULL, &key, &value, 1,
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks));
}

EAPOLClientItemIDRef
EAPOLClientItemIDCreateWithDictionary(EAPOLClientConfigurationRef cfg,
				      CFDictionaryRef dict)
{
    CFStringRef			domain;
    EAPOLClientProfileRef	profile;
    CFStringRef			profileID;
    CFDataRef			ssid;

    if (isA_CFDictionary(dict) == NULL) {
	return (NULL);
    }
    profileID = CFDictionaryGetValue(dict, kItemProfileID);
    if (isA_CFString(profileID) != NULL) {
	if (cfg != NULL) {
	    profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
	    if (profile != NULL) {
		return (EAPOLClientItemIDCreateWithProfile(profile));
	    }
	}
	return (EAPOLClientItemIDCreateWithProfileID(profileID));
    }
    ssid = CFDictionaryGetValue(dict, kItemSSID);
    if (isA_CFData(ssid) != NULL) {
	if (cfg != NULL) {
	    profile = EAPOLClientConfigurationGetProfileWithWLANSSID(cfg, ssid);
	    if (profile != NULL) {
		return (EAPOLClientItemIDCreateWithProfile(profile));
	    }
	}
	return (EAPOLClientItemIDCreateWithWLANSSID(ssid));
    }
    domain = CFDictionaryGetValue(dict, kItemDomain);
    if (isA_CFString(domain) != NULL) {
	if (cfg != NULL) {
	    profile 
		= EAPOLClientConfigurationGetProfileWithWLANDomain(cfg,
								   domain);
	    if (profile != NULL) {
		return (EAPOLClientItemIDCreateWithProfile(profile));
	    }
	}
	return (EAPOLClientItemIDCreateWithWLANDomain(domain));
    }
    if (CFDictionaryGetValue(dict, kItemDefault) != NULL) {
	return (EAPOLClientItemIDCreateDefault());
    }
    return (NULL);
}
 
PRIVATE_EXTERN AuthorizationExternalForm *
EAPOLClientItemIDGetAuthorizationExternalForm(EAPOLClientItemIDRef itemID)
{
    EAPOLClientConfigurationRef	cfg;
    EAPOLClientProfileRef	profile;

    profile = EAPOLClientItemIDGetProfile(itemID);
    if (profile == NULL) {
	return (NULL);
    }
    cfg = EAPOLClientProfileGetConfiguration(profile);    
    if (cfg == NULL) {
	return (NULL);
    }
    return (EAPOLClientConfigurationGetAuthorizationExternalForm(cfg));
}
