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
#ifdef DEBUG
#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#endif

#include "PPPControllerPriv.h"
#include "ppp_msg.h"
#include "../Family/if_ppplink.h"
#include "ppp_client.h"
#include "ppp_manager.h"
#include "ppp_utils.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */

#define CREATESERVICESETUP(a)	SCDynamicStoreKeyCreateNetworkServiceEntity(0, \
                    kSCDynamicStoreDomainSetup, kSCCompAnyRegex, a)
#define CREATESERVICESTATE(a)	SCDynamicStoreKeyCreateNetworkServiceEntity(0, \
                    kSCDynamicStoreDomainState, kSCCompAnyRegex, a)
#define CREATEPREFIXSETUP()	SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/"), \
                    kSCDynamicStoreDomainSetup, kSCCompNetwork, kSCCompService)
#define CREATEPREFIXSTATE()	SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/"), \
                    kSCDynamicStoreDomainState, kSCCompNetwork, kSCCompService)
#define CREATEGLOBALSETUP(a)	SCDynamicStoreKeyCreateNetworkGlobalEntity(0, \
                    kSCDynamicStoreDomainSetup, a)

/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

extern SCDynamicStoreRef	gDynamicStore;

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
CFDictionaryRef copyService(CFStringRef domain, CFStringRef serviceID)
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
    	kSCEntNetSMB,
        kSCEntNetDNS,
        kSCEntNetL2TP,
        kSCEntNetPPTP,
        kSCEntNetIPSec,
        NULL,
    };

    key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%@"), domain, kSCCompNetwork, kSCCompService, serviceID);
    if (key == 0)
        goto fail;
        
    data = SCDynamicStoreCopyValue(gDynamicStore, key);
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
        data = copyEntity(domain, serviceID, copy[i]);
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
CFDictionaryRef copyEntity(CFStringRef domain, CFStringRef serviceID, CFStringRef entity)
{
    CFTypeRef		data = NULL;
    CFStringRef		key;

    if (serviceID)
        key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, domain, serviceID, entity);
    else
        key = SCDynamicStoreKeyCreateNetworkGlobalEntity(0, domain, entity);

    if (key) {
        data = SCDynamicStoreCopyValue(gDynamicStore, key);
        CFRelease(key);
    }
    return data;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int existEntity(CFStringRef domain, CFStringRef serviceID, CFStringRef entity)
{
    CFTypeRef		data;

    data = copyEntity(domain, serviceID, entity);
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
int getNumberFromEntity(CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval)
{
    CFTypeRef		data;
    int 		ok = 0;

    if (data = copyEntity(domain, serviceID, entity)) {
        ok = getNumber(data, property, outval);
        CFRelease(data);
    }
    return ok;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getStringFromEntity(CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_char *str, u_int16_t maxlen)
{
    CFTypeRef		data;
    int 		ok = 0;

    data = copyEntity(domain, serviceID, entity);
    if (data) {
        ok = getString(data, property, str, maxlen);
        CFRelease(data);
    }
    return ok;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFStringRef copyCFStringFromEntity(CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property)
{
    CFTypeRef		data;
    CFStringRef		string, ret = 0;

    data = copyEntity(domain, serviceID, entity);
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
    u_char 	str[100];
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
int getAddressFromEntity(CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval)
{
    CFTypeRef		data;
    int 		ok = 0;
    CFArrayRef		array;

    data = copyEntity(domain, serviceID, entity);
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
void my_CFRelease(CFTypeRef obj)
{
    if (obj)
        CFRelease(obj);
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
void my_CFRetain(CFTypeRef obj)
{
    if (obj)
        CFRetain(obj);
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
void AddNumberFromState(CFStringRef serviceID, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict) 
{
    u_int32_t 	lval;
    
    if (getNumberFromEntity(kSCDynamicStoreDomainState, serviceID, entity, property, &lval)) 
        AddNumber(dict, property, lval);
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddStringFromState(CFStringRef serviceID, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict) 
{
    CFStringRef	string;
    
    if (string = copyCFStringFromEntity(kSCDynamicStoreDomainState, serviceID, entity, property)) {
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
    CFStringRef         	xmlError;
    CFPropertyListRef	ref = 0;

    xml = CFDataCreate(NULL, data, dataLen);
    if (xml) {
        ref = CFPropertyListCreateFromXMLData(NULL,
                xml,  kCFPropertyListImmutable, &xmlError);
        CFRelease(xml);
    }

    return ref;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFStringRef CopyUserLocalizedString(CFBundleRef bundle,
    CFStringRef key, CFStringRef value, CFArrayRef userLanguages) 
{
    CFStringRef 	result = NULL, errStr= NULL;
    CFDictionaryRef 	stringTable;
    CFDataRef 		tableData;
    SInt32 		errCode;
    CFURLRef 		tableURL;
    CFArrayRef		locArray, prefArray;

    if (userLanguages == NULL)
        return CFBundleCopyLocalizedString(bundle, key, value, NULL);

    if (key == NULL)
        return (value ? CFRetain(value) : CFRetain(CFSTR("")));

    locArray = CFBundleCopyBundleLocalizations(bundle);
    if (locArray) {
        prefArray = CFBundleCopyLocalizationsForPreferences(locArray, userLanguages);
        if (prefArray) {
            if (CFArrayGetCount(prefArray)) {
                tableURL = CFBundleCopyResourceURLForLocalization(bundle, CFSTR("Localizable"), CFSTR("strings"), NULL, 
                                    CFArrayGetValueAtIndex(prefArray, 0));
                if (tableURL) {
                    if (CFURLCreateDataAndPropertiesFromResource(NULL, tableURL, &tableData, NULL, NULL, &errCode)) {
                        stringTable = CFPropertyListCreateFromXMLData(NULL, tableData, kCFPropertyListImmutable, &errStr);
                        if (errStr)
                            CFRelease(errStr);
                        if (stringTable) {
                            result = CFDictionaryGetValue(stringTable, key);
                            if (result)
                                CFRetain(result);
                            CFRelease(stringTable);
                        }
                        CFRelease(tableData);
                    }
                    CFRelease(tableURL);
                }
            }
            CFRelease(prefArray);
        }
        CFRelease(locArray);
    }
        
    if (result == NULL)
        result = (value && !CFEqual(value, CFSTR(""))) ?  CFRetain(value) : CFRetain(key);
    
    return result;
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
