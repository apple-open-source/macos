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
#include <sys/ioctl.h>
#include <net/if_dl.h>
#include <net/if_utun.h>
#include <notify.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <servers/bootstrap.h>
#include <mach/task_special_ports.h>
#include "pppcontroller_types.h"
#include "pppcontroller.h"
#include <bsm/libbsm.h>
#include <sys/kern_event.h>
#include <netinet/in_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

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
#if !TARGET_OS_EMBEDDED
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
            CFStringGetCString((CFStringRef)ref, (char*)str, maxlen, kCFStringEncodingUTF8);
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
                CFStringGetCString((CFStringRef)string, (char*)str, maxlen, kCFStringEncodingUTF8);
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
void AddNumber(CFMutableDictionaryRef dict, CFStringRef property, u_int32_t number) 
{
   CFNumberRef num;
    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &number);
    if (num) {
        CFDictionaryAddValue(dict, property, num);
        CFRelease(num); 
    } 
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddNumber64(CFMutableDictionaryRef dict, CFStringRef property, u_int64_t number) 
{
   CFNumberRef num;
    num = CFNumberCreate(NULL, kCFNumberSInt64Type, &number);
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
	char *buf;
	struct in_addr ip = { 0 };
	CFIndex l;
	int n, result;
	CFRange range;

	if (!isString(str) || (l = CFStringGetLength(str)) == 0)
		return 0;
		
	buf = malloc(l+1);
	if (buf == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("Failed to allocate memory"));
		return 0;
	}

	range = CFRangeMake(0, l);
	n = CFStringGetBytes(str, range, kCFStringEncodingMacRoman,
						 0, FALSE, (UInt8 *)buf, l, &l);
	buf[l] = '\0';

	result = inet_aton(buf, &ip);
	free(buf);
	return result;
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


/* -----------------------------------------------------------------------------
set the sa_family field of a struct sockaddr, if it exists.
----------------------------------------------------------------------------- */
#define SET_SA_FAMILY(addr, family)		\
    bzero((char *) &(addr), sizeof(addr));	\
    addr.sa_family = (family); 			\
    addr.sa_len = sizeof(addr);

/* -----------------------------------------------------------------------------
Config the interface MTU
----------------------------------------------------------------------------- */
int set_ifmtu(char *ifname, int mtu)
{
    struct ifreq ifr;
	int ip_sockfd;
	
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0) {
		syslog(LOG_INFO, "sifmtu: cannot create ip socket, %s",
	       strerror(errno));
		return 0;
	}

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    ioctl(ip_sockfd, SIOCSIFMTU, (caddr_t) &ifr);

	close(ip_sockfd);
	return 1;
}

/* -----------------------------------------------------------------------------
Config the interface IP addresses and netmask
----------------------------------------------------------------------------- */
int set_ifaddr(char *ifname, u_int32_t o, u_int32_t h, u_int32_t m)
{
    struct ifaliasreq ifra;
	int ip_sockfd;
	
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0) {
		syslog(LOG_INFO, "sifaddr: cannot create ip socket, %s",
	       strerror(errno));
		return 0;
	}

    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
	
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    
	SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    
	if (m != 0) {
		SET_SA_FAMILY(ifra.ifra_mask, AF_INET);
		((struct sockaddr_in *) &ifra.ifra_mask)->sin_addr.s_addr = m;
    } 
	else
		bzero(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
    
    if (ioctl(ip_sockfd, SIOCAIFADDR, (caddr_t) &ifra) < 0) {
		if (errno != EEXIST) {
			//error("Couldn't set interface address: %m");
			close(ip_sockfd);
			return 0;
		}
		//warning("Couldn't set interface address: Address %I already exists", o);
    }

	close(ip_sockfd);
	return 1;
}

/* -----------------------------------------------------------------------------
Clear the interface IP addresses, and delete routes
 * through the interface if possible
 ----------------------------------------------------------------------------- */
int clear_ifaddr(char *ifname, u_int32_t o, u_int32_t h)
{
    //struct ifreq ifr;
    struct ifaliasreq ifra;
	int ip_sockfd;
	
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0) {
		syslog(LOG_INFO, "cifaddr: cannot create ip socket, %s",
	       strerror(errno));
		return 0;
	}

    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    bzero(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
    if (ioctl(ip_sockfd, SIOCDIFADDR, (caddr_t) &ifra) < 0) {
		if (errno != EADDRNOTAVAIL)
			;//warning("Couldn't delete interface address: %m");
		close(ip_sockfd);
		return 0;
    }
	
	close(ip_sockfd);
    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void
in6_len2mask(struct in6_addr *mask, int len)
{
	int i;

	bzero(mask, sizeof(*mask));
	for (i = 0; i < len / 8; i++)
		mask->s6_addr[i] = 0xff;
	if (len % 8)
		mask->s6_addr[i] = (0xff00 >> (len % 8)) & 0xff;
}

/* -----------------------------------------------------------------------------
mask address according to the mask
----------------------------------------------------------------------------- */
void
in6_maskaddr(struct in6_addr *addr, struct in6_addr *mask)
{
	int i;

	for (i = 0; i < sizeof(struct in6_addr); i++)
		addr->s6_addr[i] &= mask->s6_addr[i];
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void 
in6_addr2net(struct in6_addr *addr, int prefix, struct in6_addr *net) {

    struct in6_addr	mask;
	int i;

	in6_len2mask(&mask, prefix);

	for (i = 0; i < sizeof(mask.s6_addr); i++)	
		(*net).s6_addr[i] = (*addr).s6_addr[i] & (mask).s6_addr[i];

}

/* -----------------------------------------------------------------------------
Config the interface IPv6 addresses
ll_addr must be a 64 bits address.
----------------------------------------------------------------------------- */
int set_ifaddr6 (char *ifname, struct in6_addr *addr, int prefix)
{
    int s;
    struct in6_aliasreq addreq6;
	struct in6_addr		mask;

   s = socket(AF_INET6, SOCK_DGRAM, 0);
    if (s < 0) {
        syslog(LOG_ERR, "set_ifaddr6: can't create IPv6 socket, %s",
	       strerror(errno));
        return 0;
    }

    memset(&addreq6, 0, sizeof(addreq6));
    strlcpy(addreq6.ifra_name, ifname, sizeof(addreq6.ifra_name));

    /* my addr */
    addreq6.ifra_addr.sin6_family = AF_INET6;
    addreq6.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
    memcpy(&addreq6.ifra_addr.sin6_addr, addr, sizeof(struct in6_addr));

    /* prefix mask: 128bit */
    addreq6.ifra_prefixmask.sin6_family = AF_INET6;
    addreq6.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
	 in6_len2mask(&mask, prefix);
    memcpy(&addreq6.ifra_prefixmask.sin6_addr, &mask, sizeof(struct in6_addr));

    /* address lifetime (infty) */
    addreq6.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
    addreq6.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;

    if (ioctl(s, SIOCAIFADDR_IN6, &addreq6) < 0) {
        syslog(LOG_ERR, "set_ifaddr6: can't set IPv6 address, %s",
	       strerror(errno));
        close(s);
        return 0;
    }

    close(s);
    return 1;
}

/* -----------------------------------------------------------------------------
Clear the interface IPv6 addresses
 ----------------------------------------------------------------------------- */
int clear_ifaddr6 (char *ifname, struct in6_addr *addr)
{
    int s;
    struct in6_aliasreq addreq6;

   s = socket(AF_INET6, SOCK_DGRAM, 0);
    if (s < 0) {
        syslog(LOG_ERR, "set_ifaddr6: can't create IPv6 socket, %s",
	       strerror(errno));
        return 0;
    }

    memset(&addreq6, 0, sizeof(addreq6));
    strlcpy(addreq6.ifra_name, ifname, sizeof(addreq6.ifra_name));

    /* my addr */
    addreq6.ifra_addr.sin6_family = AF_INET6;
    addreq6.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
    memcpy(&addreq6.ifra_addr.sin6_addr, addr, sizeof(struct in6_addr));

    if (ioctl(s, SIOCDIFADDR_IN6, &addreq6) < 0) {
        syslog(LOG_ERR, "set_ifaddr6: can't set IPv6 address, %s",
	       strerror(errno));
        close(s);
        return 0;
    }

    close(s);
    return 1;
}


/* ----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
const char *inet_sockaddr_to_p(struct sockaddr *addr, char *buf, int buflen)
{
	void *p; 
		   
	switch (addr->sa_family) {
		case AF_INET:
			p = &((struct sockaddr_in *)addr)->sin_addr;
			break;
		case AF_INET6:
			p = &((struct sockaddr_in6 *)addr)->sin6_addr;
			break;
		default: 
			return NULL;
	}

	return inet_ntop(addr->sa_family, p, buf, buflen);
}

/* ----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int inet_p_to_sockaddr(char *buf, struct sockaddr *addr, int addrlen)
{
    bzero(addr, addrlen);
	
	if (addrlen >= sizeof(struct sockaddr_in)
		&& inet_pton(AF_INET, buf, &((struct sockaddr_in *)addr)->sin_addr)) {
			addr->sa_len = sizeof(struct sockaddr_in);
			addr->sa_family = AF_INET;
			return 1;
	}
	
	if (addrlen >= sizeof(struct sockaddr_in6) 
		&& inet_pton(AF_INET6, buf, &((struct sockaddr_in6 *)addr)->sin6_addr)) {
			addr->sa_len = sizeof(struct sockaddr_in6);
			addr->sa_family = AF_INET6;
			return 1;
	}

	return 0;
}


/* ----------------------------------------------------------------------------
return the default interface name and gateway for a given protocol
----------------------------------------------------------------------------- */
Boolean copyGateway(SCDynamicStoreRef store, u_int8_t family, char *ifname, int ifnamesize, struct sockaddr *gateway, int gatewaysize)
{
	CFDictionaryRef dict;
	CFStringRef key, string;
	Boolean found_interface = FALSE;
	Boolean found_router = FALSE;

 	if (ifname) 
		ifname[0] = 0;
	if (gateway)
		bzero(gateway, gatewaysize);

	if (family != AF_INET && family != AF_INET6)
		return FALSE;

	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, 
		(family == AF_INET) ? kSCEntNetIPv4 : kSCEntNetIPv6);
    if (key) {
      dict = SCDynamicStoreCopyValue(store, key);
		CFRelease(key);
        if (dict) {
		
			if (string = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryInterface)) {
				found_interface = TRUE;
				if (ifname) 
					CFStringGetCString(string, ifname, ifnamesize, kCFStringEncodingUTF8);
			}
			if (string = CFDictionaryGetValue(dict, (family == AF_INET) ? kSCPropNetIPv4Router : kSCPropNetIPv6Router)) {
				char routeraddress[256];
				routeraddress[0] = 0;
				CFStringGetCString(string, (char*)routeraddress, sizeof(routeraddress), kCFStringEncodingUTF8);
				if (routeraddress[0]) {
					struct sockaddr_storage addr;
					if (inet_p_to_sockaddr(routeraddress, (struct sockaddr *)&addr, sizeof(addr))) {
						found_router = TRUE;
						if (gateway && gatewaysize >= addr.ss_len)
							bcopy(&addr, gateway, addr.ss_len);
					}
				}
			}
			CFRelease(dict);
		}
	}
	return (found_interface && found_router);
}

/* ----------------------------------------------------------------------------
return TRUE if there is a default interface and gateway for a given protocol 
----------------------------------------------------------------------------- */
Boolean hasGateway(SCDynamicStoreRef store, u_int8_t family)
{
	return copyGateway(store, family, 0, 0, 0, 0);

}

/* ----------------------------------------------------------------------------
publish ip addresses using configd cache mechanism
use new state information model
----------------------------------------------------------------------------- */
int publish_stateaddr(SCDynamicStoreRef store, CFStringRef serviceID, char *if_name, u_int32_t server, u_int32_t o, 
			u_int32_t h, u_int32_t m, int isprimary)
{
    struct in_addr		addr;
    CFMutableArrayRef		array;
    CFMutableDictionaryRef	ipv4_dict;
    CFStringRef			str;
    CFNumberRef		num;
	SCNetworkServiceRef netservRef;
	
    /* create the IPV4 dictionnary */
    if ((ipv4_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        return 0;
	
	/* set the ip address src and dest arrays */
    if (array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks)) {
        addr.s_addr = o;
        if (str = CFStringCreateWithFormat(0, 0, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr))) {
            CFArrayAppendValue(array, str);
            CFRelease(str);
            CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, array); 
        }
        CFRelease(array);
    }
	
    /* set the router */
    addr.s_addr = h;
    if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr))) {
        CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Router, str);
        CFRelease(str);
    }
	
	num = CFNumberCreate(NULL, kCFNumberIntType, &isprimary);
	if (num) {
        CFDictionarySetValue(ipv4_dict, kSCPropNetOverridePrimary, num);
		CFRelease(num);
	}
	
	if (if_name) {
		if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), if_name)) {
			CFDictionarySetValue(ipv4_dict, kSCPropInterfaceName, str);
			CFRelease(str);
		}
	}
	
    /* set the server */
    addr.s_addr = server;
    if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr))) {
		CFDictionarySetValue(ipv4_dict, CFSTR("ServerAddress"), str);
        CFRelease(str);
    }
	
#if 0
    /* add the network signature */
    if (network_signature) {
		if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), network_signature)) {
			CFDictionarySetValue(ipv4_dict, CFSTR("NetworkSignature"), str);
			CFRelease(str);
		}
	}
#endif
	
    /* update the store now */
    if (str = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, kSCEntNetIPv4)) {
        
        if (SCDynamicStoreSetValue(store, str, ipv4_dict) == 0)
            ;//warning("SCDynamicStoreSetValue IP %s failed: %s\n", ifname, SCErrorString(SCError()));
		
        CFRelease(str);
    }
    
    CFRelease(ipv4_dict);
	
	/* rank service, to prevent it from becoming primary */
	if (!isprimary) {
		netservRef = _SCNetworkServiceCopyActive(store, serviceID);
		if (netservRef) {
			SCNetworkServiceSetPrimaryRank(netservRef, kSCNetworkServicePrimaryRankLast);
			CFRelease(netservRef);
		}
	}
	
    return 1;
}

/* -----------------------------------------------------------------------------
 set dns information
 ----------------------------------------------------------------------------- */
int publish_dns(SCDynamicStoreRef store, CFStringRef serviceID, CFArrayRef dns, CFStringRef domain, CFArrayRef supp_domains)
{    
    int				ret = 0;
    CFMutableDictionaryRef	dict = NULL;
    CFStringRef			key = NULL;
    CFPropertyListRef		ref;
	
    if (store == NULL)
        return 0;
	
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, kSCEntNetDNS);
    if (!key) 
        goto end;
	
    if (ref = SCDynamicStoreCopyValue(store, key)) {
        dict = CFDictionaryCreateMutableCopy(0, 0, ref);
        CFRelease(ref);
    } else
        dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
    if (!dict || (CFGetTypeID(dict) != CFDictionaryGetTypeID()))
        goto end;
	
    CFDictionarySetValue(dict, kSCPropNetDNSServerAddresses, dns);
	
    if (domain)
		CFDictionarySetValue(dict, kSCPropNetDNSDomainName, domain);
	
    if (supp_domains)
		CFDictionarySetValue(dict, kSCPropNetDNSSupplementalMatchDomains, supp_domains);
	
	/* warn lookupd of upcoming change */
	notify_post("com.apple.system.dns.delay");
	
    if (SCDynamicStoreSetValue(store, key, dict))
        ret = 1;
    else
		;// warning("SCDynamicStoreSetValue DNS/WINS %s failed: %s\n", ifname, SCErrorString(SCError()));
	
end:
    if (key)  
        CFRelease(key);
    if (dict)  
        CFRelease(dict);
    return ret;
	
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
Boolean equal_address(struct sockaddr *addr1, struct sockaddr *addr2)
{
				
	if (addr1->sa_family != addr2->sa_family)
		return FALSE;
	
	if (addr1->sa_family == AF_INET) {
		struct sockaddr_in *addr1_in = (struct sockaddr_in *)addr1;
		struct sockaddr_in *addr2_in = (struct sockaddr_in *)addr2;
		return (addr1_in->sin_addr.s_addr == addr2_in->sin_addr.s_addr);
	}
	
	if (addr1->sa_family == AF_INET6) {
		struct sockaddr_in6 *addr1_in6 = (struct sockaddr_in6 *)addr1;
		struct sockaddr_in6 *addr2_in6 = (struct sockaddr_in6 *)addr2;
		return (!bcmp(&addr1_in6->sin6_addr, &addr2_in6->sin6_addr, sizeof(struct in6_addr)));
	}
	
	return FALSE;
}

/* -----------------------------------------------------------------------------
    add/remove a route via a gateway
----------------------------------------------------------------------------- */
int
route_gateway(int cmd, struct sockaddr *dest, struct sockaddr *mask, struct sockaddr *gateway, int use_gway_flag, int use_blackhole_flag)
{
    int 			len;
    int 			rtm_seq = 0;

    struct rtmsg_in4 {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
    };
    struct rtmsg_in6 {
	struct rt_msghdr	hdr;
	struct sockaddr_in6	dst;
	struct sockaddr_in6	gway;
	struct sockaddr_in6	mask;
    };

    int 			sockfd = -1;
    struct rtmsg_in6 rtmsg; // use rtmsg_in6 since it is the bigger one;
	    
	if (dest == NULL || (dest->sa_family != AF_INET
		&& dest->sa_family != AF_INET6))
		return -1;

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, PF_ROUTE)) < 0) {
	syslog(LOG_INFO, "route_gateway: open routing socket failed, %s",
	       strerror(errno));
	return (-1);
    }

    memset(&rtmsg, 0, sizeof(rtmsg));

	// fill in header, which is common to IPv4 and IPv6
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC;
    if (use_gway_flag)
        rtmsg.hdr.rtm_flags |= RTF_GATEWAY;
    if (use_blackhole_flag)
        rtmsg.hdr.rtm_flags |= RTF_BLACKHOLE;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_NETMASK | RTA_GATEWAY;

	// then fill in address specific portion
	if (dest->sa_family == AF_INET) {
		struct rtmsg_in4 *rtmsg4 = (struct rtmsg_in4 *)&rtmsg; 

		bcopy(dest, &rtmsg4->dst, sizeof(rtmsg4->dst));
		if (gateway)
			bcopy(gateway, &rtmsg4->gway, sizeof(rtmsg4->gway));
		if (mask)
			bcopy(mask, &rtmsg4->mask, sizeof(rtmsg4->mask));

		len = sizeof(struct rtmsg_in4);
	}
	else {
		struct rtmsg_in6 *rtmsg6 = (struct rtmsg_in6 *)&rtmsg;
		
		bcopy(dest, &rtmsg6->dst, sizeof(rtmsg6->dst));
		if (gateway)
			bcopy(gateway, &rtmsg6->gway, sizeof(rtmsg6->gway));
		if (mask)
			bcopy(mask, &rtmsg6->mask, sizeof(rtmsg6->mask));

		len = sizeof(struct rtmsg_in6);
	}

	rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
		syslog(LOG_ERR, "route_gateway: write routing socket failed, %s", strerror(errno));

#if 0
		/* print routing message for debugging */
		char buf[256];
		syslog(LOG_ERR, "-------");
		struct rtmsg_in4 *rtmsg4 = (struct rtmsg_in4 *)&rtmsg; 
		inet_sockaddr_to_p(dest->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->dst : (struct sockaddr *)&rtmsg.dst, buf, sizeof(buf));
		syslog(LOG_ERR, "route_gateway: rtmsg.dst = %s", buf);
		inet_sockaddr_to_p(dest->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->gway : (struct sockaddr *)&rtmsg.gway, buf, sizeof(buf));
		syslog(LOG_ERR, "route_gateway: rtmsg.gway = %s", buf);
		inet_sockaddr_to_p(dest->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->mask : (struct sockaddr *)&rtmsg.mask, buf, sizeof(buf));
		syslog(LOG_ERR, "route_gateway: rtmsg.mask = %s", buf);
		syslog(LOG_ERR, "-------");
#endif

		close(sockfd);
		return (-1);
    }

    close(sockfd);
    return (0);
}

/* -----------------------------------------------------------------------------
add/remove a host route
----------------------------------------------------------------------------- */
boolean_t
set_host_gateway(int cmd, struct sockaddr *host, struct sockaddr *gateway, char *ifname, int isnet)
{
    int 			len;
    int 			rtm_seq = 0;
    struct rtmsg_in4 {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
	struct sockaddr_dl	link;
    };
    struct rtmsg_in6 {
	struct rt_msghdr	hdr;
	struct sockaddr_in6	dst;
	struct sockaddr_in6	gway;
	struct sockaddr_in6	mask;
	struct sockaddr_dl	link;
    };

    int 			sockfd = -1;
    struct rtmsg_in6 rtmsg; // use rtmsg_in6 since it is the bigger one;
	struct sockaddr_dl *link;
	
	if (host == NULL || (host->sa_family != AF_INET
		&& host->sa_family != AF_INET6))
		return FALSE;
	
    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, PF_ROUTE)) < 0) {
	syslog(LOG_INFO, "host_gateway: open routing socket failed, %s",
	       strerror(errno));
	return (FALSE);
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC;
    if (isnet)
        rtmsg.hdr.rtm_flags |= RTF_CLONING;
    else 
        rtmsg.hdr.rtm_flags |= RTF_HOST;
    if (gateway)
        rtmsg.hdr.rtm_flags |= RTF_GATEWAY;
    
	rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_NETMASK | RTA_GATEWAY;
	
	if (host->sa_family == AF_INET) {
		struct rtmsg_in4 *rtmsg4 = (struct rtmsg_in4 *)&rtmsg; 

		bcopy(host, &rtmsg4->dst, sizeof(rtmsg4->dst));

		if (gateway) 
			bcopy(gateway, &rtmsg4->gway, sizeof(rtmsg4->gway));
		
		rtmsg4->mask.sin_len = sizeof(rtmsg4->mask);
		rtmsg4->mask.sin_family = AF_INET;
		rtmsg4->mask.sin_addr.s_addr = 0xFFFFFFFF;

		len = sizeof(struct rtmsg_in4);
		link = &rtmsg4->link;
	}
	else {
		struct rtmsg_in6 *rtmsg6 = (struct rtmsg_in6 *)&rtmsg;
		
		bcopy(host, &rtmsg6->dst, sizeof(rtmsg6->dst));
		
		if (gateway) 
			bcopy(gateway, &rtmsg6->gway, sizeof(rtmsg6->gway));

		rtmsg6->mask.sin6_len = sizeof(rtmsg6->mask);
		rtmsg6->mask.sin6_family = AF_INET6;
		rtmsg6->mask.sin6_addr.__u6_addr.__u6_addr32[0] = 0xFFFFFFFF;
		rtmsg6->mask.sin6_addr.__u6_addr.__u6_addr32[1] = 0xFFFFFFFF;
		rtmsg6->mask.sin6_addr.__u6_addr.__u6_addr32[2] = 0xFFFFFFFF;
		rtmsg6->mask.sin6_addr.__u6_addr.__u6_addr32[3] = 0xFFFFFFFF;

		len = sizeof(struct rtmsg_in6);
		link = &rtmsg6->link;
	}
	
    if (ifname) {
		link->sdl_len = sizeof(rtmsg.link);
		link->sdl_family = AF_LINK;
		link->sdl_nlen = MIN(strlen(ifname), sizeof(link->sdl_data));
		rtmsg.hdr.rtm_addrs |= RTA_IFP;
		bcopy(ifname, link->sdl_data, link->sdl_nlen);
    }
    else {
		/* no link information */
		len -= sizeof(rtmsg.link);
    }
	
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
		syslog(LOG_ERR, "host_gateway: write routing socket failed, command %d, %s", cmd, strerror(errno));

#if 0
		/* print routing message for debugging */
		char buf[256];
		syslog(LOG_ERR, "********");
		struct rtmsg_in4 *rtmsg4 = (struct rtmsg_in4 *)&rtmsg; 
		syslog(LOG_ERR, "host_gateway: dest->sa_family = %d rtmsg.hdr.rtm_msglen = %d", host->sa_family, rtmsg.hdr.rtm_msglen);
		inet_sockaddr_to_p(host->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->dst : (struct sockaddr *)&rtmsg.dst, buf, sizeof(buf));
		syslog(LOG_ERR, "host_gateway: rtmsg.dst = %s", buf);
		inet_sockaddr_to_p(host->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->gway : (struct sockaddr *)&rtmsg.gway, buf, sizeof(buf));
		syslog(LOG_ERR, "host_gateway: rtmsg.gway = %s", buf);
		inet_sockaddr_to_p(host->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->mask : (struct sockaddr *)&rtmsg.mask, buf, sizeof(buf));
		syslog(LOG_ERR, "host_gateway: rtmsg.mask = %s", buf);
		syslog(LOG_ERR, "********");
#endif
		close(sockfd);
		return (FALSE);
    }

    close(sockfd);
    return (TRUE);
}

/* ----------------------------------------------------------------------------
 publish proxies using configd cache mechanism
 ----------------------------------------------------------------------------- */
int publish_proxies(SCDynamicStoreRef store, CFStringRef serviceID, int autodetect, CFStringRef server, int port, int bypasslocal, CFStringRef exceptionlist)
{
	int				val, ret = -1;
    CFStringRef		cfstr = NULL;
    CFArrayRef		cfarray;
    CFNumberRef		cfnum,  cfone = NULL;
    CFMutableDictionaryRef	proxies_dict = NULL;
		
    if ((proxies_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        goto fail;

	val = 1;
	cfone = CFNumberCreate(NULL, kCFNumberIntType, &val);
	if (cfone == NULL)
		goto fail;
	
	if (autodetect) {
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesProxyAutoDiscoveryEnable, cfone);
	}
	else {
		// MUST have server
		if (server == NULL)
			goto fail;
		
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesFTPEnable, cfone);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPEnable, cfone);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPSEnable, cfone);

		cfnum = CFNumberCreate(NULL, kCFNumberIntType, &port);
		if (cfnum == NULL)
			goto fail;
		
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesFTPPort, cfnum);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPPort, cfnum);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPSPort, cfnum);
		CFRelease(cfnum);
		
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesFTPProxy, server);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPProxy, server);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPSProxy, server);

		cfnum = CFNumberCreate(NULL, kCFNumberIntType, &bypasslocal);
		if (cfnum == NULL)
			goto fail;
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesExcludeSimpleHostnames, cfnum);
		CFRelease(cfnum);
		
		if (exceptionlist) {
			cfarray = CFStringCreateArrayBySeparatingStrings(NULL, exceptionlist, CFSTR(";"));
			if (cfarray) {
				CFDictionarySetValue(proxies_dict, kSCPropNetProxiesExceptionsList, cfarray);
				CFRelease(cfarray);
			}
		}
	}
		
    /* update the store now */
    cfstr = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, kSCEntNetProxies);
    if (cfstr == NULL)
		goto fail;

	if (SCDynamicStoreSetValue(store, cfstr, proxies_dict) == 0) {
		//warning("SCDynamicStoreSetValue IP %s failed: %s\n", ifname, SCErrorString(SCError()));
		goto fail;
	}
		
	ret = 0;	
		
fail:
	
    my_CFRelease(&cfone);
	my_CFRelease(&cfstr);
    my_CFRelease(&proxies_dict);
    return ret;
}

/* -----------------------------------------------------------------------------
 Create the new tun interface, and return the socket
 ----------------------------------------------------------------------------- */
int create_tun_interface(char *name, int name_max_len, int *index, int flags, int ext_stats)
{

	struct ctl_info kernctl_info;
	struct sockaddr_ctl kernctl_addr;
	u_int32_t optlen;
	int tunsock = -1;

	tunsock = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	if (tunsock == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: cannot create kernel control socket (errno = %d)"), errno);
		goto fail;
	}
		
	bzero(&kernctl_info, sizeof(kernctl_info));
    strlcpy(kernctl_info.ctl_name, UTUN_CONTROL_NAME, sizeof(kernctl_info.ctl_name));
	if (ioctl(tunsock, CTLIOCGINFO, &kernctl_info)) {
		SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: ioctl failed on kernel control socket (errno = %d)"), errno);
		goto fail;
	}
	
	bzero(&kernctl_addr, sizeof(kernctl_addr)); // sets the sc_unit field to 0
	kernctl_addr.sc_len = sizeof(kernctl_addr);
	kernctl_addr.sc_family = AF_SYSTEM;
	kernctl_addr.ss_sysaddr = AF_SYS_CONTROL;
	kernctl_addr.sc_id = kernctl_info.ctl_id;
	kernctl_addr.sc_unit = 0; // we will get the unit number from getpeername
	if (connect(tunsock, (struct sockaddr *)&kernctl_addr, sizeof(kernctl_addr))) {
		SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: connect failed on kernel control socket (errno = %d)"), errno);
		goto fail;
	}

	optlen = name_max_len;
	if (getsockopt(tunsock, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, name, &optlen)) {
		SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: getsockopt ifname failed on kernel control socket (errno = %d)"), errno);
		goto fail;	
	}

	*index = if_nametoindex(name);

	if (flags) {
		int optflags = 0;
		optlen = sizeof(u_int32_t);
		if (getsockopt(tunsock, SYSPROTO_CONTROL, UTUN_OPT_FLAGS, &optflags, &optlen)) {
			SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: getsockopt flags failed on kernel control socket (errno = %d)"), errno);
			goto fail;	
		}
		 
		optflags |= (UTUN_FLAGS_NO_INPUT + UTUN_FLAGS_NO_OUTPUT);
		optlen = sizeof(u_int32_t);
		if (setsockopt(tunsock, SYSPROTO_CONTROL, UTUN_OPT_FLAGS, &optflags, optlen)) {
			SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: setsockopt flags failed on kernel control socket (errno = %d)"), errno);
			goto fail;	
		}
	}
	
	if (ext_stats) {
		int optval = 1;
		if (setsockopt(tunsock, SYSPROTO_CONTROL, UTUN_OPT_EXT_IFDATA_STATS, &optval, sizeof(optval))) {
			SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: setsockopt externat stats failed on kernel control socket (errno = %d)"), errno);
			goto fail;	
		}
	}

	return tunsock;
	
fail:
	my_close(tunsock);
	return -1;
	
}


/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int setup_bootstrap_port()
{    
	mach_port_t			server, bootstrap = 0;
	int					result;
	kern_return_t		status;
	audit_token_t		audit_token;
	uid_t               euid;

	status = bootstrap_look_up(bootstrap_port, PPPCONTROLLER_SERVER, &server);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			/* service currently registered, "a good thing" (tm) */
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* service not currently registered, try again later */
			return -1;
		default :
			return -1;
	}

	/* open a new session with the server */
	status = pppcontroller_bootstrap(server, &bootstrap, &result, &audit_token);
	mach_port_deallocate(mach_task_self(), server);

	if (status != KERN_SUCCESS) {
		printf("setup_bootstrap_port error: %s\n", mach_error_string(status));
		if (status != MACH_SEND_INVALID_DEST)
			printf("setup_bootstrap_port error NOT MACH_SEND_INVALID_DEST: %s\n", mach_error_string(status));
		return -1;
	}

	audit_token_to_au32(audit_token,
				NULL,			// auidp
				&euid,			// euid
				NULL,			// egid
				NULL,			// ruid
				NULL,			// rgid
				NULL,			// pid
				NULL,			// asid
				NULL);			// tid
	if (euid != 0) {
		printf("setup_bootstrap_port cannot authenticate bootstrap port from controller\n");
		return -1;
	}

	if (bootstrap) {
		task_set_bootstrap_port(mach_task_self(), bootstrap);
		mach_port_deallocate(mach_task_self(), bootstrap);
	}

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int event_create_socket(void * ctxt, int *eventfd, CFSocketRef *eventref, CFSocketCallBack callout, Boolean anysubclass)
{
    CFRunLoopSourceRef	rls;
    CFSocketContext	context = { 0, ctxt, NULL, NULL, NULL };
    struct kev_request	kev_req;
        
	*eventfd = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
	if (*eventfd < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("event_create_socket cannot create event socket (errno = %d) "), errno);
		goto fail;
	}

	kev_req.vendor_code = KEV_VENDOR_APPLE;
	kev_req.kev_class = KEV_NETWORK_CLASS;
	kev_req.kev_subclass = anysubclass ? KEV_ANY_SUBCLASS : KEV_INET_SUBCLASS;
	ioctl(*eventfd, SIOCSKEVFILT, &kev_req);
        
    if ((*eventref = CFSocketCreateWithNative(NULL, *eventfd, 
                    kCFSocketReadCallBack, callout, &context)) == 0) {
        goto fail;
    }
    if ((rls = CFSocketCreateRunLoopSource(NULL, *eventref, 0)) == 0)
        goto fail;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    return 0;
    
fail:
	if (*eventref) {
		CFSocketInvalidate(*eventref);
		CFRelease(*eventref);
	}
	else 
		if (*eventfd >= 0) {
			close(*eventfd);
	}
	*eventref = 0;
	*eventfd = -1;

    return -1;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
typedef struct exec_callback_args {
	CFRunLoopRef            rl;
	CFRunLoopSourceRef      rls;
	CFRunLoopSourceContext  rlc;
	pid_t                   pid;
	int                     status;
	struct rusage	        rusage;
	SCDPluginExecCallBack   callback;
	void                   *callbackContext;
} exec_callback_args_t;

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static 
void exec_callback(pid_t pid, int status, struct rusage *rusage, void *context)
{
	if (isA_CFData(context)) {
		exec_callback_args_t *args = (__typeof__(args))CFDataGetMutableBytePtr((CFMutableDataRef)context);
		args->pid = pid;
		args->status = status;
		bcopy(rusage, &args->rusage, sizeof(args->rusage));
		// args->context already contains the service
		CFRunLoopSourceSignal(args->rls);
		CFRunLoopWakeUp(args->rl);
	}
}

static void
SCNCPluginExecCallbackRunLoopSource (void *info)
{
	if (isA_CFData(info)) {
		exec_callback_args_t *args = (__typeof__(args))CFDataGetMutableBytePtr((CFMutableDataRef)info);
		if (args->callback) {
			args->callback(args->pid, args->status, &args->rusage, args->callbackContext);
		}
		CFRunLoopSourceInvalidate(args->rls);
		CFRelease(args->rls);
		CFRelease(args->rl);
		CFRelease((CFMutableDataRef)info); // release (was allocated in SCNCPluginExecCallbackRunLoopSourceInit)
	}
}

CFMutableDataRef
SCNCPluginExecCallbackRunLoopSourceInit (CFRunLoopRef           runloop,
										 SCDPluginExecCallBack  callback,
										 void	               *callbackContext)
{
	CFMutableDataRef      dataRef; // to be used as SCNCPluginExecCallbackRunLoopSource's info
	UInt8                *dataPtr;
	exec_callback_args_t  args;

	// create dataref and fill it with runloop args
	dataRef = CFDataCreateMutable(NULL, sizeof(args));
	if (dataRef == NULL) {
		return NULL;
	}
	CFDataSetLength(dataRef, sizeof(args));
	dataPtr = CFDataGetMutableBytePtr(dataRef);

	bzero(&args, sizeof(args));
	args.rlc.info = dataRef; // use as SCNCPluginExecCallbackRunLoopSource's info
	args.rlc.perform = SCNCPluginExecCallbackRunLoopSource;
	args.rls = CFRunLoopSourceCreate(NULL, 0, &args.rlc);
	if (!args.rls){
		CFRelease(dataRef);
		return NULL;
	}
	args.callback = callback;
	args.callbackContext = callbackContext;
	if (!runloop) {
		args.rl = CFRunLoopGetCurrent();
	} else {
		args.rl = runloop;
	}
	CFRetain(args.rl);
	CFRunLoopAddSource(args.rl, args.rls, kCFRunLoopDefaultMode);
	bcopy(&args, dataPtr, sizeof(args));
	return dataRef; // to be used as exec_callback's context
}

pid_t
SCNCPluginExecCommand (CFRunLoopRef           runloop,
					   SCDPluginExecCallBack  callback,
					   void        	         *callbackContext,
					   uid_t                  uid,
					   gid_t                  gid,
					   const char            *path,
					   char * const           argv[])
{
	pid_t            rc;
	CFMutableDataRef exec_callback_context;

	exec_callback_context = SCNCPluginExecCallbackRunLoopSourceInit(runloop, callback, callbackContext);
	if (!exec_callback_context){
		SCLog(TRUE, LOG_ERR, CFSTR("SCNC: failed to initialize plugin exec_callback's runloop source"));
		return -1;
	}

	rc = _SCDPluginExecCommand(exec_callback, 
							   exec_callback_context, 
							   uid, 
							   gid, 
							   path, 
							   argv);
	return rc;
}

pid_t
SCNCPluginExecCommand2 (CFRunLoopRef           runloop,
						SCDPluginExecCallBack  callback,
						void                  *callbackContext,
						uid_t                  uid,
						gid_t                  gid,
						const char            *path,
						char * const           argv[],
						SCDPluginExecSetup     setup,
						void                  *setupContext)
{
	pid_t            rc;
	CFMutableDataRef exec_callback_context;

	exec_callback_context = SCNCPluginExecCallbackRunLoopSourceInit(runloop, callback, callbackContext);
	if (!exec_callback_context){
		SCLog(TRUE, LOG_ERR, CFSTR("SCNC: failed to initialize plugin exec_callback's runloop source"));
		return -1;
	}

	rc = _SCDPluginExecCommand2(exec_callback,
								exec_callback_context,
								uid,
								gid, 
								path, 
								argv, 
								setup, 
								setupContext);
	return rc;
}

void
applyEnvironmentVariablesApplierFunction (const void *key, const void *value, void *context)
{
	CFRange range;

	if (isA_CFString(key)) {
		char key_buf[256];
		char value_buf[256];

		range.location = 0;
		range.length = CFStringGetLength((CFStringRef)key);
		if (range.length <= 0 ||
			range.length >= sizeof(key_buf) ||
			CFStringGetBytes((CFStringRef)key, range, kCFStringEncodingUTF8, 0, false, (UInt8 *)key_buf, sizeof(key_buf), NULL) <= 0) {
			SCLog(TRUE, LOG_ERR, CFSTR("invalid EnvironmentVariables key %@, value %@"), key, value);
			return;
		}
		key_buf[range.length] = '\0';

		unsetenv((const char *)key_buf);

		if (isA_CFString(value)) {
			range.location = 0;
			range.length = CFStringGetLength((CFStringRef)value);
			if (range.length <= 0 ||
				range.length >= sizeof(value_buf) ||
				CFStringGetBytes((CFStringRef)value, range, kCFStringEncodingUTF8, 0, false, (UInt8 *)value_buf, sizeof(value_buf), NULL) <= 0) {
				SCLog(TRUE, LOG_ERR, CFSTR("invalid EnvironmentVariables key %@, value %@"), key, value);
				return;
			}
			value_buf[range.length] = '\0';
		} else if (isA_CFNumber(value)) {
			int64_t number = 0;
			if (CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &number)) {
				snprintf(value_buf,sizeof(value_buf), "%lld", number);
			} else {
				SCLog(TRUE, LOG_ERR, CFSTR("invalid EnvironmentVariables key %@, value %@"), key, value);
				return;
			}
		} else if (isA_CFBoolean(value)) {
			snprintf(value_buf, sizeof(value_buf), "%s", CFBooleanGetValue((CFBooleanRef)value) ? "Yes" : "No");
		} else {
			SCLog(TRUE, LOG_ERR, CFSTR("invalid EnvironmentVariables key %@, value %@"), key, value);
			return;
		}

		setenv(key_buf, value_buf, TRUE);
	} else {
		SCLog(TRUE, LOG_ERR, CFSTR("invalid EnvironmentVariables key"));
	}
}

CFDictionaryRef
collectEnvironmentVariables (SCDynamicStoreRef storeRef, CFStringRef serviceID)
{
	if (!storeRef) {
		SCLog(TRUE, LOG_ERR, CFSTR("invalid DynamicStore passed to %s"), __FUNCTION__);
		return NULL;
	}

	if (!serviceID) {
		SCLog(TRUE, LOG_ERR, CFSTR("invalid serviceID passed to %s"), __FUNCTION__);
		return NULL;
	}

	return copyEntity(storeRef, kSCDynamicStoreDomainSetup, serviceID, CFSTR("EnvironmentVariables"));
}

void
applyEnvironmentVariables (CFDictionaryRef envVarDict)
{
	if (!envVarDict) {
		return;
	} else if (isA_CFDictionary(envVarDict) &&
			   CFDictionaryGetCount(envVarDict) > 0) {
		CFDictionaryApplyFunction(envVarDict, applyEnvironmentVariablesApplierFunction, NULL);
	} else {
		SCLog(TRUE, LOG_ERR, CFSTR("empty or invalid EnvironmentVariables dictionary"));
	}
}
