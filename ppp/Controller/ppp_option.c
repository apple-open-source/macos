/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include "ppp_manager.h"
#include "ppp_option.h"

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

static CFStringRef parse_component(CFStringRef key, CFStringRef prefix);

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
    int 		i, nb, ret = 0;
    CFRunLoopSourceRef	rls = NULL;
    CFStringRef         notifs[] = {
        kSCEntNetPPP,
        kSCEntNetModem,
        kSCEntNetInterface,
    	kSCEntNetIPv4,
    	kSCEntNetIPv6,
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

    nb = CFArrayGetCount(services);
    for (i = 0; i < nb; i++) {
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
    if (services) CFRelease(services);
    if (key) CFRelease(key);
    if (setup) CFRelease(setup);
    if (keys) CFRelease(keys);
    if (patterns) CFRelease(patterns);
    return ret;
fail:
    SCLog(TRUE, LOG_ERR, CFSTR("PPPController options_init_all : allocation failed, error = %s\n"),
                SCErrorString(SCError()));
    if (gCfgCache) CFRelease(gCfgCache);
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
    if (interface) CFRelease(interface);
    if (service) CFRelease(service);
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
    int 		i, nb;
    struct ppp		*ppp;

    key = CREATEGLOBALSETUP(kSCEntNetIPv4);
    if (key) {
        ip_dict = (CFDictionaryRef)SCDynamicStoreCopyValue(gCfgCache, key);
        if (ip_dict) {
            serviceorder = CFDictionaryGetValue(ip_dict, kSCPropNetServiceOrder);        
            if (serviceorder) {
  	        nb = CFArrayGetCount(serviceorder);
	        for (i = 0; i < nb; i++) {
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
    u_long		i, nb, doreorder = 0, dopostsetup = 0;

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

    nb = CFArrayGetCount(changedKeys);
    for (i = 0; i < nb; i++) {

        CFStringRef	change, serviceID;
        
        change = CFArrayGetValueAtIndex(changedKeys, i);

        // --------- Check for change of console user --------- 
        if (CFEqual(change, userkey)) {
            CFStringRef olduser = gLoggedInUser;
            gLoggedInUser = SCDynamicStoreCopyConsoleUser(session, 0, 0);
            if (gLoggedInUser == 0)
                ppp_logout();	// key disappeared, user logged out
            else if (olduser == 0)
                ppp_login();	// key appeared, user logged in
            else
                ppp_logswitch();	// key changed, user has switched
            if (olduser) CFRelease(olduser);
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
    if (setup) CFRelease(setup);
    if (state) CFRelease(state);
    if (userkey) CFRelease(userkey);
    if (ipkey) CFRelease(ipkey);
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
        kSCEntNetDNS,
        kSCEntNetL2TP,
        kSCEntNetPPTP,
        NULL,
    };

    key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%@"), domain, kSCCompNetwork, kSCCompService, serviceID);
    if (key == 0)
        goto fail;
        
    data = SCDynamicStoreCopyValue(gCfgCache, key);
    if (data == 0)
        goto fail;
        
    CFRelease(key);
        
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
        data = SCDynamicStoreCopyValue(gCfgCache, key);
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
           string = CFStringCreateWithCharacters(NULL, 
                    (UniChar *)CFDataGetBytePtr(ref), CFDataGetLength(ref)/sizeof(UniChar));               	    
           if (string) {
                CFStringGetCString(string, str, maxlen, kCFStringEncodingUTF8);
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
