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
#include <SystemConfiguration/SystemConfiguration.h>
//#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#define SCLog

#include "ppp_msg.h"
#include "../Family/if_ppplink.h"
#include "ppp_client.h"
#include "ppp_option.h"
#include "ppp_manager.h"

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

static __inline__ CFTypeRef isA_CFType(CFTypeRef obj, CFTypeID type);
static __inline__ CFTypeRef isA_CFDictionary(CFTypeRef obj);
static __inline__ void my_CFRelease(CFTypeRef obj);
static CFStringRef parse_component(CFStringRef key, CFStringRef prefix);
static u_int32_t CFStringAddrToLong(CFStringRef string);

static void reorder_services();
static int process_servicestate(CFStringRef serviceID);
static int process_servicesetup(CFStringRef serviceID);
static void cache_notifier(SCDynamicStoreRef session, CFArrayRef changedKeys, void *info);


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

CFStringRef		gLoggedInUser = NULL;
SCDynamicStoreRef	gCfgCache = NULL;

/* -----------------------------------------------------------------------------
install the cache notification and read the initilal configured interfaces
must be called when session cache has been setup
----------------------------------------------------------------------------- */
int options_init_all()
{
    CFStringRef         key = NULL, setup = NULL;
    CFMutableArrayRef	keys = NULL, patterns = NULL;
    CFArrayRef		services = NULL;
    int 		i, ret = 0;
    CFRunLoopSourceRef	rls = NULL;
    CFStringRef         notifs[] = {
        kSCEntNetPPP,
        kSCEntNetModem,
        kSCEntNetInterface,
    	kSCEntNetIPv4,
        kSCEntNetDNS,
        NULL,
    };

    /* opens now our session to the cache */
    if ((gCfgCache = SCDynamicStoreCreate(0, CFSTR("PPPController"), cache_notifier, 0)) == NULL)
        goto fail;
    
    if ((rls = SCDynamicStoreCreateRunLoopSource(0, gCfgCache, 0)) == NULL) 
        goto fail;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    gLoggedInUser = SCDynamicStoreCopyConsoleUser(gCfgCache, 0, 0);
    
    keys = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    patterns = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    if (keys == NULL || patterns == NULL)
        goto fail;

    /* install the notifier for the cache/setup changes */
    for (i = 0; notifs[i]; i++) {   
        if ((key = CREATESERVICESETUP(notifs[i])) == NULL)
            goto fail;    
        CFArrayAppendValue(patterns, key);
        CFRelease(key);
    }
    
    /* install the notifier for the cache/state changes */
    if ((key = CREATESERVICESTATE(kSCEntNetPPP)) == NULL)
        goto fail;
    CFArrayAppendValue(patterns, key);
    CFRelease(key);
    
    /* install the notifier for the global changes */
    if ((key = CREATEGLOBALSETUP(kSCEntNetIPv4)) == NULL)
        goto fail;
    CFArrayAppendValue(keys, key);
    CFRelease(key);
    
    /* install the notifier for user login/logout */
    if ((key = SCDynamicStoreKeyCreateConsoleUser(0)) == NULL)
        goto fail;
    CFArrayAppendValue(keys, key);
    CFRelease(key);    

    /* add all the notification in one chunk */
    SCDynamicStoreSetNotificationKeys(gCfgCache, keys, patterns);

    /* read the initial configured interfaces */
    key = CREATESERVICESETUP(kSCEntNetPPP);
    setup = CREATEPREFIXSETUP();
    if (key == NULL || setup == NULL)
        goto fail;
        
    services = SCDynamicStoreCopyKeyList(gCfgCache, key);
    if (services == NULL)
        goto done;	// no PPP service setup

    for (i = 0; i < CFArrayGetCount(services); i++) {
        CFStringRef serviceID;
        if (serviceID = parse_component(CFArrayGetValueAtIndex(services, i), setup)) {
            process_servicesetup(serviceID);            
            CFRelease(serviceID);
        }
    }
    
    reorder_services();
    ppp_postupdatesetup();
    //ppp_printlist();
done:    
    my_CFRelease(services);
    my_CFRelease(key);
    my_CFRelease(setup);
    my_CFRelease(keys);
    my_CFRelease(patterns);
    return ret;
fail:
    SCLog(TRUE, LOG_ERR, CFSTR("PPPController options_init_all : allocation failed, error = %s\n"),
                SCErrorString(SCError()));
    my_CFRelease(gCfgCache);
    ret = 1;
    goto done;
}

/* -----------------------------------------------------------------------------
install the cache notification and read the initilal configured interfaces
must be called when session cache has been setup
----------------------------------------------------------------------------- */
int process_servicesetup(CFStringRef serviceID)
{
    CFDictionaryRef	service = NULL, interface;
    CFStringRef         subtype = NULL, iftype = NULL;
    struct ppp 		*ppp;

    ppp = ppp_findbyserviceID(serviceID);

    interface = copyEntity(kSCDynamicStoreDomainSetup, serviceID, kSCEntNetInterface);
    if (interface)
        iftype = CFDictionaryGetValue(interface, kSCPropNetInterfaceType);

    if (!interface || 
        (iftype && !CFEqual(iftype, kSCValNetInterfaceTypePPP))) {
        // check to see if service has disappear
        if (ppp) {
            //SCDLog(LOG_INFO, CFSTR("Service has disappear : %@"), serviceID);
            ppp_dispose(ppp);
        }
        goto done;
    }

    service = copyEntity(kSCDynamicStoreDomainSetup, serviceID, kSCEntNetPPP);
    if (!service)	// should we allow creating PPP configuration without PPP dictionnary ?
        goto done;

    /* kSCPropNetServiceSubType contains the entity key Modem, PPPoE, or L2TP */
    subtype = CFDictionaryGetValue(interface, kSCPropNetInterfaceSubType);
    if (!subtype)
        goto done;
    
    //SCDLog(LOG_INFO, CFSTR("change appears, subtype = %d, serviceID = %@\n"), subtype, serviceID);

    if (ppp && !CFEqual(subtype, ppp->subtypeRef)) {
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
    return 0;
}

/* -----------------------------------------------------------------------------
install the cache notification and read the initilal configured interfaces
must be called when session cache has been setup
----------------------------------------------------------------------------- */
int process_servicestate(CFStringRef serviceID)
{
    CFDictionaryRef	service = NULL;
    struct ppp 		*ppp;

    ppp = ppp_findbyserviceID(serviceID);
    if (!ppp)
        return 0;

    service = copyEntity(kSCDynamicStoreDomainState, serviceID, kSCEntNetPPP);
    if (!service)
        return 0;

    ppp_updatestate(ppp, service);
       
    CFRelease(service);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void reorder_services()
{
    CFDictionaryRef	ip_dict = NULL;
    CFStringRef		key, serviceID;
    CFArrayRef		serviceorder;
    int 		i;
    struct ppp		*ppp;

    key = CREATEGLOBALSETUP(kSCEntNetIPv4);
    if (key) {
        ip_dict = (CFDictionaryRef)SCDynamicStoreCopyValue(gCfgCache, key);
        if (ip_dict) {
            serviceorder = CFDictionaryGetValue(ip_dict, kSCPropNetServiceOrder);        
            if (serviceorder) {
                for (i = 0; i < CFArrayGetCount(serviceorder); i++) {
                    serviceID = CFArrayGetValueAtIndex(serviceorder, i);                    
                    if (ppp = ppp_findbyserviceID(serviceID))
                        ppp_setorder(ppp, 0xffff);
                }
            }
            CFRelease(ip_dict);
        }
        CFRelease(key);
    }
}

/* -----------------------------------------------------------------------------
the configd cache/setup has changed
----------------------------------------------------------------------------- */
void cache_notifier(SCDynamicStoreRef session, CFArrayRef changedKeys, void *info)
{
    CFStringRef		setup, state, userkey, ipkey;
    u_long		i, doreorder = 0, dopostsetup = 0;

    //SCLog(TRUE, LOG_ERR, CFSTR("PPPController cache_notifier \n"));
    
    if (changedKeys == NULL) 
        return;
    
    setup = CREATEPREFIXSETUP();        
    state = CREATEPREFIXSTATE();
    userkey = SCDynamicStoreKeyCreateConsoleUser(0);
    ipkey = CREATEGLOBALSETUP(kSCEntNetIPv4);
    
    if (setup == NULL || state == NULL || userkey == NULL || ipkey == NULL) {
        SCLog(TRUE, LOG_ERR, CFSTR("PPPController cache_notifier : can't allocate keys\n"));
        goto done;
    }

    for (i = 0; i < CFArrayGetCount(changedKeys); i++) {

        CFStringRef	change, serviceID;
        
        change = CFArrayGetValueAtIndex(changedKeys, i);

        // --------- Check for change of console user --------- 
        if (CFEqual(change, userkey)) {
            my_CFRelease(gLoggedInUser);
            if (gLoggedInUser = SCDynamicStoreCopyConsoleUser(session, 0, 0)) 
                ppp_login();	// key appeared, user logged in
            else 
                ppp_logout();	// key disappeared, user logged out
            continue;
        }

        // ---------  Check for change in service order --------- 
        if (CFEqual(change, ipkey)) {
            // can't just reorder the list now 
            // because the list may contain service not already created
            doreorder = 1;
            continue;
        }

        // --------- Check for change in other entities (state or setup) --------- 
        serviceID = parse_component(change, setup);
        if (serviceID) {
            process_servicesetup(serviceID);
            CFRelease(serviceID);
            dopostsetup = 1;
            continue;
        }
        
        serviceID = parse_component(change, state);
        if (serviceID) {
            process_servicestate(serviceID);
            CFRelease(serviceID);
            continue;
        }
    }

    if (doreorder) {
        reorder_services();
        //ppp_printlist();
    }
    if (dopostsetup)
        ppp_postupdatesetup();

done:
    my_CFRelease(setup);
    my_CFRelease(state);
    my_CFRelease(userkey);
    my_CFRelease(ipkey);
    return;
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
CFTypeRef copyEntity(CFStringRef domain, CFStringRef serviceID, CFStringRef entity)
{
    CFTypeRef		data = NULL;
    CFStringRef		key;

    if (serviceID)
        key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, domain, serviceID, entity);
    else
        key = SCDynamicStoreKeyCreateNetworkGlobalEntity(0, domain, entity);

    if (key) {
        data = SCDynamicStoreCopyValue(gCfgCache, key);
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
    if (ref && (CFGetTypeID(ref) == CFNumberGetTypeID())) {
        CFNumberGetValue(ref, kCFNumberSInt32Type, outval);
        return 0;
    }
    return -1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getNumberFromEntity(CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval)
{
    CFTypeRef		data;
    int 		err = -1;

    if (data = copyEntity(domain, serviceID, entity)) {
        err = getNumber(data, property, outval);
        CFRelease(data);
    }
    return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getStringFromEntity(CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_char *str, u_int16_t maxlen)
{
    CFTypeRef		data;
    int 		err = -1;

    data = copyEntity(domain, serviceID, entity);
    if (data) {
        err = getString(data, property, str, maxlen);
        CFRelease(data);
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
int getAddressFromEntity(CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval)
{
    CFTypeRef		data;
    int 		err = -1;
    CFArrayRef		array;

    data = copyEntity(domain, serviceID, entity);
    if (data) {
        array = CFDictionaryGetValue(data, property);
        if (array && CFArrayGetCount(array)) {
            *outval = CFStringAddrToLong(CFArrayGetValueAtIndex(array, 0));
            err = 0;
        }
        CFRelease(data);
    }
    return err;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getServiceName(CFStringRef serviceID, u_char *str, u_int16_t maxlen)
{
    CFTypeRef		data;
    int 		err = -1;

    data = copyEntity(kSCDynamicStoreDomainSetup, serviceID, 0);
    if (data) {
        err = getString(data, kSCPropUserDefinedName, str, maxlen);
        CFRelease(data);
    }
    return err;
}

