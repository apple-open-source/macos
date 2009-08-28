/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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


/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */
#include <mach/mach.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <CoreFoundation/CoreFoundation.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#include <SystemConfiguration/SCValidation.h>

#include "ppp_msg.h"
#include "../Family/if_ppplink.h"
#include "scnc_client.h"
#include "scnc_utils.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
 Given a string 'key' and a string prefix 'prefix',
 return the next component in the slash '/' separated
 key.  If no slash follows the prefix, return NULL.

 Examples:
 1. key = "a/b/c" prefix = "a/"    returns "b"
 2. key = "a/b/c" prefix = "a/b/"  returns NULL
----------------------------------------------------------------------------- */
CFStringRef parse_component(CFStringRef key, CFStringRef prefix)
{
    CFMutableStringRef	comp;
    CFRange		range;

    if (!CFStringHasPrefix(key, prefix))
    	return NULL;

    comp = CFStringCreateMutableCopy(NULL, 0, key);
    CFStringDelete(comp, CFRangeMake(0, CFStringGetLength(prefix)));
    range = CFStringFind(comp, CFSTR("/"), 0);
    if (range.location == kCFNotFound) {
        CFRelease(comp);
        return NULL;
    }
    range.length = CFStringGetLength(comp) - range.location;
    CFStringDelete(comp, range);
    return comp;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFDictionaryRef copyService(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID)
{
    CFTypeRef		data = NULL;
    CFMutableDictionaryRef	service = NULL;
    CFStringRef		key = NULL;
    int			i;
    CFStringRef         copy[] = {
        kSCEntNetPPP,
        kSCEntNetModem,
        kSCEntNetInterface,
    	kSCEntNetIPv4,
    	kSCEntNetIPv6,
#ifndef TARGET_EMBEDDED_OS
    	kSCEntNetSMB,
#endif
        kSCEntNetDNS,
        kSCEntNetL2TP,
        kSCEntNetPPTP,
        kSCEntNetIPSec,
        NULL,
    };

    key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%@"), domain, kSCCompNetwork, kSCCompService, serviceID);
    if (key == 0)
        goto fail;
        
    data = SCDynamicStoreCopyValue(store, key);
    if (data == 0) {
		data = CFDictionaryCreate(NULL, 0, 0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (data == 0)
			goto fail;
	}
        
    CFRelease(key);
	key = NULL;
        
    service = CFDictionaryCreateMutableCopy(NULL, 0, data);
    if (service == 0)
        goto fail;
        
    CFRelease(data);

    for (i = 0; copy[i]; i++) {   
        data = copyEntity(store, domain, serviceID, copy[i]);
        if (data) {
        
            CFDictionaryAddValue(service, copy[i], data);
            CFRelease(data);
        }
    }

    return service;

fail:
    if (key) 
        CFRelease(key);
    if (data)
        CFRelease(data);
    if (service)
        CFRelease(service);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFDictionaryRef copyEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, CFStringRef entity)
{
    CFTypeRef		data = NULL;
    CFStringRef		key;

    if (serviceID)
        key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, domain, serviceID, entity);
    else
        key = SCDynamicStoreKeyCreateNetworkGlobalEntity(0, domain, entity);

    if (key) {
        data = SCDynamicStoreCopyValue(store, key);
        CFRelease(key);
    }
    return data;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int existEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, CFStringRef entity)
{
    CFTypeRef		data;

    data = copyEntity(store, domain, serviceID, entity);
    if (data) {
        CFRelease(data);
        return 1;
    }
    
    return 0;
}

/* -----------------------------------------------------------------------------
get a string from the dictionnary, in service/property
----------------------------------------------------------------------------- */
int getString(CFDictionaryRef service, CFStringRef property, u_char *str, u_int16_t maxlen)
{
    CFStringRef		string;
    CFDataRef		ref;

    str[0] = 0;
    ref  = CFDictionaryGetValue(service, property);
    if (ref) {
        if (CFGetTypeID(ref) == CFStringGetTypeID()) {
            CFStringGetCString((CFStringRef)ref, str, maxlen, kCFStringEncodingUTF8);
            return 1;
        }
        else if (CFGetTypeID(ref) == CFDataGetTypeID()) {
            CFStringEncoding    encoding;

#if     __BIG_ENDIAN__
            encoding = (*(CFDataGetBytePtr(ref) + 1) == 0x00) ? kCFStringEncodingUTF16LE : kCFStringEncodingUTF16BE;
#else   // __LITTLE_ENDIAN__
            encoding = (*(CFDataGetBytePtr(ref)    ) == 0x00) ? kCFStringEncodingUTF16BE : kCFStringEncodingUTF16LE;
#endif
            string = CFStringCreateWithBytes(NULL, (const UInt8 *)CFDataGetBytePtr(ref), CFDataGetLength(ref), encoding, FALSE);
            if (string) {
                CFStringGetCString((CFStringRef)string, str, maxlen, kCFStringEncodingUTF8);
                CFRelease(string);
                return 1;
            }
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
get a number from the dictionnary, in service/property
----------------------------------------------------------------------------- */
int getNumber(CFDictionaryRef dict, CFStringRef property, u_int32_t *outval)
{
    CFNumberRef		ref;

    ref  = CFDictionaryGetValue(dict, property);
    if (ref && (CFGetTypeID(ref) == CFNumberGetTypeID())) {
        CFNumberGetValue(ref, kCFNumberSInt32Type, outval);
        return 1;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getNumberFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval)
{
    CFTypeRef		data;
    int 		ok = 0;

    if (data = copyEntity(store, domain, serviceID, entity)) {
        ok = getNumber(data, property, outval);
        CFRelease(data);
    }
    return ok;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getStringFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_char *str, u_int16_t maxlen)
{
    CFTypeRef		data;
    int 		ok = 0;

    data = copyEntity(store, domain, serviceID, entity);
    if (data) {
        ok = getString(data, property, str, maxlen);
        CFRelease(data);
    }
    return ok;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFStringRef copyCFStringFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property)
{
    CFTypeRef		data;
    CFStringRef		string, ret = 0;

    data = copyEntity(store, domain, serviceID, entity);
    if (data) {
        string  = CFDictionaryGetValue(data, property);
        if (string && (CFGetTypeID(string) == CFStringGetTypeID())) {
            CFRetain(string);
            ret = string;
        }

        CFRelease(data);
    }
    return ret; 
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int32_t CFStringAddrToLong(CFStringRef string)
{
    char 	str[100];
    u_int32_t	ret = 0;
    
    if (string) {
	str[0] = 0;
        CFStringGetCString(string, str, sizeof(str), kCFStringEncodingMacRoman);
        ret = ntohl(inet_addr(str));
    }
    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getAddressFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval)
{
    CFTypeRef		data;
    int 		ok = 0;
    CFArrayRef		array;

    data = copyEntity(store, domain, serviceID, entity);
    if (data) {
        array = CFDictionaryGetValue(data, property);
        if (array && CFArrayGetCount(array)) {
            *outval = CFStringAddrToLong(CFArrayGetValueAtIndex(array, 0));
            ok = 1;
        }
        CFRelease(data);
    }
    return ok;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean my_CFEqual(CFTypeRef obj1, CFTypeRef obj2)
{
    if (obj1 == NULL && obj2 == NULL)
        return true;
    else if (obj1 == NULL || obj2 == NULL)
        return false;
    
    return CFEqual(obj1, obj2);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void my_CFRelease(void *t)
{
    void * * obj = (void * *)t;
    if (obj && *obj) {
	CFRelease(*obj);
	*obj = NULL;
    }
    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void my_close(int fd)
{
    if (fd != -1)
        close(fd);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFTypeRef my_CFRetain(CFTypeRef obj)
{
    if (obj)
        CFRetain(obj);
	return obj;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean isDictionary (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFDictionaryGetTypeID());
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean isArray (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFArrayGetTypeID());
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean isString (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFStringGetTypeID());
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean isNumber (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFNumberGetTypeID());
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean isData (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFDataGetTypeID());
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddNumber(CFMutableDictionaryRef dict, CFStringRef property, u_int32_t nunmber) 
{
   CFNumberRef num;
    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &nunmber);
    if (num) {
        CFDictionaryAddValue(dict, property, num);
        CFRelease(num); 
    } 
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddString(CFMutableDictionaryRef dict, CFStringRef property, char *string) 
{
    CFStringRef str;
    str = CFStringCreateWithCString(NULL, string, kCFStringEncodingUTF8);
    if (str) { 
        CFDictionaryAddValue(dict, property, str);
        CFRelease(str); 
    }
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddNumberFromState(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict) 
{
    u_int32_t 	lval;
    
    if (getNumberFromEntity(store, kSCDynamicStoreDomainState, serviceID, entity, property, &lval)) 
        AddNumber(dict, property, lval);
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddStringFromState(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict) 
{
    CFStringRef	string;
    
    if (string = copyCFStringFromEntity(store, kSCDynamicStoreDomainState, serviceID, entity, property)) {
        CFDictionaryAddValue(dict, property, string);
        CFRelease(string);
    }
}        

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
CFDataRef Serialize(CFPropertyListRef obj, void **data, u_int32_t *dataLen)
{
    CFDataRef           	xml;
    
    xml = CFPropertyListCreateXMLData(NULL, obj);
    if (xml) {
        *data = (void*)CFDataGetBytePtr(xml);
        *dataLen = CFDataGetLength(xml);
    }
    return xml;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
CFPropertyListRef Unserialize(void *data, u_int32_t dataLen)
{
    CFDataRef           	xml;
    CFPropertyListRef	ref = 0;

    xml = CFDataCreate(NULL, data, dataLen);
    if (xml) {
        ref = CFPropertyListCreateFromXMLData(NULL,
                xml,  kCFPropertyListImmutable, NULL);
        CFRelease(xml);
    }

    return ref;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void *my_Allocate(int size) 
{
	void			*addr;
	kern_return_t	status;

	status = vm_allocate(mach_task_self(), (vm_address_t *)&addr, size, TRUE);
	if (status != KERN_SUCCESS) {
		return 0;
	}

	return addr;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void my_Deallocate(void * addr, int size) 
{
	kern_return_t	status;

	if (addr == 0)
		return;

	status = vm_deallocate(mach_task_self(), (vm_address_t)addr, size);
	if (status != KERN_SUCCESS) {
	}

	return;
}

// ----------------------------------------------------------------------------
//	GetIntFromDict
// ----------------------------------------------------------------------------
Boolean GetIntFromDict (CFDictionaryRef dict, CFStringRef property, u_int32_t *outval, u_int32_t defaultval)
{
    CFNumberRef		ref;
	
	ref  = CFDictionaryGetValue(dict, property);
	if (isNumber(ref)
		&&  CFNumberGetValue(ref, kCFNumberSInt32Type, outval))
		return TRUE;
	
	*outval = defaultval;
	return FALSE;
}

// ----------------------------------------------------------------------------
//	GetStrFromDict
// ----------------------------------------------------------------------------
int GetStrFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen, char *defaultval)
{
    CFStringRef		ref;

	ref  = CFDictionaryGetValue(dict, property);
	if (!isString(ref)
		|| !CFStringGetCString(ref, outstr, maxlen, kCFStringEncodingUTF8))
		strncpy(outstr, defaultval, maxlen);
	
	return strlen(outstr);
}

// ----------------------------------------------------------------------------
//	GetStrAddrFromDict
// ----------------------------------------------------------------------------
Boolean GetStrAddrFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen)
{
    CFStringRef		ref;
	in_addr_t               addr;
	
	ref  = CFDictionaryGetValue(dict, property);
	if (isString(ref)
			&& CFStringGetCString(ref, outstr, maxlen, kCFStringEncodingUTF8)) {
					addr = inet_addr(outstr);
					return addr != INADDR_NONE;
	}
	
	return FALSE;
}

// ----------------------------------------------------------------------------
//	GetStrNetFromDict
// ----------------------------------------------------------------------------
Boolean GetStrNetFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen)
{
    CFStringRef		ref;
	in_addr_t               net;

	ref  = CFDictionaryGetValue(dict, property);
	if (isString(ref)
			&& CFStringGetCString(ref, outstr, maxlen, kCFStringEncodingUTF8)) {
			net = inet_network(outstr);
			return net != INADDR_NONE;// && net != 0;
	}
	
	return FALSE;
}



/* -----------------------------------------------------------------------------
publish a dictionnary entry in the cache, given a key
----------------------------------------------------------------------------- */
int publish_keyentry(SCDynamicStoreRef store, CFStringRef key, CFStringRef entry, CFTypeRef value)
{
    CFMutableDictionaryRef	dict;
    CFPropertyListRef		ref;

    if (ref = SCDynamicStoreCopyValue(store, key)) {
        dict = CFDictionaryCreateMutableCopy(0, 0, ref);
        CFRelease(ref);
    }
    else
        dict = CFDictionaryCreateMutable(0, 0, 
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    if (dict == 0)
        return 0;
    
    CFDictionarySetValue(dict,  entry, value);
    if (SCDynamicStoreSetValue(store, key, dict) == 0)
		;//SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: publish_entry SCDSet() failed: %s\n"), SCErrorString(SCError()));
    CFRelease(dict);
    
    return 1;
 }
/* -----------------------------------------------------------------------------
unpublish a dictionnary entry from the cache, given the dict key
----------------------------------------------------------------------------- */
int unpublish_keyentry(SCDynamicStoreRef store, CFStringRef key, CFStringRef entry)
{
    CFMutableDictionaryRef	dict;
    CFPropertyListRef		ref;

    if (ref = SCDynamicStoreCopyValue(store, key)) {
        if (dict = CFDictionaryCreateMutableCopy(0, 0, ref)) {
            CFDictionaryRemoveValue(dict, entry);
            if (SCDynamicStoreSetValue(store, key, dict) == 0)
				;//SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: unpublish_keyentry SCDSet() failed: %s\n"), SCErrorString(SCError()));
            CFRelease(dict);
        }
        CFRelease(ref);
    }
    return 0;
}


/* -----------------------------------------------------------------------------
publish a numerical entry in the cache, given a dictionary
----------------------------------------------------------------------------- */
int publish_dictnumentry(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict, CFStringRef entry, int val)
{
    int			ret = ENOMEM;
    CFNumberRef		num;
    CFStringRef		key;

    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, dict);
    if (key) {
        num = CFNumberCreate(NULL, kCFNumberIntType, &val);
        if (num) {
            ret = publish_keyentry(store, key, entry, num);
            CFRelease(num);
            ret = 0;
        }
        CFRelease(key);
    }
    return ret;
}


/* -----------------------------------------------------------------------------
 unpublish a dictionnary entry from the cache, given the dict name
 ----------------------------------------------------------------------------- */
int unpublish_dictentry(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict, CFStringRef entry)
{
    int			ret = ENOMEM;
    CFStringRef		key;
    
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, dict);
    if (key) {
        ret = unpublish_keyentry(store, key, entry);
        CFRelease(key);
        ret = 0;
    }
    return ret;
}

/* -----------------------------------------------------------------------------
 unpublish a dictionnary entry from the cache, given the dict name
 ----------------------------------------------------------------------------- */
int unpublish_dict(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict)
{
    int			ret = ENOMEM;
    CFStringRef		key;
    
	if (!store)
		return -1;
	
	if (dict)
		key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, dict);
	else 
		key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%@"), kSCDynamicStoreDomainState, kSCCompNetwork, kSCCompService, serviceID);
    if (key) {
        SCDynamicStoreRemoveValue(store, key);
        CFRelease(key);
		ret = 0;
    }

    return ret;
}

/* -----------------------------------------------------------------------------
publish a string entry in the cache, given a dictionary
----------------------------------------------------------------------------- */
int publish_dictstrentry(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict, CFStringRef entry, char *str, int encoding)
{
    
    int			ret = ENOMEM;
    CFStringRef 	ref;
    CFStringRef		key;
    
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, dict);
    if (key) {
        ref = CFStringCreateWithCString(NULL, str, encoding);
        if (ref) {
            ret = publish_keyentry(store, key, entry, ref);
            CFRelease(ref);
            ret = 0;
        }
        CFRelease(key);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
 return f s CFString contains an IP address
 ----------------------------------------------------------------------------- */
int
cfstring_is_ip(CFStringRef str)
{
	char buf[32];
	struct in_addr ip = { 0 };
	CFIndex l;
	int n;
	CFRange range;

	if (!isString(str))
		return 0;

	range = CFRangeMake(0, CFStringGetLength(str));
	n = CFStringGetBytes(str, range, kCFStringEncodingMacRoman,
						 0, FALSE, (UInt8 *)buf, sizeof(buf), &l);
	buf[l] = '\0';

	return inet_aton(buf, &ip);
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
CFStringRef
copyPrimaryService (SCDynamicStoreRef store)
{
    CFDictionaryRef	dict;
    CFStringRef		key;
    CFStringRef		primary = NULL;
    
    if ((key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, 
                                                          kSCDynamicStoreDomainState, 
                                                          kSCEntNetIPv4)) == NULL) {
        return NULL;
    }
    
    dict = SCDynamicStoreCopyValue(store, key);
    CFRelease(key);
    if (isA_CFDictionary(dict)) {
        primary = CFDictionaryGetValue(dict,
                                       kSCDynamicStorePropNetPrimaryService);
        
        primary = isA_CFString(primary);
        if (primary)
            CFRetain(primary);
    }
    if (dict != NULL) {
        CFRelease(dict);
    }
    return primary;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
Boolean UpdatePasswordPrefs(CFStringRef serviceID, CFStringRef interfaceType, SCNetworkInterfacePasswordType passwordType, 
								   CFStringRef passwordEncryptionKey, CFStringRef passwordEncryptionValue, CFStringRef logTitle)
{
	SCPreferencesRef		prefs = NULL;
	SCNetworkServiceRef		service = NULL;
	SCNetworkInterfaceRef	interface = NULL;
	CFMutableDictionaryRef	newConfig = NULL;
	CFDictionaryRef			config;
	Boolean					ok, locked = FALSE, success = FALSE;
	
	prefs = SCPreferencesCreate(NULL, CFSTR("UpdatePassword"), NULL);
	if (prefs == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCPreferencesCreate fails"), logTitle);
        goto done;
	}
	// lock the prefs
	ok = SCPreferencesLock(prefs, TRUE);
	if (!ok) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCPreferencesLock fails"), logTitle);
        goto done;
	}
	
	locked = TRUE;
	
	// get the service
	service = SCNetworkServiceCopy(prefs, serviceID);
	if (service == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCNetworkServiceCopy fails"), logTitle);
        goto done;
	}
	// get the interface associated with the service
	interface = SCNetworkServiceGetInterface(service);
	if ((interface == NULL) || !CFEqual(SCNetworkInterfaceGetInterfaceType(interface), interfaceType)) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: interface not %@"), logTitle, interfaceType);
        goto done;
	}
	
	// remove any current password (from the system keychain)
	if (SCNetworkInterfaceCheckPassword(interface, passwordType)) {
        // if password current associated with this interface
        ok = SCNetworkInterfaceRemovePassword(interface,  passwordType);
        if (!ok) {
			SCLog(TRUE, LOG_ERR, CFSTR("%@: SCNetworkInterfaceRemovePassword fails"), logTitle);
        }
	}
	
	// update passworEncryptionKey
	config = SCNetworkInterfaceGetConfiguration(interface);
	
	if (config != NULL) {
        newConfig = CFDictionaryCreateMutableCopy(NULL, 0, config);
	}
	else {
        newConfig = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}
	
	if (newConfig == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: cannot allocate new interface configuration"), logTitle);
		goto done;
	}
	
	if (passwordEncryptionValue) {
		CFDictionarySetValue(newConfig, passwordEncryptionKey, passwordEncryptionValue);
	} else {
		CFDictionaryRemoveValue( newConfig, passwordEncryptionKey);
	}
	
	ok = SCNetworkInterfaceSetConfiguration(interface, newConfig);
	if ( !ok ) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCNetworkInterfaceSetConfiguration fails"), logTitle);
		goto done;
	}
	
	// commit & apply the changes
	ok = SCPreferencesCommitChanges(prefs);
	if (!ok) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCPreferencesCommitChanges fails"), logTitle);
		goto done;
	}
	ok = SCPreferencesApplyChanges(prefs);
	if (!ok) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCPreferencesApplyChanges fails"), logTitle);
		goto done;
	}
	
	success = TRUE;
	
	done :
	if (newConfig!= NULL) {
		CFRelease(newConfig);
	}
	if (service != NULL) {
		CFRelease(service);
	}
	if (locked) {
		SCPreferencesUnlock(prefs);
	}
	if (prefs != NULL) {
		CFRelease(prefs);
	}
	return success;
}

