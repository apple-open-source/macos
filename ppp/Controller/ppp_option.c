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

#include "ppp_msg.h"
#include "../Family/PPP.kmodproj/ppp.h"

#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>

#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "chap.h"
#include "upap.h"
#include "ppp_client.h"
#include "ppp_option.h"
#include "ppp_manager.h"
#include "ppp_utils.h"
#include "link.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */

static __inline__ CFTypeRef isA_CFType(CFTypeRef obj, CFTypeID type);
static __inline__ CFTypeRef isA_CFDictionary(CFTypeRef obj);
static __inline__ void my_CFRelease(CFTypeRef obj);
static u_int8_t getNumber(CFDictionaryRef service, CFStringRef property, u_int32_t *outval);
static u_int8_t getString(CFDictionaryRef service, CFStringRef property, u_char *str, u_int16_t maxlen);
static CFStringRef parse_component(CFStringRef key, CFStringRef prefix);
static CFArrayRef getEntityAll(SCDSessionRef session, CFStringRef entity);
static CFTypeRef getEntity(SCDSessionRef session, CFStringRef serviceID, CFStringRef entity);
//static CFStringRef getStringProperty(SCDSessionRef session, CFStringRef serviceID, CFStringRef entity, CFStringRef property);
static void setFromString(struct opt_str *dst, CFDictionaryRef service, CFStringRef property);
static void setFromLong(struct opt_long *dst, CFDictionaryRef service, CFStringRef property,
                        u_long min, u_long max, u_long def);
static u_int32_t CFStringAddrToLong(CFStringRef string);
static void display_options(struct options *opts);
static void display_str_opt (u_char *name, struct opt_str *option);
static void display_long_opt (u_char *name, struct opt_long *option);
static void fill_options(SCDSessionRef session, CFStringRef serviceID, CFDictionaryRef service, struct options *opts);
static boolean_t cache_notifier(SCDSessionRef session, void * arg);

static void init_options (struct options *opts);
static void default_options (struct options *opts);
//static u_long getServiceID(u_char *service);
static void fill_pppoe_options(CFDictionaryRef service, struct options *opts);
static void fill_tty_options(CFDictionaryRef service, struct options *opts);
static void apply_options(struct ppp *ppp);
static void reorder_services(SCDSessionRef session);


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
    CFStringRef         key;
    CFArrayRef		services;
    u_int32_t 		i;
    u_short		pppoe = 0, pppserial = 0;
    
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
    key = SCDKeyCreateConsoleUser();
    status = SCDNotifierAdd(session, key, 0);
    CFRelease(key);
    SCDNotifierInformViaCallback(session, cache_notifier, NULL);

    /* read the initial configured interfaces */
    services = getEntityAll(session, kSCEntNetPPP);
    if (services == NULL)
        return;

    for (i = 0; i < CFArrayGetCount(services); i++) {
        CFDictionaryRef		service = NULL, subservice = NULL, interface = NULL;
        CFStringRef         	subtype = NULL, serviceID = NULL;
        struct ppp 		*ppp = NULL;

        service = CFArrayGetValueAtIndex(services, i);
        if (!service)
            goto loop_done;
        serviceID = CFDictionaryGetValue(service, CFSTR("SERVICEID"));        
        if (!serviceID)
            goto loop_done;
        interface = getEntity(session, serviceID, kSCEntNetInterface);
        if (!interface)
            goto loop_done;

        /* kSCPropNetServiceSubType contains the entity key Modem, PPPoE, or L2TP */
        subtype = CFDictionaryGetValue(interface, kSCPropNetInterfaceSubType);
        if (!subtype)
            goto loop_done;

        if (CFStringCompare(subtype, kSCValNetInterfaceSubTypePPPoE, 0) == kCFCompareEqualTo) 
            ppp = ppp_new("pppoe", pppoe++, APPLE_IF_FAM_PPP_PPPoE,  CFStringGetIntValue(serviceID));
        else if (CFStringCompare(subtype, kSCValNetInterfaceSubTypePPPSerial, 0) == kCFCompareEqualTo)
            ppp = ppp_new("ppp", pppserial++, APPLE_IF_FAM_PPP_SERIAL, CFStringGetIntValue(serviceID));

        if (!ppp)
            goto loop_done;

       // retrieve the default options 
        init_options(&ppp->def_options);
        default_options(&ppp->def_options);
        fill_options(session, serviceID, service, &ppp->def_options);
        setFromString(&(ppp->def_options.dev.name), interface, kSCPropNetInterfaceDeviceName);
       switch (ppp->subfamily) { 
            case APPLE_IF_FAM_PPP_PPPoE:
                subservice = getEntity(session, serviceID, kSCEntNetPPPoE);
                if (subservice)
                    fill_pppoe_options(subservice, &ppp->def_options);
                break;
            case APPLE_IF_FAM_PPP_SERIAL:
                subservice = getEntity(session, serviceID, kSCEntNetModem);
                if (subservice)
                    fill_tty_options(subservice, &ppp->def_options);
                break;
        }
        display_options(&ppp->def_options);
        apply_options(ppp);

loop_done:            
        my_CFRelease(interface);
	my_CFRelease(subservice);
    }
    
    my_CFRelease(services);
    reorder_services(session);
    //ppp_printlist();
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void reorder_services(SCDSessionRef session)
{
    CFDictionaryRef	ip_dict = NULL;
    CFStringRef		key;
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
                        CFStringRef serviceID = NULL;
    
                        serviceID = CFArrayGetValueAtIndex(serviceorder, i);
                        
                        ppp = ppp_findbyserviceID(CFStringGetIntValue(serviceID));
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
remove cache notifier
----------------------------------------------------------------------------- */
void options_dispose_all(SCDSessionRef session)
{

}

/* -----------------------------------------------------------------------------
return the id associated to the service C string
for example, for the service "Setup:/Network/Service/2/PPP", id = 2
----------------------------------------------------------------------------- */
u_long getServiceID(u_char *service)
{
    u_long 	m = 10, id = 0, len = strlen(service);
    u_char 	*p = &service[len - 1];
    
    while ((len-- > 1) && (*p-- != '/'));
    if (len--) {
        id = (*p--) - '0';
        while (len-- && (*p != '/')) {
            id += ((*p--) - '0') * m;
            m *= 10;
        }
    }
    return id;
}

/* -----------------------------------------------------------------------------
return if a user is currently logged in
----------------------------------------------------------------------------- */
u_char isUserLoggedIn() 
{
    return gLoggedInUser[0] != 0;
}

/* -----------------------------------------------------------------------------
the configd cache/setup has changed
----------------------------------------------------------------------------- */
boolean_t cache_notifier(SCDSessionRef session, void * arg)
{
    CFArrayRef          changes = NULL;
    CFStringRef		prefix = NULL;
    SCDStatus           status;
    u_long		i, doreorder = 0;
    char 		str[32];

    status = SCDNotifierGetChanges(session, &changes);
    if (status != SCD_OK || changes == NULL) 
        return TRUE;
    
    prefix = SCDKeyCreate(CFSTR("%@/%@/%@/"),
                        kSCCacheDomainSetup, kSCCompNetwork, kSCCompService);

    //SCDLog(LOG_INFO, CFSTR("ppp_setup_change Changes: %@"), changes);
    
    for (i = 0; i < CFArrayGetCount(changes); i++) {

        CFStringRef		change = NULL, serviceID = NULL, subtype = NULL, iftype = NULL, globalip = NULL, user = NULL;
        CFDictionaryRef 	service = NULL, subservice = NULL, interface = NULL;
        struct ppp 		*ppp = NULL;
        u_short 		pppsubtype;
        
        change = CFArrayGetValueAtIndex(changes, i);

        // Check for change of console user
        user = SCDKeyCreateConsoleUser();
        if (user && CFStringCompare(change, user, 0) == kCFCompareEqualTo) {
            str[0] = 0;
            status = SCDConsoleUserGet(str, sizeof(str), 0, 0);
            if (status != SCD_OK)
                ppp_logout();	// key disappeared, user logged out
            strncpy(gLoggedInUser, str, sizeof(gLoggedInUser));
            CFRelease(user);
            continue;
        }

        // Check for change in service order
        globalip = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, kSCEntNetIPv4);
        if (globalip && CFStringCompare(change, globalip, 0) == kCFCompareEqualTo) {
            // can't just reorder the list now 
            // because the list may contain service not already created
            doreorder = 1;
            CFRelease(globalip);
            continue;
        }

        // Check for change in other entities
        serviceID = parse_component(change, prefix);
        if (!serviceID)
            goto loop_done;

        ppp = ppp_findbyserviceID(CFStringGetIntValue(serviceID));

        interface = getEntity(session, serviceID, kSCEntNetInterface);      
        if (interface)
            iftype = CFDictionaryGetValue(interface, kSCPropNetInterfaceType);
                  
        if (!interface || 
            (iftype && (CFStringCompare(iftype, kSCValNetInterfaceTypePPP, 0) != kCFCompareEqualTo))) {
            // check to see if service has disappear
            if (ppp) {
                //SCDLog(LOG_INFO, CFSTR("Service has disappear : %@"), serviceID);
                init_options(&ppp->def_options);
                default_options(&ppp->def_options);
                ppp_dispose(ppp);
            }
            goto loop_done;
        }

        service = getEntity(session, serviceID, kSCEntNetPPP);
        if (!service)	// should we allow creating PPP configuration without PPP dictionnary ?
            goto loop_done;
        
         /* kSCPropNetServiceSubType contains the entity key Modem, PPPoE, or L2TP */
        subtype = CFDictionaryGetValue(interface, kSCPropNetInterfaceSubType);
        if (!subtype)
            goto loop_done;
        
        if (CFStringCompare(subtype, kSCValNetInterfaceSubTypePPPoE, 0) == kCFCompareEqualTo)
            pppsubtype = APPLE_IF_FAM_PPP_PPPoE;
        else if (CFStringCompare(subtype, kSCValNetInterfaceSubTypePPPSerial, 0) == kCFCompareEqualTo)
            pppsubtype = APPLE_IF_FAM_PPP_SERIAL;
        else 
            goto loop_done;  // not a ppp interface
            
        // check to see if it is a new service
        if (!ppp) {
            switch (pppsubtype) {
                case APPLE_IF_FAM_PPP_PPPoE:
                    ppp = ppp_new("pppoe", ppp_findfreeunit(APPLE_IF_FAM_PPP_PPPoE),
                        APPLE_IF_FAM_PPP_PPPoE,  CFStringGetIntValue(serviceID));
                    break;
                case APPLE_IF_FAM_PPP_SERIAL:
                    ppp = ppp_new("ppp", ppp_findfreeunit(APPLE_IF_FAM_PPP_SERIAL),
                        APPLE_IF_FAM_PPP_SERIAL, CFStringGetIntValue(serviceID));
                    break;
            }
        }
        
        if (!ppp) 
            goto loop_done;

        // retrieve the default options 
        init_options(&ppp->def_options);
        default_options(&ppp->def_options);
        fill_options(session, serviceID, service, &ppp->def_options);
        setFromString(&(ppp->def_options.dev.name), interface, kSCPropNetInterfaceDeviceName);
        switch (ppp->subfamily) { 
            case APPLE_IF_FAM_PPP_PPPoE:
                subservice = getEntity(session, serviceID, kSCEntNetPPPoE);
                if (subservice)
                    fill_pppoe_options(subservice, &ppp->def_options);
                break;
            case APPLE_IF_FAM_PPP_SERIAL:
                subservice = getEntity(session, serviceID, kSCEntNetModem);
                if (subservice)
                    fill_tty_options(subservice, &ppp->def_options);
                break;
        }
        display_options(&ppp->def_options);
        apply_options(ppp);

loop_done:
	my_CFRelease(serviceID);
	my_CFRelease(service);
	my_CFRelease(subservice);
	my_CFRelease(interface);
	my_CFRelease(user);
	my_CFRelease(globalip);
    }

    my_CFRelease(prefix);
    my_CFRelease(changes);
    if (doreorder) {
        reorder_services(session);
        //ppp_printlist();
    }
    return TRUE;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void apply_options(struct ppp *ppp)
{

    if (ppp->phase == PPP_IDLE) {
        ppp_reinit(ppp, 3);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void init_options (struct options *opts)
{
    
    memset (opts, 0, sizeof (struct options));
}

/* -----------------------------------------------------------------------------
need to read the default options from the database
----------------------------------------------------------------------------- */
void default_options (struct options *opts)
{
    
    // defaults for lcp part
    set_long_opt(&opts->lcp.pcomp, OPT_LCP_PCOMP_DEF, 0, 1, 1);
    set_long_opt(&opts->lcp.accomp, OPT_LCP_ACCOMP_DEF, 0, 1, 1);
    set_long_opt(&opts->lcp.mru, OPT_LCP_MRU_DEF, 0, 0xFFFFFFFF, 1);
    set_long_opt(&opts->lcp.mtu, OPT_LCP_MRU_DEF, 0, 0xFFFFFFFF, 1);
    set_long_opt(&opts->lcp.rcaccm, OPT_LCP_RCACCM_DEF, 0, 0xFFFFFFFF, 1);
    set_long_opt(&opts->lcp.txaccm, OPT_LCP_TXACCM_DEF, 0, 0xFFFFFFFF, 1);
    set_long_opt(&opts->lcp.echo, (((u_long)OPT_LCP_ECHOINTERVAL_DEF) << 16) + OPT_LCP_ECHOFAILURE_DEF , 0, 0xFFFFFFFF, 0);

    // defaults for ipcp part
    set_long_opt(&opts->ipcp.hdrcomp, OPT_IPCP_HDRCOMP_DEF, 0, 1, 1);
    set_long_opt(&opts->ipcp.localaddr, 0, 0, 1, 1);
    set_long_opt(&opts->ipcp.remoteaddr, 0, 0, 1, 1);
   set_long_opt(&opts->ipcp.useserverdns,OPT_IPCP_USESERVERDNS_DEF, 0, 1, 1);
    set_long_opt(&opts->ipcp.serverdns1, 0, 0, 1, 1);
    set_long_opt(&opts->ipcp.serverdns2, 0, 0, 1, 1);

    // defaults for sec part
    set_long_opt(&opts->auth.proto, OPT_AUTH_PROTO_DEF, 0, 1, 1);

    // defaults for dev part
    set_str_opt(&opts->dev.name, OPT_DEV_NAME_DEF, strlen(OPT_DEV_NAME_DEF));
    set_long_opt(&opts->dev.speed, OPT_DEV_SPEED_DEF, 0, 0xFFFFFFFF, 1);
    set_str_opt(&opts->dev.connectscript, OPT_DEV_CONNECTSCRIPT_DEF, strlen(OPT_DEV_CONNECTSCRIPT_DEF));
    set_long_opt(&opts->dev.speaker, OPT_DEV_SPEAKER_DEF, 0, 0xFFFFFFFF, 1);
    set_long_opt(&opts->dev.dialmode, OPT_DEV_DIALMODE_DEF, 0, 0xFFFFFFFF, 1);
    set_long_opt(&opts->dev.pulse, OPT_DEV_PULSE_DEF, 0, 0xFFFFFFFF, 1);
    //set_str_opt(&opts->dev.connectprgm, OPT_DEV_CONNECTPRGM_DEF, strlen(OPT_DEV_CONNECTPRGM_DEF));

    // defaults for comm part
    set_long_opt(&opts->comm.terminalmode, OPT_COMM_TERMINALMODE_DEF, 0, 0xFFFFFFFF, 1);
    set_str_opt(&opts->comm.terminalscript, "", strlen(""));
    //set_str_opt(&opts->comm.terminalprgm, "", strlen(""));
    set_str_opt(&opts->comm.remoteaddr, "", strlen(""));
    set_str_opt(&opts->comm.altremoteaddr, "", strlen(""));
    set_str_opt(&opts->comm.listenfilter, "", strlen(""));
    set_long_opt(&opts->comm.connectdelay, OPT_COMM_CONNECTDELAY_DEF, 0, 0xFFFFFFFF, 1);
    set_long_opt(&opts->comm.idletimer, OPT_COMM_IDLETIMER_DEF, 0, 0xFFFFFFFF, 1);
    set_long_opt(&opts->comm.sessiontimer, OPT_COMM_SESSIONTIMER_DEF, 0, 0xFFFFFFFF, 1);
    set_long_opt(&opts->comm.redialcount, 0, 0, 0xFFFFFFFF, 1);
    set_long_opt(&opts->comm.redialinterval, 0, 0, 0xFFFFFFFF, 1);

    // defaults for misc part
    set_str_opt(&opts->misc.logfile, OPT_LOGFILE_DEF, strlen(OPT_LOGFILE_DEF));
    set_long_opt(&opts->comm.loopback, 0, 0, 1, 1);
    set_long_opt(&opts->misc.autoconnect, OPT_AUTOCONNECT_DEF, 0, 1, 1);
    set_long_opt(&opts->misc.disclogout, OPT_DISCLOGOUT_DEF, 0, 1, 1);
    set_long_opt(&opts->misc.connlogout, OPT_CONNLOGOUT_DEF, 0, 1, 1);
    set_long_opt(&opts->misc.verboselog, OPT_VERBOSELOG_DEF, 0, 1, 1);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void display_options(struct options *opts)
{
    display_long_opt("kSCPropNetPPPLCPCompressionPField", &opts->lcp.pcomp);
    display_long_opt("kSCPropNetPPPLCPCompressionACField", &opts->lcp.accomp);
    display_long_opt("kSCPropNetPPPLCPMRU", &opts->lcp.mru);
    display_long_opt("kSCPropNetPPPLCPMTU", &opts->lcp.mtu);
    display_long_opt("kSCPropNetPPPLCPReceiveACCM", &opts->lcp.rcaccm);
    display_long_opt("kSCPropNetPPPLCPTransmitACCM", &opts->lcp.txaccm);
    display_long_opt("kSCPropNetPPPLCPEchoInterval/Failure", &opts->lcp.echo);

    // ipcp options
    display_long_opt("kSCPropNetPPPIPCPCompressionVJ", &opts->ipcp.hdrcomp);
    display_long_opt("kSCPropNetPPPIPCPUseServerDNS", &opts->ipcp.useserverdns);
    display_long_opt("kSCPropNetPPPIPCPLocalAddress", &opts->ipcp.localaddr);
    display_long_opt("kSCPropNetPPPIPCPRemoteAddress", &opts->ipcp.remoteaddr);

    // auth options
    display_str_opt("kSCPropNetPPPAuthName", &opts->auth.name);
    display_str_opt("kSCPropNetPPPAuthPassword", &opts->auth.passwd);

    // dev options
    display_str_opt("kSCPropNetModemPortName", &opts->dev.name);
    display_str_opt("kSCPropNetModemConnectionScript", &opts->dev.connectscript);
    display_long_opt("kSCPropNetModemDialMode", &opts->dev.dialmode);
    display_long_opt("kSCPropNetModemSpeaker", &opts->dev.speaker);
    display_long_opt("kSCPropNetModemPulseDial", &opts->dev.pulse);

    // comm options
    display_str_opt("kSCPropNetPPPCommRemoteAddress", &opts->comm.remoteaddr);
    display_str_opt("kSCPropNetPPPCommAlternateRemoteAddress", &opts->comm.altremoteaddr);
    display_str_opt("kSCPropNetPPPCommTerminalScript", &opts->comm.terminalscript);
    display_long_opt("kSCPropNetPPPCommTerminalWindow[Terminal Mode]", &opts->comm.terminalmode);
    display_long_opt("kSCPropNetPPPCommConnectDelay", &opts->comm.connectdelay);
    display_long_opt("kSCPropNetPPPCommIdleTimer", &opts->comm.idletimer);
    display_long_opt("kSCPropNetPPPCommRedialCount", &opts->comm.redialcount);
    display_long_opt("kSCPropNetPPPCommRedialInterval", &opts->comm.redialinterval);

    // other options
    display_long_opt("kSCPropNetPPPVerboseLogging", &opts->misc.verboselog);
    display_str_opt("kSCPropNetPPPLogfile", &opts->misc.logfile);
    display_long_opt("kSCPropNetPPPDialOnDemand", &opts->misc.autoconnect);
    display_long_opt("kSCPropNetPPPDisconnectOnLogout", &opts->misc.disclogout);
    display_long_opt("kSCPropNetPPPAllowConnectWhenLogout", &opts->misc.connlogout);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void fill_options(SCDSessionRef session, CFStringRef serviceID, CFDictionaryRef service, struct options *opts)
{
    u_int32_t 		lval, lval1;
    CFDictionaryRef 	dict;
    CFArrayRef		array;
    
    // lcp options
    setFromLong(&opts->lcp.pcomp, service, kSCPropNetPPPLCPCompressionPField, 0, 1, 1);
    setFromLong(&opts->lcp.accomp, service, kSCPropNetPPPLCPCompressionACField, 0, 1, 1);
    setFromLong(&opts->lcp.mru, service, kSCPropNetPPPLCPMRU, 0, 0xFFFFFFFF, 1);
    setFromLong(&opts->lcp.mtu, service, kSCPropNetPPPLCPMTU, 0, 0xFFFFFFFF, 1);
    setFromLong(&opts->lcp.rcaccm, service, kSCPropNetPPPLCPReceiveACCM, 0, 0xFFFFFFFF, 1);
    setFromLong(&opts->lcp.txaccm, service, kSCPropNetPPPLCPTransmitACCM, 0, 0xFFFFFFFF, 1);
    lval = 1;  // echo ON by default
    getNumber(service, kSCPropNetPPPLCPEchoEnabled, &lval);
    if (lval 
        && getNumber(service, kSCPropNetPPPLCPEchoInterval, &lval)
        && getNumber(service, kSCPropNetPPPLCPEchoFailure, &lval1))
        set_long_opt(&opts->lcp.echo, (lval << 16) + lval1 , 0, 0xFFFFFFFF, 0);
    else 
      set_long_opt(&opts->lcp.echo, 0, 0, 0xFFFFFFFF, 0);    

    // ipcp options
    setFromLong(&opts->ipcp.hdrcomp, service, kSCPropNetPPPIPCPCompressionVJ, 0, 1, 1);
    dict = getEntity(session, serviceID, kSCEntNetIPv4);
    if (dict) {
        array = CFDictionaryGetValue(dict, kSCPropNetIPv4Addresses);
        if (array && CFArrayGetCount(array)) 
            set_long_opt(&opts->ipcp.localaddr, 
                CFStringAddrToLong(CFArrayGetValueAtIndex(array, 0)), 0, 0xFFFFFFFF, 1);
               
        array = CFDictionaryGetValue(dict, kSCPropNetIPv4DestAddresses);
        if (array && CFArrayGetCount(array)) 
            set_long_opt(&opts->ipcp.remoteaddr, 
                CFStringAddrToLong(CFArrayGetValueAtIndex(array, 0)), 0, 0xFFFFFFFF, 1);
        CFRelease(dict);
    }
    dict = getEntity(session, serviceID, kSCEntNetDNS);
    if (dict) {
        array = CFDictionaryGetValue(dict, kSCPropNetDNSServerAddresses);
        if (array && CFArrayGetCount(array)) 
            set_long_opt(&opts->ipcp.useserverdns,
                CFStringAddrToLong(CFArrayGetValueAtIndex(array, 0)) == 0 , 0, 1, 1);
        CFRelease(dict);
    }
    
    // auth options
    setFromString(&opts->auth.name, service, kSCPropNetPPPAuthName);
    setFromString(&opts->auth.passwd, service, kSCPropNetPPPAuthPassword);

    // comm options
    setFromString(&opts->comm.remoteaddr, service, kSCPropNetPPPCommRemoteAddress);
    setFromString(&opts->comm.altremoteaddr, service, kSCPropNetPPPCommAlternateRemoteAddress);
    //if (getNumber(service, kSCPropNetPPPCommDisplayTerminalWindow, &lval)) {
    if (getNumber(service, kSCPropNetPPPCommDisplayTerminalWindow, &lval)) {
        lval1 = PPP_COMM_TERM_NONE;
        if (lval) {
            lval1 = PPP_COMM_TERM_SCRIPT;
            if (getNumber(service, kSCPropNetPPPCommDisplayTerminalWindow, &lval) && lval)
                lval1 = PPP_COMM_TERM_WINDOW;
        }
        set_long_opt(&opts->comm.terminalmode, lval1, 0, 0xFFFFFFFF, 1);
        setFromString(&opts->comm.terminalscript, service, kSCPropNetPPPCommTerminalScript);
    }
    setFromLong(&opts->comm.connectdelay, service, kSCPropNetPPPCommConnectDelay, 0, 0xFFFFFFFF, 1);
    
    setFromLong(&opts->misc.disclogout, service, kSCPropNetPPPDisconnectOnLogout, 0, 0xFFFFFFFF, 1);
    setFromLong(&opts->misc.connlogout, service, CFSTR("AllowConnectWhenLogout"), 0, 0xFFFFFFFF, 1);
    if (getNumber(service, kSCPropNetPPPDisconnectOnIdle, &lval) && lval)
        setFromLong(&opts->comm.idletimer, service, kSCPropNetPPPDisconnectOnIdleTimer, 0, 0xFFFFFFFF, 1);
    if (getNumber(service, kSCPropNetPPPCommRedialEnabled, &lval) && lval) {
        setFromLong(&opts->comm.redialcount, service, kSCPropNetPPPCommRedialCount, 0, 0xFFFFFFFF, 1);
        setFromLong(&opts->comm.redialinterval, service, kSCPropNetPPPCommRedialInterval, 0, 0xFFFFFFFF, 1);
    }

    // other options
    setFromLong(&opts->misc.verboselog, service, kSCPropNetPPPVerboseLogging, 0, 0xFFFFFFFF, 1);
    setFromString(&opts->misc.logfile, service, kSCPropNetPPPLogfile);
    setFromLong(&opts->misc.autoconnect, service, kSCPropNetPPPDialOnDemand, 0, 1, 1);

#if 0
    STRING_DECL kSCPropNetPPPReminderTimer;              /* CFNumber */
    STRING_DECL kSCPropNetPPPAlert;                      /* CFArray[CFString] */
    /* kSCPropNetPPPAlert values */
    STRING_DECL kSCValNetPPPAlertPassword;               /* CFString */
    STRING_DECL kSCValNetPPPAlertReminder;               /* CFString */
    STRING_DECL kSCValNetPPPAlertStatus;                 /* CFString */
#endif

}

/* -----------------------------------------------------------------------------
this part is dedicated to pppoe interfaces
that's just a place holder until we have generic configurator in place
----------------------------------------------------------------------------- */
void fill_pppoe_options(CFDictionaryRef service, struct options *opts)
{

    //setFromString(&opts->dev.name, service, kSCPropNetPPPoEPortName);
}

/* -----------------------------------------------------------------------------
this part is dedicated to tty interfaces
----------------------------------------------------------------------------- */
void fill_tty_options(CFDictionaryRef service, struct options *opts)
{
    u_long	lval;
    CFStringRef	ref;
    
    //setFromString(&opts->dev.name, service, kSCPropNetModemPortName);
    setFromLong(&opts->dev.speed, service, kSCPropNetModemSpeed, 0, 0xFFFFFFFF, 1);
    setFromString(&opts->dev.connectscript, service, kSCPropNetModemConnectionScript);

    ref  = CFDictionaryGetValue(service, kSCPropNetModemDialMode);
    if (ref) {
        lval = 0;
        if (CFStringCompare(ref, kSCValNetModemDialModeIgnoreDialTone, 0) == kCFCompareEqualTo) 
            lval = 1;
        if (CFStringCompare(ref, kSCValNetModemDialModeManual, 0) == kCFCompareEqualTo)
            lval = 2;
        set_long_opt(&opts->dev.dialmode, lval, 0, 0xFFFFFFFF, 1);
    }
    setFromLong(&opts->dev.pulse, service, kSCPropNetModemPulseDial, 0, 0xFFFFFFFF, 1);
    setFromLong(&opts->dev.speaker, service, kSCPropNetModemSpeaker, 0, 0xFFFFFFFF, 1);
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
* Function: parse_component
* Purpose:
*   Given a string 'key' and a string prefix 'prefix',
*   return the next component in the slash '/' separated
*   key.  If no slash follows the prefix, return NULL.
*
* Examples:
* 1. key = "a/b/c" prefix = "a/"
*    returns "b"
* 2. key = "a/b/c" prefix = "a/b/"
*    returns NULL
----------------------------------------------------------------------------- */
static CFStringRef parse_component(CFStringRef key, CFStringRef prefix)
{
    CFMutableStringRef	comp;
    CFRange			range;

    if (CFStringHasPrefix(key, prefix) == FALSE) {
            return NULL;
    }
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
CFArrayRef getEntityAll(SCDSessionRef session, CFStringRef entity)
{
    CFStringRef		prefix = NULL, key = NULL;
    CFMutableArrayRef	all	= NULL;
    CFArrayRef		arr	= NULL;
    int			i;
    SCDStatus		status;

    prefix = SCDKeyCreate(CFSTR("%@/%@/%@/"), kSCCacheDomainSetup, kSCCompNetwork, kSCCompService);
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup, kSCCompAnyRegex, entity);

    SCDLock(session);

    all = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    status = SCDList(session, key, kSCDRegexKey, &arr);
    if (status != SCD_OK || CFArrayGetCount(arr) == 0)
        goto done;

    for (i = 0; i < CFArrayGetCount(arr); i++) {

        CFMutableDictionaryRef	ent_dict	= NULL;
        CFStringRef 		ent_key		= CFArrayGetValueAtIndex(arr, i);
        CFStringRef		serviceID	= NULL;

        serviceID = parse_component(ent_key, prefix);
        if (serviceID == NULL) 
            goto loop_done;
        
        ent_dict = (CFMutableDictionaryRef)getEntity(session, serviceID, entity);
        if (ent_dict == NULL) 
            goto loop_done;
        
        // need to stuff the service id as a special property
        CFDictionarySetValue(ent_dict, CFSTR("SERVICEID"), serviceID);
        
        CFArrayAppendValue(all, ent_dict);

loop_done:
        my_CFRelease(ent_dict);
        my_CFRelease(serviceID);
    }

done:
    my_CFRelease(prefix);
    my_CFRelease(key);
    my_CFRelease(arr);
    if (all) {
        if (CFArrayGetCount(all) == 0) {
            CFRelease(all);
            all = 0;
        }
    }
    SCDUnlock(session);
    return all;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFTypeRef getEntity(SCDSessionRef session, CFStringRef serviceID, CFStringRef entity)
{
    CFTypeRef		data = NULL;
    CFStringRef		key;
    SCDHandleRef	handle = NULL;

    if (entity) 
        key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup, serviceID, entity);
    else 
        key = SCDKeyCreate(CFSTR("%@/%@/%@/%@"), kSCCacheDomainSetup, kSCCompNetwork, kSCCompService, serviceID);
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
----------------------------------------------------------------------------- */
#if 0
CFStringRef getStringProperty(SCDSessionRef session, CFStringRef serviceID, CFStringRef entity, CFStringRef property)
{
    CFStringRef		prop = NULL;
    CFDictionaryRef	dict;
    
    dict = getEntity(session, serviceID, entity);
    if (dict) { 
        prop = CFDictionaryGetValue(dict, property);
        if (prop)
            prop = CFStringCreateCopy(NULL, prop);
        CFRelease(dict);
    }
    return prop;
}
#endif
/* -----------------------------------------------------------------------------
get a number from the dictionnary, in service/property
----------------------------------------------------------------------------- */
u_int8_t getNumber(CFDictionaryRef service, CFStringRef property, u_int32_t *outval)
{
    CFNumberRef		ref;

    ref  = CFDictionaryGetValue(service, property);
    if (ref && (CFNumberGetType(ref) == kCFNumberSInt32Type)) {
        CFNumberGetValue(ref, kCFNumberSInt32Type, outval);
        return 1;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
get a string from the dictionnary, in service/property
----------------------------------------------------------------------------- */
u_int8_t getString(CFDictionaryRef service, CFStringRef property, u_char *str, u_int16_t maxlen)
{
    CFStringRef		string;
    CFDataRef		ref;

    str[0] = 0;
    ref  = CFDictionaryGetValue(service, property);
    if (ref) {
        if (CFGetTypeID(ref) == CFStringGetTypeID()) {
           CFStringGetCString((CFStringRef)ref, str, maxlen, kCFStringEncodingUTF8 /*kCFStringEncodingMacRoman*/);
            return 1;
        }
        else if (CFGetTypeID(ref) == CFDataGetTypeID()) {
           string = CFStringCreateWithCharacters(NULL, 
                    (UniChar *)CFDataGetBytePtr(ref), CFDataGetLength(ref)/sizeof(UniChar));               	    
           if (string) {
                CFStringGetCString(string, str, maxlen, kCFStringEncodingUTF8 /*kCFStringEncodingMacRoman*/);
                CFRelease(string);
            	return 1;
            }
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void setFromString(struct opt_str *dst, CFDictionaryRef service, CFStringRef property)
{
    u_char	str[OPT_STR_LEN];

    if (getString(service, property, str, sizeof(str)))
        set_str_opt(dst, str, strlen(str));
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void setFromLong(struct opt_long *dst, CFDictionaryRef service, CFStringRef property, u_long min, u_long max, u_long def)
{
    u_int32_t  	lval;

    if (getNumber(service, property, &lval))
        set_long_opt(dst, lval, min, max, def);
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
void display_str_opt (u_char *name, struct opt_str *option)
{

    if (option->set)
        printf("ppp option %s = '%s'\n", name, option->str);
    else 
        printf("ppp option %s = %s\n", name, "--- NOT SET ---");
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void display_long_opt (u_char *name, struct opt_long *option)
{
    if (option->set)
        printf("ppp option %s = '%ld'\n", name, option->val);
    else 
        printf("ppp option %s = %s\n", name, "--- NOT SET ---");
}


