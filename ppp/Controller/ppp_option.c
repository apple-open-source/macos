/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <CoreFoundation/CoreFoundation.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef	USE_SYSTEMCONFIGURATION_PUBLIC_APIS
#include <SystemConfiguration/SystemConfiguration.h>
#else	/* USE_SYSTEMCONFIGURATION_PUBLIC_APIS */
#include <SystemConfiguration/v1Compatibility.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#endif	/* USE_SYSTEMCONFIGURATION_PUBLIC_APIS */

#include "ppp_msg.h"
#include "../Family/if_ppplink.h"
#include "ppp_client.h"
#include "ppp_option.h"
#include "ppp_manager.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */

static __inline__ CFTypeRef isA_CFType(CFTypeRef obj, CFTypeID type);
static __inline__ CFTypeRef isA_CFDictionary(CFTypeRef obj);
static __inline__ void my_CFRelease(CFTypeRef obj);
static CFStringRef parse_component(CFStringRef key, CFStringRef prefix);
static u_int32_t CFStringAddrToLong(CFStringRef string);
static boolean_t cache_notifier(SCDSessionRef session, void * arg);

static void reorder_services(SCDSessionRef session);
static int process_servicestate(SCDSessionRef session, CFStringRef serviceID);
static int process_servicesetup(SCDSessionRef session, CFStringRef serviceID);


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

char gLoggedInUser[32];

/* -----------------------------------------------------------------------------
install the cache notification and read the initilal configured interfaces
must be called when session cache has been setup
----------------------------------------------------------------------------- */
void options_init_all(SCDSessionRef session)
{
    SCDStatus		status;
    CFStringRef         serviceID, key, prefix;
    CFArrayRef		services;
    u_int32_t 		i;

    gLoggedInUser[0] = 0;
    SCDConsoleUserGet(gLoggedInUser, sizeof(gLoggedInUser), 0, 0);

    /* install the notifier for the cache/setup changes */
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup, kSCCompAnyRegex, kSCEntNetPPP);
    status = SCDNotifierAdd(session, key, kSCDRegexKey);
    CFRelease(key);
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup, kSCCompAnyRegex, kSCEntNetModem);
    status = SCDNotifierAdd(session, key, kSCDRegexKey);
    CFRelease(key);
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup, kSCCompAnyRegex, kSCEntNetInterface);
    status = SCDNotifierAdd(session, key, kSCDRegexKey);
    CFRelease(key);
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup, kSCCompAnyRegex, kSCEntNetIPv4);
    status = SCDNotifierAdd(session, key, kSCDRegexKey);
    CFRelease(key);
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup, kSCCompAnyRegex, kSCEntNetDNS);
    status = SCDNotifierAdd(session, key, kSCDRegexKey);
    CFRelease(key);
    key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, kSCEntNetIPv4);
    status = SCDNotifierAdd(session, key, 0);
    CFRelease(key);
    
    /* install the notifier for user login/logout */
    key = SCDKeyCreateConsoleUser();
    status = SCDNotifierAdd(session, key, 0);
    CFRelease(key);
    
    /* install the notifier for the cache/state changes */
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainState, kSCCompAnyRegex, kSCEntNetPPP);
    status = SCDNotifierAdd(session, key, kSCDRegexKey);
    CFRelease(key);

    /* let's say we want to be informed via call back for the changes */
    SCDNotifierInformViaCallback(session, cache_notifier, NULL);

    /* read the initial configured interfaces */
    prefix = SCDKeyCreate(CFSTR("%@/%@/%@/"), kSCCacheDomainSetup, kSCCompNetwork, kSCCompService);
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup, kSCCompAnyRegex, kSCEntNetPPP);
    status = SCDList(session, key, kSCDRegexKey, &services);
    if (status == SCD_OK) {
        for (i = 0; i < CFArrayGetCount(services); i++) {
            serviceID = parse_component(CFArrayGetValueAtIndex(services, i), prefix);
            if (serviceID) {
                process_servicesetup(session, serviceID);            
                CFRelease(serviceID);
            }
        }
        my_CFRelease(services);
    }
    
    my_CFRelease(prefix);
    my_CFRelease(key);
    reorder_services(session);
    ppp_postupdatesetup();
    //ppp_printlist();
}

/* -----------------------------------------------------------------------------
install the cache notification and read the initilal configured interfaces
must be called when session cache has been setup
----------------------------------------------------------------------------- */
int process_servicesetup(SCDSessionRef session, CFStringRef serviceID)
{
    CFDictionaryRef	service = NULL, subservice = NULL, interface;
    CFStringRef         subtype = NULL, iftype = NULL;
    struct ppp 		*ppp;

    ppp = ppp_findbyserviceID(serviceID);

    interface = getEntity(session, kSCCacheDomainSetup, serviceID, kSCEntNetInterface);
    if (interface)
        iftype = CFDictionaryGetValue(interface, kSCPropNetInterfaceType);

    if (!interface || 
        (iftype && (CFStringCompare(iftype, kSCValNetInterfaceTypePPP, 0) != kCFCompareEqualTo))) {
        // check to see if service has disappear
        if (ppp) {
            //SCDLog(LOG_INFO, CFSTR("Service has disappear : %@"), serviceID);
            ppp_dispose(ppp);
        }
        goto done;
    }

    service = getEntity(session, kSCCacheDomainSetup, serviceID, kSCEntNetPPP);
    if (!service)	// should we allow creating PPP configuration without PPP dictionnary ?
        goto done;

    /* kSCPropNetServiceSubType contains the entity key Modem, PPPoE, or L2TP */
    subtype = CFDictionaryGetValue(interface, kSCPropNetInterfaceSubType);
    if (!subtype)
        goto done;
    
    //SCDLog(LOG_INFO, CFSTR("change appears, subtype = %d, serviceID = %@\n"), subtype, serviceID);

    if (ppp && CFStringCompare(subtype, ppp->subtypeRef, 0) != kCFCompareEqualTo) {
        // subtype has changed
        ppp_dispose(ppp);
        ppp = 0;
    }

    // check to see if it is a new service
    if (!ppp) {
        ppp = ppp_new(serviceID, subtype);
        if (!ppp)
            goto done;
    }

    ppp_updatesetup(ppp, service);
       
done:            
    my_CFRelease(interface);
    my_CFRelease(service);
    my_CFRelease(subservice);
    return 0;
}

/* -----------------------------------------------------------------------------
install the cache notification and read the initilal configured interfaces
must be called when session cache has been setup
----------------------------------------------------------------------------- */
int process_servicestate(SCDSessionRef session, CFStringRef serviceID)
{
    CFDictionaryRef	service = NULL;
    struct ppp 		*ppp;

    ppp = ppp_findbyserviceID(serviceID);
    if (!ppp)
        return 0;

    service = getEntity(session, kSCCacheDomainState, serviceID, kSCEntNetPPP);
    if (!service)
        return 0;

    ppp_updatestate(ppp, service);
       
    CFRelease(service);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void reorder_services(SCDSessionRef session)
{
    CFDictionaryRef	ip_dict = NULL;
    CFStringRef		key, serviceID;
    CFArrayRef		serviceorder;
    SCDHandleRef	handle = NULL;
    int 		i;
    struct ppp		*ppp;

    key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, kSCEntNetIPv4);
    if (key) {
        if (SCDGet(session, key, &handle) == SCD_OK) {
            ip_dict  = SCDHandleGetData(handle);
            if (ip_dict) {
                serviceorder = CFDictionaryGetValue(ip_dict, kSCPropNetServiceOrder);        
                if (serviceorder) {
                    for (i = 0; i < CFArrayGetCount(serviceorder); i++) {
    
                        serviceID = CFArrayGetValueAtIndex(serviceorder, i);
                        
                        ppp = ppp_findbyserviceID(serviceID);
                        if (ppp) {
                            ppp_setorder(ppp, 0xffff);
                        }
                    }
                }
            }
            SCDHandleRelease(handle);
        }
        CFRelease(key);
    }
}

/* -----------------------------------------------------------------------------
the configd cache/setup has changed
----------------------------------------------------------------------------- */
boolean_t cache_notifier(SCDSessionRef session, void * arg)
{
    CFArrayRef          changes = NULL;
    CFStringRef		prefixsetup = NULL, prefixstate = NULL;
    SCDStatus           status;
    u_long		i, doreorder = 0, dopostsetup = 0;
    char 		str[32];

    status = SCDNotifierGetChanges(session, &changes);
    if (status != SCD_OK || changes == NULL) 
        return TRUE;
    
    prefixsetup = SCDKeyCreate(CFSTR("%@/%@/%@/"), kSCCacheDomainSetup, kSCCompNetwork, kSCCompService);
    prefixstate = SCDKeyCreate(CFSTR("%@/%@/%@/"), kSCCacheDomainState, kSCCompNetwork, kSCCompService);

    //SCDLog(LOG_INFO, CFSTR("ppp_setup_change Changes: %@"), changes);
    
    for (i = 0; i < CFArrayGetCount(changes); i++) {

        CFStringRef	change = NULL, serviceID = NULL, globalip = NULL, user = NULL;
        
        change = CFArrayGetValueAtIndex(changes, i);

        // --------- Check for change of console user --------- 
        user = SCDKeyCreateConsoleUser();
        if (user && CFStringCompare(change, user, 0) == kCFCompareEqualTo) {
            str[0] = 0;
            status = SCDConsoleUserGet(str, sizeof(str), 0, 0);
            if (status == SCD_OK)
                ppp_login();	// key appeared, user logged in
            else
                ppp_logout();	// key disappeared, user logged out
            
            strncpy(gLoggedInUser, str, sizeof(gLoggedInUser));
            CFRelease(user);
            continue;
        }
	my_CFRelease(user);

        // ---------  Check for change in service order --------- 
        globalip = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, kSCEntNetIPv4);
        if (globalip && CFStringCompare(change, globalip, 0) == kCFCompareEqualTo) {
            // can't just reorder the list now 
            // because the list may contain service not already created
            doreorder = 1;
            CFRelease(globalip);
            continue;
        }
	my_CFRelease(globalip);

        // --------- Check for change in other entities (state or setup) --------- 
        serviceID = parse_component(change, prefixsetup);
        if (serviceID) {
            process_servicesetup(session, serviceID);
            CFRelease(serviceID);
            dopostsetup = 1;
            continue;
        }
        
        serviceID = parse_component(change, prefixstate);
        if (serviceID) {
            process_servicestate(session, serviceID);
            CFRelease(serviceID);
            continue;
        }
        
    }

    my_CFRelease(prefixsetup);
    my_CFRelease(prefixstate);
    my_CFRelease(changes);

    if (doreorder) {
        reorder_services(session);
        //ppp_printlist();
    }
    if (dopostsetup)
        ppp_postupdatesetup();
    return TRUE;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static __inline__ CFTypeRef isA_CFType(CFTypeRef obj, CFTypeID type)
{
    if (obj == NULL)
        return (NULL);

    if (CFGetTypeID(obj) != type)
        return (NULL);
    return (obj);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static __inline__ CFTypeRef isA_CFDictionary(CFTypeRef obj)
{
    return (isA_CFType(obj, CFDictionaryGetTypeID()));
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static __inline__ void my_CFRelease(CFTypeRef obj)
{
    if (obj)
        CFRelease(obj);
    return;
}

/* -----------------------------------------------------------------------------
 Given a string 'key' and a string prefix 'prefix',
 return the next component in the slash '/' separated
 key.  If no slash follows the prefix, return NULL.

 Examples:
 1. key = "a/b/c" prefix = "a/"    returns "b"
 2. key = "a/b/c" prefix = "a/b/"  returns NULL
----------------------------------------------------------------------------- */
static CFStringRef parse_component(CFStringRef key, CFStringRef prefix)
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
CFTypeRef getEntity(SCDSessionRef session, CFStringRef domain, CFStringRef serviceID, CFStringRef entity)
{
    CFTypeRef		data = NULL;
    CFStringRef		key;
    SCDHandleRef	handle = NULL;

    if (entity) 
        key = SCDKeyCreateNetworkServiceEntity(domain, serviceID, entity);
    else 
        key = SCDKeyCreate(CFSTR("%@/%@/%@/%@"), domain, kSCCompNetwork, kSCCompService, serviceID);
    if (key) {
        if (SCDGet(session, key, &handle) == SCD_OK) {
            data  = SCDHandleGetData(handle);
            if (data) {
                if (CFGetTypeID(data) == CFDictionaryGetTypeID())	
                    data = (CFTypeRef)CFDictionaryCreateMutableCopy(NULL, 0, data);
                else if (CFGetTypeID(data) == CFStringGetTypeID())
                    data = (CFTypeRef)CFStringCreateCopy(NULL, data);
            }
            SCDHandleRelease(handle);
        }
        CFRelease(key);
    }
    return data;
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
            return 0;
        }
        else if (CFGetTypeID(ref) == CFDataGetTypeID()) {
           string = CFStringCreateWithCharacters(NULL, 
                    (UniChar *)CFDataGetBytePtr(ref), CFDataGetLength(ref)/sizeof(UniChar));               	    
           if (string) {
                CFStringGetCString(string, str, maxlen, kCFStringEncodingUTF8);
                CFRelease(string);
            	return 0;
            }
        }
    }
    return -1;
}

/* -----------------------------------------------------------------------------
get a number from the dictionnary, in service/property
----------------------------------------------------------------------------- */
int getNumber(CFDictionaryRef dict, CFStringRef property, u_int32_t *outval)
{
    CFNumberRef		ref;

    ref  = CFDictionaryGetValue(dict, property);
    if (ref && (CFNumberGetType(ref) == kCFNumberSInt32Type)) {
        CFNumberGetValue(ref, kCFNumberSInt32Type, outval);
        return 0;
    }
    return -1;
}
/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getNumberFromEntity(SCDSessionRef session, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval)
{
    CFTypeRef		data = NULL;
    CFStringRef		key;
    SCDHandleRef	handle = NULL;
    int 		err = -1;

    key = SCDKeyCreate(CFSTR("%@/%@/%@/%@/%@"), domain, kSCCompNetwork, kSCCompService, serviceID, entity);
    if (key) {
        if (SCDGet(session, key, &handle) == SCD_OK) {
            data  = SCDHandleGetData(handle);
            if (data) 
                err = getNumber(data, property, outval);
            SCDHandleRelease(handle);
        }
        CFRelease(key);
    }
    return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getStringFromEntity(SCDSessionRef session, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_char *str, u_int16_t maxlen)
{
    CFTypeRef		data = NULL;
    CFStringRef		key;
    SCDHandleRef	handle = NULL;
    int 		err = -1;

    if (serviceID)
        key = SCDKeyCreate(CFSTR("%@/%@/%@/%@/%@"), domain, kSCCompNetwork, kSCCompService, serviceID, entity);
    else
        key = SCDKeyCreateNetworkGlobalEntity(domain, entity);
    if (key) {
        if (SCDGet(session, key, &handle) == SCD_OK) {
            data  = SCDHandleGetData(handle);
            if (data) {
                err = getString(data, property, str, maxlen);
            }
            SCDHandleRelease(handle);
        }
        CFRelease(key);
    }
    return err;
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
        ret = inet_addr(str);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getAddressFromEntity(SCDSessionRef session, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval)
{
    CFTypeRef		data = NULL;
    CFStringRef		key;
    SCDHandleRef	handle = NULL;
    int 		err = -1;
    CFArrayRef		array;

    key = SCDKeyCreate(CFSTR("%@/%@/%@/%@/%@"), domain, kSCCompNetwork, kSCCompService, serviceID, entity);
    if (key) {
        if (SCDGet(session, key, &handle) == SCD_OK) {
            data  = SCDHandleGetData(handle);
            if (data) {
                array = CFDictionaryGetValue(data, property);
                if (array && CFArrayGetCount(array)) {
                    *outval = CFStringAddrToLong(CFArrayGetValueAtIndex(array, 0));
                    err = 0;
                }
            }
            SCDHandleRelease(handle);
        }
        CFRelease(key);
    }
    return err;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getServiceName(SCDSessionRef session, CFStringRef serviceID, u_char *str, u_int16_t maxlen)
{
    CFTypeRef		data = NULL;
    CFStringRef		key;
    SCDHandleRef	handle = NULL;
    int 		err = -1;

    key = SCDKeyCreate(CFSTR("%@/%@/%@/%@"), kSCCacheDomainSetup, kSCCompNetwork, kSCCompService, serviceID);
    if (key) {
        if (SCDGet(session, key, &handle) == SCD_OK) {
            data  = SCDHandleGetData(handle);
            if (data) {
                err = getString(data, kSCPropUserDefinedName, str, maxlen);
            }
            SCDHandleRelease(handle);
        }
        CFRelease(key);
    }
    return err;
}

