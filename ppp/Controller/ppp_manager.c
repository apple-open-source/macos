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
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/route.h>
#include <net/dlil.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#include <mach/mach_time.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDPlugin.h>
//#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#define SCLog

#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "../Family/ppp_domain.h"

#include "ppp_client.h"
#include "ppp_option.h"
#include "ppp_command.h"
#include "ppp_manager.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

TAILQ_HEAD(, ppp) 	ppp_head;
io_connect_t		gIOPort;
io_connect_t		gSleeping;
long			gSleepArgument;
CFUserNotificationRef 	gSleepNotification;
double	 		gTimeScaleSeconds;

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static void ppp_display_error(struct ppp *ppp);
static void ppp_sleep(void * x, io_service_t y, natural_t messageType, void *messageArgument);
static void exec_callback(pid_t pid, int status, struct rusage *rusage, void *context);

u_int32_t ppp_translate_error(u_int16_t subtype, u_int32_t native_ppp_error, u_int32_t native_dev_error);
int get_str (struct ppp *ppp, struct opt_str *option, 
        CFStringRef entity, CFStringRef property,
        u_char *opt, u_int32_t *outlen, CFDictionaryRef setupdict, u_char lookinsetup, u_char *defaultval);

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_init_all() {

    IONotificationPortRef	notify;
    io_object_t			iterator;
    mach_timebase_info_data_t   timebaseInfo;

    gSleeping = 0;	// obviously we are not sleeping :-)
    gSleepNotification = 0;    
    
    TAILQ_INIT(&ppp_head);

    /* install the power management callback */
    gIOPort = IORegisterForSystemPower(0, &notify, ppp_sleep, &iterator);
    if (gIOPort == 0) {
        printf("IORegisterForSystemPower failed\n");
        return 1;
    }
    
    if (mach_timebase_info(&timebaseInfo) != KERN_SUCCESS) {	// returns scale factor for ns
        printf("mach_timebase_info failed\n");
        return 1;
    }
    gTimeScaleSeconds = ((double) timebaseInfo.numer / (double) timebaseInfo.denom) / 1000000000;

    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                        IONotificationPortGetRunLoopSource(notify),
                        kCFRunLoopDefaultMode);
                        
    /* read configuration from database */
    if (options_init_all()) {
        return 1;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_dispose_all()
{

    // dispose ppp data structures
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int32_t ppp_makeref(struct ppp *ppp)
{
    return (((u_long)ppp->subtype) << 16) + ppp->unit;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int32_t ppp_makeifref(struct ppp *ppp)
{
    return (((u_long)ppp->subtype) << 16) + ppp->ifunit;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct ppp *ppp_findbyname(u_char *name, u_short ifunit)
{
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((ppp->ifunit == ifunit)
            && !strncmp(ppp->name, name, IFNAMSIZ)) {
            return ppp;
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct ppp *ppp_findbyserviceID(CFStringRef serviceID)
{
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) 
        if (CFEqual(ppp->serviceID, serviceID)) 
            return ppp;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct ppp *ppp_findbysid(u_char *data, int len)
{
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) 
        if (ppp->sid && !strncmp(ppp->sid, data, len)) 
            return ppp;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_setorder(struct ppp *ppp, u_int16_t order)
{

    TAILQ_REMOVE(&ppp_head, ppp, next);
    switch (order) {
        case 0:
            TAILQ_INSERT_HEAD(&ppp_head, ppp, next);
            break;
        case 0xFFFF:
            TAILQ_INSERT_TAIL(&ppp_head, ppp, next);
            break;
        }
}

/* -----------------------------------------------------------------------------
find the ppp structure corresponding to the reference
if ref == -1, then return the default structure (first in the list)
if ref == 
----------------------------------------------------------------------------- */
struct ppp *ppp_findbyref(u_long ref)
{
    u_short		subtype = ref >> 16;
    u_short		unit = ref & 0xFFFF;
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (((ppp->subtype == subtype) || (subtype == 0xFFFF))
            &&  ((ppp->unit == unit) || (unit == 0xFFFF))) {
            return ppp;
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
find the ppp structure corresponding to the message
----------------------------------------------------------------------------- */
struct ppp *ppp_find(struct msg *msg)
{
    
    if (msg->hdr.m_flags & USE_SERVICEID)
        return ppp_findbysid(msg->data, msg->hdr.m_link);
    else
        return ppp_findbyref(msg->hdr.m_link);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_printlist()
{
    struct ppp		*ppp;

    SCLog(TRUE, LOG_INFO, CFSTR("Printing list of ppp services : \n"));
    TAILQ_FOREACH(ppp, &ppp_head, next) {
        SCLog(TRUE, LOG_INFO, CFSTR("Service : %@, subtype = %d\n"), ppp->serviceID, ppp->subtype);
    }
}

/* -----------------------------------------------------------------------------
find the ppp structure corresponding to the reference
if ref == -1, then return the default structure (first in the list)
if ref == 
----------------------------------------------------------------------------- */
struct ppp *ppp_findbyifref(u_long ref)
{
    u_short		subtype = ref >> 16;
    u_short		ifunit = ref & 0xFFFF;
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (((ppp->subtype == subtype) || (subtype == 0xFFFF))
            &&  ((ppp->ifunit == ifunit) || (ifunit == 0xFFFF))) {
            return ppp;
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
get the first free ref numer within a family
----------------------------------------------------------------------------- */
u_short ppp_findfreeunit(u_short subtype)
{
    struct ppp		*ppp = TAILQ_FIRST(&ppp_head);
    u_short		unit = 0;

    while (ppp) {
    	if ((subtype == ppp->subtype)
            && (ppp->unit == unit)) {
            unit++;
            if (unit == 0xFFFF)
                return unit;
            ppp = TAILQ_FIRST(&ppp_head); // restart
        }
        else 
            ppp = TAILQ_NEXT(ppp, next); // continue
    }

    return unit;
}

/* -----------------------------------------------------------------------------
an interface structure needs to be created
unit is the ppp managed unit, not the ifunit 
----------------------------------------------------------------------------- */
struct ppp *ppp_new(CFStringRef serviceID , CFStringRef subtypeRef)
{
    struct ppp 		*ppp;
    u_short 		unit, subtype, len;
    CFURLRef		url;
    u_char 		str[MAXPATHLEN], str2[32];

    //SCLog(LOG_INFO, CFSTR("ppp_new, subtype = %%@, serviceID = %@\n"), subtypeRef, serviceID);

    if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPSerial)) 
        subtype = PPP_TYPE_SERIAL;
    else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPoE))
        subtype = PPP_TYPE_PPPoE;
    else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPTP))
        subtype = PPP_TYPE_PPTP;
    else if (CFEqual(subtypeRef, CFSTR("L2TP")/*kSCValNetInterfaceSubTypeL2TP*/))
        subtype = PPP_TYPE_L2TP;
//  else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPoA))
//      subtype = PPP_TYPE_PPPoA;
//  else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPISDN))
//      subtype = PPP_TYPE_ISDN;
//  else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPOther))
//      subtype = PPP_TYPE_OTHER;
    else 
        subtype = PPP_TYPE_OTHER;

    unit = ppp_findfreeunit(subtype);
    if (unit == 0xFFFF)
        return 0;	// no room left...
        
    ppp = malloc(sizeof(struct ppp));
    if (!ppp)
        return 0;	// very bad...

    bzero(ppp, sizeof(struct ppp));

    ppp->serviceID = CFRetain(serviceID);
    ppp->subtypeRef = CFRetain(subtypeRef);
    // keep a C version of the service ID
    len = CFStringGetLength(serviceID) + 1;
    if (ppp->sid = malloc(len)) {
        CFStringGetCString(serviceID, ppp->sid, len, kCFStringEncodingUTF8);
    }
    
    ppp->unit = unit;
    ppp->ifunit = 0xFFFF;		// no real unit yet
    strncpy(ppp->name, "ppp", IFNAMSIZ);
    ppp->subtype = subtype;
    ppp->phase = PPP_IDLE;
    ppp->alertenable = OPT_ALERT_DEF;

    strcpy(str, DIR_KEXT);
    str2[0] = 0;
    CFStringGetCString(ppp->subtypeRef, str2, sizeof(str2), kCFStringEncodingUTF8);
    strcat(str, str2);
    strcat(str, ".ppp");	// add plugin suffix
    url = CFURLCreateFromFileSystemRepresentation(NULL, str, strlen(str), TRUE);
    if (url) {
        ppp->bundle = CFBundleCreate(0, url);
        CFRelease(url);
    }

    TAILQ_INSERT_TAIL(&ppp_head, ppp, next);

    return ppp;
}

/* -----------------------------------------------------------------------------
changed for this ppp occured in configd cache
----------------------------------------------------------------------------- */
void ppp_updatestate(struct ppp *ppp, CFDictionaryRef service)
{
    u_int32_t 	lval;
    int		val;
    u_char	str[16];
    struct ppp 	*ppp2;

    if (!getNumber(service, kSCPropNetPPPStatus, &lval)) {

        // just started and status not yet published
        if (ppp->started_link == 0 || lval) {
                    
            ppp->started_link = 0;
            ppp->phase = lval;
        
            //CFStringGetCString(ppp->serviceID, str, sizeof(str), kCFStringEncodingUTF8);
            //printf("service '%s', state = %d\n", str, lval);
            
            ppp->ifunit = 0xFFFF;
            if (ppp->phase == PPP_RUNNING || ppp->phase == PPP_ONHOLD || ppp->phase == PPP_STATERESERVED) {
                // should get the interfce name at the right place 
                if (!getString(service, kSCPropInterfaceName, str, sizeof(str))) {
                    sscanf(str, "ppp%d", &val);
                    ppp->ifunit = val;
                }
            }
        }
    }

    if (ppp->kill_link) {
        //CFStringGetCString(ppp->serviceID, str, sizeof(str), kCFStringEncodingUTF8);
        //printf("update state, need kill link for service '%s'\n", str);
        val = ppp->kill_link;
        ppp->kill_link = 0;
        ppp_dodisconnect(ppp, val);
    }

    if (ppp->needdispose && !ppp_dispose(ppp))
        return;

    if (getNumber(service, kSCPropNetPPPDeviceLastCause, &ppp->lastdevstatus))
        ppp->lastdevstatus = 0;
        
    if (getNumber(service, kSCPropNetPPPLastCause, &ppp->laststatus))
        ppp->laststatus = 0;
    
    if (getNumber(service, kSCPropNetPPPConnectTime, &ppp->conntime)) 
        ppp->conntime = 0;

    if (getNumber(service, CFSTR("DisconnectTime")/*kSCPropNetPPPDisconnectTime*/, &ppp->disconntime))
        ppp->disconntime = 0;
        
    if (ppp->phase != ppp->oldphase) {
        switch (ppp->phase) {
 	    case PPP_STATERESERVED:
 	    case PPP_HOLDOFF:
	      // forget about the last error when we rearm the dial on demand
	      if (ppp->oldphase != PPP_INITIALIZE && ppp->oldphase != PPP_HOLDOFF)
                ppp_display_error(ppp);
	      break;
 
            case PPP_IDLE:
                ppp->kill_sent = 0;
	      if (ppp->oldphase != PPP_STATERESERVED && ppp->oldphase != PPP_HOLDOFF)
                    ppp_display_error(ppp);

                if (gSleeping) {
                    TAILQ_FOREACH(ppp2, &ppp_head, next)
                        if (ppp2->phase != PPP_IDLE)
                            break;
                    if (ppp2 == 0) {
                        if (gSleepNotification) {
                            CFUserNotificationCancel(gSleepNotification);
                            CFRelease(gSleepNotification);
                            gSleepNotification = 0;
                        }
                        IOAllowPowerChange(gIOPort, gSleepArgument);
                    }
                }
                
                if (ppp->needconnect) {
                    ppp->needconnect = 0;
                    ppp_doconnect(ppp, ppp->needconnectopts, 0);
                    if (ppp->needconnectopts) {
                        free(ppp->needconnectopts);
                        ppp->needconnectopts = 0;
                    }
                }
                else {
                    // check if last error != MAXFAIL
                    if ((!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                        kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &lval) && lval)
                         && (getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                            kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &lval) || !lval
                            || gLoggedInUser)) {
                        ppp_doconnect(ppp, 0, 1);
                    }
                }
                break;
        }
        ppp->oldphase = ppp->phase;
    }    

    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFStringRef ppp_copyUserLocalizedString(CFBundleRef bundle,
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
void ppp_display_error(struct ppp *ppp) 
{
    CFStringRef 	ppp_msg = NULL, dev_msg = NULL, msg = NULL;
    CFPropertyListRef	langRef = NULL;

    if ((ppp->alertenable & PPP_ALERT_ERRORS) == 0)
        return;

    if (gLoggedInUser) {
	langRef = CFPreferencesCopyValue(CFSTR("AppleLanguages"), 
            kCFPreferencesAnyApplication, gLoggedInUser, kCFPreferencesAnyHost);
    }

    if (ppp->lastdevstatus && ppp->bundle) {            
        dev_msg = CFStringCreateWithFormat(0, 0, CFSTR("Device Error %d"), ppp->lastdevstatus);
        if (dev_msg)
            msg = ppp_copyUserLocalizedString(ppp->bundle, dev_msg, dev_msg, langRef);
    }
    
    if (msg == NULL) {
        ppp_msg = CFStringCreateWithFormat(0, 0, CFSTR("PPP Error %d"), ppp->laststatus);
        if (ppp_msg)
            msg = ppp_copyUserLocalizedString(gBundleRef, ppp_msg, ppp_msg, langRef) ;
    }

    if (msg && CFStringGetLength(msg))
        CFUserNotificationDisplayNotice(120, kCFUserNotificationNoteAlertLevel, 
            gIconURLRef, NULL, gBundleURLRef, CFSTR("Internet Connect"), msg, NULL);

    if (msg) CFRelease(msg);
    if (ppp_msg) CFRelease(ppp_msg);
    if (dev_msg) CFRelease(dev_msg);
    if (langRef) CFRelease(langRef);
}

/* -----------------------------------------------------------------------------
changed for this ppp occured in configd cache
----------------------------------------------------------------------------- */
void ppp_updatesetup(struct ppp *ppp, CFDictionaryRef service)
{

    // delay part or all the setup once all the notification for the service have been received
    ppp->dosetup = 1;

    return;
}

/* -----------------------------------------------------------------------------
changed for this ppp occured in configd cache
----------------------------------------------------------------------------- */
void ppp_postupdatesetup()
{
    u_int32_t 	lval;
    struct ppp 	*ppp;
    
    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (ppp->dosetup) {
            ppp->dosetup = 0;
            ppp->needdispose = 0;
            switch (ppp->phase) {
                case PPP_IDLE:
                    //printf("ppp_updatesetup : unit %d, PPP_IDLE\n", ppp->unit);
                    if ((!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                        kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &lval) && lval)
                         && (getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                            kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &lval) || !lval
                            || gLoggedInUser)) {
                            ppp_doconnect(ppp, 0, 1);
                    }
                    break;
                case PPP_STATERESERVED:
                case PPP_HOLDOFF:
                    ppp_dodisconnect(ppp, 15);
                    break;
                default :
                    //printf("ppp_updatesetup : unit %d, NOT IDLE, state = %d\n", ppp->unit, ppp->phase);
                    //ppp_dodisconnect(ppp, 15);
                    ;
            }
	}
    }

    return;
}

/* -----------------------------------------------------------------------------
call back from power management
----------------------------------------------------------------------------- */
void ppp_sleep(void * x, io_service_t y, natural_t messageType, void *messageArgument)
{
    CFMutableDictionaryRef 	dict;
    SInt32 			error;
    u_int32_t			disc, allow, alerte, lval;
    CFDictionaryRef		service = NULL;
    struct ppp 			*ppp;

    //printf("messageType %08lx, arg %08lx\n",(long unsigned int)messageType, (long unsigned int)messageArgument);
    
    switch ( messageType ) {
    
        case kIOMessageSystemWillSleep:
            gSleeping  = 1;	// time to sleep
            allow = 1;
            alerte = 0;
            TAILQ_FOREACH(ppp, &ppp_head, next) {
	       if (ppp->phase != PPP_IDLE
                    // by default, disconnect on sleep
                    && (getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                        kSCEntNetPPP, kSCPropNetPPPDisconnectOnSleep, &disc)
			|| disc	
                        || ppp->phase == PPP_STATERESERVED 
                        || ppp->phase == PPP_HOLDOFF)) { 
                    allow = 0;
                    if (ppp->phase != PPP_STATERESERVED && ppp->phase != PPP_HOLDOFF)
                        alerte = 1;
                    ppp_dodisconnect(ppp, 15);
                }
            }
            if (allow) {
                IOAllowPowerChange(gIOPort, (long)messageArgument);
                break;
            }
            
            gSleepArgument = (long)messageArgument;    
           
            if (alerte) {
                dict = CFDictionaryCreateMutable(NULL, 0, 
                    &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                if (dict) {
                    CFDictionaryAddValue(dict, kCFUserNotificationIconURLKey, gIconURLRef);
                    CFDictionaryAddValue(dict, kCFUserNotificationLocalizationURLKey, gBundleURLRef);
                    CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, CFSTR("Internet Connect"));
                    CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, CFSTR("Waiting for disconnection"));
                    gSleepNotification = CFUserNotificationCreate(NULL, 0, 
                                        kCFUserNotificationNoteAlertLevel 
                                        + kCFUserNotificationNoDefaultButtonFlag, &error, dict);
                }
            }
            break;

        case kIOMessageCanSystemSleep:
            // I refuse idle sleep if ppp is connected
            TAILQ_FOREACH(ppp, &ppp_head, next) {
                if (ppp->phase != PPP_IDLE
                    && ppp->phase != PPP_STATERESERVED 
                    && ppp->phase != PPP_HOLDOFF) {
                    IOCancelPowerChange(gIOPort, (long)messageArgument);
                    return;
                }
            }
            IOAllowPowerChange(gIOPort, (long)messageArgument);
            break;

        case kIOMessageSystemWillNotSleep:
            /* should get it only if someone refuse an idle sleep 
                but I don't have anything to do here */
            break;
            
        case kIOMessageSystemHasPoweredOn:
            gSleeping = 0; // time to wakeup
            if (gSleepNotification) {
                CFUserNotificationCancel(gSleepNotification);
                CFRelease(gSleepNotification);
                gSleepNotification = 0;
            }
            TAILQ_FOREACH(ppp, &ppp_head, next) {
                service = copyEntity(kSCDynamicStoreDomainState, ppp->serviceID, kSCEntNetPPP);
                if (service) {
                    ppp_updatestate(ppp, service);
                    CFRelease(service);
                    if (ppp->phase == PPP_IDLE) {
                        if ((!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                            kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &lval) && lval)
                            && (getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                                kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &lval) || !lval
                                || gLoggedInUser)) {
                     
                                ppp_doconnect(ppp, 0, 1);
                        }
                    }
                }
            }
            break;
    }
    
}

/* -----------------------------------------------------------------------------
user has looged out
need to check the disconnect on logout flag for the ppp interfaces
----------------------------------------------------------------------------- */
int ppp_logout()
{
    struct ppp		*ppp;
    u_int32_t		disc;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (ppp->phase != PPP_IDLE
            && !getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &disc) 
            && disc)
            ppp_dodisconnect(ppp, 15);
    }
    return 0;
}

/* -----------------------------------------------------------------------------
user has logged in
need to check the dialondemand flag again
----------------------------------------------------------------------------- */
int ppp_login()
{
    struct ppp		*ppp;
    u_int32_t		val;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (ppp->phase == PPP_IDLE
            && !getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &val) 
            && val)
            ppp_doconnect(ppp, 0, 1);
    }
    return 0;
}

/* -----------------------------------------------------------------------------
an interface is come down, dispose the ppp structure
unit is the ppp managed unit, not the ifunit 
----------------------------------------------------------------------------- */
int ppp_dispose(struct ppp *ppp)
{

    // need to close the protocol first
    ppp_dodisconnect(ppp, 15);

    if (ppp->phase != PPP_IDLE) {
        ppp->needdispose = 1;
        return 1;
    }
    
    TAILQ_REMOVE(&ppp_head, ppp, next);    

    // then free the structure
    if (ppp->sid)
        free(ppp->sid);
    if (ppp->bundle) 
        CFRelease(ppp->bundle);
    CFRelease(ppp->serviceID);
    CFRelease(ppp->subtypeRef);
    free(ppp);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void addparam(char **arg, u_int32_t *argi, char *param)
{
    int len = strlen(param);

    if (len && (arg[*argi] = malloc(len + 1))) {
        strcpy(arg[*argi], param);
        (*argi)++;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_triggerdemand(struct ppp *ppp)
{
    struct ifreq	ifr;
    int 		s, i;
    //char str[16];

    // CFStringGetCString(ppp->serviceID, str, sizeof(str), kCFStringEncodingUTF8);
    // printf("service '%s', ppp_triggerdemand\n", str);

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return errno;

    memset (&ifr, 0, sizeof (ifr));
    sprintf(ifr.ifr_name, "ppp%d", ppp->ifunit);
    if (ioctl(s, OSIOCGIFDSTADDR, &ifr) < 0) {
        close(s);
        return errno;
    }

    // just send a byte out there
    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_port = 1;
    i = sendto(s, &s, 1, 0, (struct sockaddr *)&ifr.ifr_addr, sizeof(struct sockaddr_in));     
    close(s);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_doconnect(struct ppp *ppp, struct options *opts, u_int8_t dialondemand)
{
#define MAXARG 100
    char 			str[MAXPATHLEN], str2[256], *cmdarg[MAXARG];
    int 			needpasswd = 0;
    u_char 			sopt[OPT_STR_LEN];
    u_int32_t			len, lval, lval1, i, argi = 0;
    CFDictionaryRef		service = NULL;
    CFArrayRef			array = NULL;
    CFStringRef			string = NULL;

    if (gSleeping) 
        return EIO;	// not the right time to dial

    switch (ppp->phase) {
        case PPP_IDLE:
            break;
        case PPP_STATERESERVED:	// kill dormant process and post connection flag
        case PPP_HOLDOFF:
            //return ppp_triggerdemand(ppp);
            // since pppd is in dormant phase, it should immediatly quit.
            // FIx me : there is a small window of conflict where pppd is changing state, but it's 
            // not yet published
            if (opts) {
                ppp->needconnectopts = malloc(sizeof(struct options));
                if (!ppp->needconnectopts) {
                    ppp->laststatus = EXIT_FATAL_ERROR;
                    ppp->lastdevstatus = 0;
                    return EXIT_FATAL_ERROR;
                }
                bcopy(opts, ppp->needconnectopts, sizeof(struct options));
            }
            ppp_dodisconnect(ppp, 15);
            ppp->needconnect = 1;
            ppp->phase = PPP_INITIALIZE;
            return 0;
            break;
        default:
            return EIO;	// not the right time to dial
    }

    service = copyEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, kSCEntNetPPP);
    if (!service)
        return 0;

    argi = 0;
    for (i = 0; i < MAXARG; i++) {
        cmdarg[i] = 0;
    }
        
    addparam(cmdarg, &argi, PPPD_PRGM);

/* ************************************************************************* */
    /* first Service ID options */

    CFStringGetCString(ppp->serviceID, str, sizeof(str), kCFStringEncodingUTF8);
    addparam(cmdarg, &argi, "serviceid");
    addparam(cmdarg, &argi, str);

/* ************************************************************************* */
    /* then some basic admin options */
    
    // add the dialog plugin
    if (gPluginsDir) {
        addparam(cmdarg, &argi, "plugin");
        CFStringGetCString(gPluginsDir, str, sizeof(str), kCFStringEncodingUTF8);
        strcat(str, "PPPDialogs.ppp");
        addparam(cmdarg, &argi, str);
    }

    if (!getNumber(service, kSCPropNetPPPVerboseLogging, &lval) && lval)
        addparam(cmdarg, &argi, "debug");
        
    ppp_getoptval(ppp, opts, PPP_OPT_ALERTENABLE, &ppp->alertenable, &len, service);
                
/* ************************************************************************* */

    if (!ppp_getoptval(ppp, opts, PPP_OPT_LOGFILE, sopt, &len, service) && sopt[0]) {
        // if logfile start with /, it's a full path
        // otherwise it's relative to the logs folder (convention)
        // we also strongly advise to name the file with the link number
        // for example ppplog0
        // the default path is /var/log
        // it's useful to have the debug option with the logfile option
        // with debug option, pppd will log the negociation
        // debug option is different from kernel debug trace

        addparam(cmdarg, &argi, "logfile");

        sprintf(str, "%s%s", sopt[0] == '/' ? "" : DIR_LOGS, sopt);
        addparam(cmdarg, &argi, str);
    }

/* ************************************************************************* */
    /* then device specific options */    

    /* connect plugin parameter */    
    addparam(cmdarg, &argi, "plugin");
    CFStringGetCString(ppp->subtypeRef, str2, sizeof(str2) - 4, kCFStringEncodingUTF8);
    strcat(str2, ".ppp");	// add plugin suffix
    addparam(cmdarg, &argi, str2);
        
    if (!ppp_getoptval(ppp, opts, PPP_OPT_DEV_NAME, sopt, &len, service) && sopt[0]) {
        addparam(cmdarg, &argi, "device");
        addparam(cmdarg, &argi, sopt);
    }

    if (!ppp_getoptval(ppp, opts, PPP_OPT_DEV_SPEED, &lval, &len, service) && lval) {
        sprintf(str, "%d", lval);
        addparam(cmdarg, &argi, str);
    }
        
    switch (ppp->subtype) {
    
    case PPP_TYPE_SERIAL: 

        // the controller has a built-in knowledge of serial connections

        /*  PPPSerial currently supports Modem
            in case of Modem, the DeviceName key will contain the actual device,
            while the Modem dictionnary will contain the modem settings.
            if Hardware is undefined, or different from Modem, 
            we expect to find external configuration files. 
            (This is the case for ppp over ssh) 
        */
        if (getStringFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
            kSCEntNetInterface, kSCPropNetInterfaceHardware, sopt, OPT_STR_LEN) 
            || strcmp(sopt, "Modem")) {
                // we are done
                break;
        }
    
        if (!ppp_getoptval(ppp, opts, PPP_OPT_DEV_CONNECTSCRIPT, sopt, &len, service) && sopt[0]) {
            // ---------- connect script parameter ----------
            addparam(cmdarg, &argi, "modemscript");
            addparam(cmdarg, &argi, sopt);
            
            // add all the ccl flags
            lval = 1;
            getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetModem, kSCPropNetModemSpeaker, &lval) ;
            addparam(cmdarg, &argi, lval ? "modemsound" : "nomodemsound");
    
            lval = 1;
            getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetModem, kSCPropNetModemErrorCorrection, &lval);
            addparam(cmdarg, &argi, lval ? "modemreliable" : "nomodemreliable");

            lval = 1;
            getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetModem, kSCPropNetModemDataCompression, &lval);
            addparam(cmdarg, &argi, lval ? "modemcompress" : "nomodemcompress");

            lval = 0;
            getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetModem, kSCPropNetModemPulseDial, &lval);
            addparam(cmdarg, &argi, lval ? "modempulse" : "modemtone");
    
            // dialmode : 0 = normal, 1 = blind(ignoredialtone), 2 = manual
            lval = 0;
            ppp_getoptval(ppp, opts, PPP_OPT_DEV_DIALMODE, &lval, &len, service);
            addparam(cmdarg, &argi, "modemdialmode");
            sprintf(str, "%d", lval);
            addparam(cmdarg, &argi, str);
        }
        break;
        
    }
    
/* ************************************************************************* */
    /* then COMM options */

    if (!ppp_getoptval(ppp, opts, PPP_OPT_COMM_TERMINALMODE, &lval, &len, service)) {

        /* add the PPPSerial plugin if not already present
         Fix me : terminal mode is only supported in PPPSerial types of connection
         but subtype using ptys can use it the same way */    
        if (lval != PPP_COMM_TERM_NONE && ppp->subtype != PPP_TYPE_SERIAL) {
            addparam(cmdarg, &argi, "plugin");
            addparam(cmdarg, &argi, "PPPSerial.ppp");
        }

        if (lval == PPP_COMM_TERM_WINDOW) {
            addparam(cmdarg, &argi, "terminalwindow");
        }
        else if (lval == PPP_COMM_TERM_SCRIPT) {
            if (!ppp_getoptval(ppp, opts, PPP_OPT_COMM_TERMINALSCRIPT, sopt, &len, service) && sopt[0]) {
                addparam(cmdarg, &argi, "terminalscript");
                addparam(cmdarg, &argi, sopt);
            }
        }
    }

/* ************************************************************************* */

    // add generic phonenumber option
    if (!ppp_getoptval(ppp, opts, PPP_OPT_COMM_REMOTEADDR, sopt, &len, service) && sopt[0]) {
        addparam(cmdarg, &argi, "remoteaddress");
        sprintf(str, "%s", sopt);
        addparam(cmdarg, &argi, str);
    }
    
    // ---------- add the redial options ----------
    if (!getNumber(service, kSCPropNetPPPCommRedialEnabled, &lval) && lval) {
            
        if (!getString(service, kSCPropNetPPPCommAlternateRemoteAddress, sopt, sizeof(sopt)) && sopt[0]) {
            addparam(cmdarg, &argi, "altremoteaddress");
            sprintf(str, "%s", sopt);
            addparam(cmdarg, &argi, str);
        }
        
        if (!getNumber(service, kSCPropNetPPPCommRedialCount, &lval)) {
            addparam(cmdarg, &argi, "redialcount");
            sprintf(str, "%d", lval);
            addparam(cmdarg, &argi, str);
        }

        if (!getNumber(service, kSCPropNetPPPCommRedialInterval, &lval)) {
            addparam(cmdarg, &argi, "redialtimer");
            sprintf(str, "%d", lval);
            addparam(cmdarg, &argi, str);
        }
    }

/* ************************************************************************* */

    if (!ppp_getoptval(ppp, opts, PPP_OPT_COMM_IDLETIMER, &lval, &len, service) && lval) {
        addparam(cmdarg, &argi, "idle");
        sprintf(str, "%d", lval);
        addparam(cmdarg, &argi, str);
    }

    if (!ppp_getoptval(ppp, opts, PPP_OPT_COMM_SESSIONTIMER, &lval, &len, service) && lval) {
        addparam(cmdarg, &argi, "maxconnect");
        sprintf(str, "%d", lval);
        addparam(cmdarg, &argi, str);
    }
    
/* ************************************************************************* */
    /*  then MISC options */

    if (dialondemand &&
        !ppp_getoptval(ppp, opts, PPP_OPT_AUTOCONNECT, &lval, &len, service) && lval) {
        addparam(cmdarg, &argi, "demand");
        addparam(cmdarg, &argi, "holdoff");
        sprintf(str, "%d", 30);
        addparam(cmdarg, &argi, str);
    }

/* ************************************************************************* */
    /* then LCP options */

    // set echo option, so ppp hangup if we pull the modem cable
    // echo option is 2 bytes for interval + 2 bytes for failure
    if (!ppp_getoptval(ppp, opts, PPP_OPT_LCP_ECHO, &lval, &len, service) && lval) {
        if (lval >> 16) {
            addparam(cmdarg, &argi, "lcp-echo-interval");
            sprintf(str, "%d", lval >> 16);
            addparam(cmdarg, &argi, str);
        }
        if (lval & 0xffff) {
            addparam(cmdarg, &argi, "lcp-echo-failure");
            sprintf(str, "%d", lval & 0xffff);
            addparam(cmdarg, &argi, str);
        }
    }
    
/* ************************************************************************* */

    if (!ppp_getoptval(ppp, opts, PPP_OPT_LCP_HDRCOMP, &lval, &len, service)) {
        if (!(lval & 1))
            addparam(cmdarg, &argi, "nopcomp");
        if (!(lval & 2))
            addparam(cmdarg, &argi, "noaccomp");
    }

/* ************************************************************************* */

    if (!ppp_getoptval(ppp, opts, PPP_OPT_LCP_MRU, &lval, &len, service) && lval) {
        addparam(cmdarg, &argi, "mru");
        sprintf(str, "%d", lval);
        addparam(cmdarg, &argi, str);
    }

/* ************************************************************************* */

    if (!ppp_getoptval(ppp, opts, PPP_OPT_LCP_MTU, &lval, &len, service) && lval) {
        addparam(cmdarg, &argi, "mtu");
        sprintf(str, "%d", lval);
        addparam(cmdarg, &argi, str);
    }

/* ************************************************************************* */

    if (!ppp_getoptval(ppp, opts, PPP_OPT_LCP_RCACCM, &lval, &len, service) && lval) {
        addparam(cmdarg, &argi, "asyncmap");
        sprintf(str, "%d", lval);
        addparam(cmdarg, &argi, str);
    }
    else {
        addparam(cmdarg, &argi, "receive-all");
    }

/* ************************************************************************* */

     if (!ppp_getoptval(ppp, opts, PPP_OPT_LCP_TXACCM, &lval, &len, service) && lval) {
            addparam(cmdarg, &argi, "escape");
            str[0] = 0;
            for (lval1 = 0; lval1 < 32; lval1++) {
                if ((lval >> lval1) & 1) {
                    sprintf(str2, "%d,", lval1);
                    strcat(str, str2);
               }
            }
            str[strlen(str)-1] = 0; // remove last ','
            addparam(cmdarg, &argi, str);
       }

/* ************************************************************************* */
    /* then IPCP options */

#if 0
    if (!getString(service, CFSTR("IPCPUpScript"), str, sizeof(str)) && str[0]) {
        addparam(cmdarg, &argi, "ip-up");
        addparam(cmdarg, &argi, str);
    }

    if (!getString(service, CFSTR("IPCPDownScript"), str, sizeof(str)) && str[0]) {
        addparam(cmdarg, &argi, "ip-down");
        addparam(cmdarg, &argi, str);
    }
#endif
    
    if (!getStringFromEntity(kSCDynamicStoreDomainState, 0, 
        kSCEntNetIPv4, kSCPropNetIPv4Router, sopt, OPT_STR_LEN) && sopt[0]) {
        addparam(cmdarg, &argi, "ipparam");
        addparam(cmdarg, &argi, sopt);
    }
    
    // OverridePrimary option not handled yet in Setup by IPMonitor
    if (!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
        kSCEntNetIPv4, CFSTR("OverridePrimary"), &lval) && lval) {
        addparam(cmdarg, &argi, "defaultroute");
    }

/* ************************************************************************* */

     if (! (!ppp_getoptval(ppp, opts, PPP_OPT_IPCP_HDRCOMP, &lval, &len, service) && lval))
        addparam(cmdarg, &argi, "novj");

/* ************************************************************************* */

    if (!ppp_getoptval(ppp, opts, PPP_OPT_IPCP_LOCALADDR, &lval, &len, service) && lval)
        sprintf(str2, "%d.%d.%d.%d", lval >> 24, (lval >> 16) & 0xFF, (lval >> 8) & 0xFF, lval & 0xFF);
    else 
        strcpy(str2, "0");

    strcpy(str, str2);
    strcat(str, ":");
    if (!ppp_getoptval(ppp, opts, PPP_OPT_IPCP_REMOTEADDR, &lval, &len, service) && lval) 
        sprintf(str2, "%d.%d.%d.%d", lval >> 24, (lval >> 16) & 0xFF, (lval >> 8) & 0xFF, lval & 0xFF);
    else 
        strcpy(str2, "0");
    strcat(str, str2);
    addparam(cmdarg, &argi, str);

    addparam(cmdarg, &argi, "noipdefault");
    addparam(cmdarg, &argi, "ipcp-accept-local");
    addparam(cmdarg, &argi, "ipcp-accept-remote");

/* ************************************************************************* */

    // usepeerdns is there is a no dns value
    // setting 0 in the dns field will disable the usepeerdns option
    if (getAddressFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
        kSCEntNetDNS, kSCPropNetDNSServerAddresses, &lval))
        addparam(cmdarg, &argi, "usepeerdns");

/* ************************************************************************* */
    /* then AUTH options */

    // don't want authentication on our side...
    addparam(cmdarg, &argi, "noauth");

     if (!ppp_getoptval(ppp, opts, PPP_OPT_AUTH_PROTO, &lval, &len, service) && (lval != PPP_AUTH_NONE)) {
        if (!ppp_getoptval(ppp, opts, PPP_OPT_AUTH_NAME, sopt, &len, service) && sopt[0]) {
            addparam(cmdarg, &argi, "user");
            addparam(cmdarg, &argi, sopt);

            if (!get_str(ppp, opts ? &opts->auth.passwd : 0, kSCEntNetPPP, kSCPropNetPPPAuthPassword, sopt, &len, service, 1, "") && sopt[0]) {
                addparam(cmdarg, &argi, "password");
                addparam(cmdarg, &argi, sopt);
            }
            else { 
                /* we need to prompt for a password if we have a username and no saved password */
                needpasswd = 1;
            }
        }
    }

/* ************************************************************************* */

    // we want pppd to detach.
    // that 's the default behavior, be we don't want to block configd 
    // if the user has setup bad config files
     addparam(cmdarg, &argi, "forcedetach");

    if (!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPCCPEnabled, &lval) && lval
        
        // Fix me : to enforce use of MS-CHAP, refuse any alteration of default auth proto 
        // a dialer specifying PAP or CHAP will works without CCP/MPPE
        // even is CCP is enabled in the configuration.
        // Will be revisited when addition compression modules and
        // authentication modules will be added 
        && !ppp_getoptval(ppp, opts, PPP_OPT_AUTH_PROTO, &lval, &len, service) 
                && (lval == OPT_AUTH_PROTO_DEF)) {

        // Fix me : mppe is the only currently supported compression 
        // if the CCPAccepted and CCPRequired array are not there, 
        // assume we accept all types of compression we support

        addparam(cmdarg, &argi, "mppe-stateless");
        addparam(cmdarg, &argi, "mppe-128");        
        addparam(cmdarg, &argi, "mppe-40");        

        // Fix me : also enforce the use of MS-CHAP
        // A better authentication protocol needs to be implemented 
        // this will also be necessary for good EAP support 
        addparam(cmdarg, &argi, "refuse-pap");
        addparam(cmdarg, &argi, "refuse-chap-md5");
    }
    else {
        // no compression protocol
        addparam(cmdarg, &argi, "noccp");	
    }
    
    // loop local traffic destined to the local ip address
    addparam(cmdarg, &argi, "looplocal");       

    if (!(ppp->alertenable & PPP_ALERT_PASSWORDS) || !needpasswd)
        addparam(cmdarg, &argi, "noaskpassword");

    // reminder option must be specified after PPPDialogs plugin option
    if (!getNumber(service, kSCPropNetPPPIdleReminder, &lval) && lval) {
        if (!getNumber(service, kSCPropNetPPPIdleReminderTimer, &lval) && lval) {
            addparam(cmdarg, &argi, "reminder");
            sprintf(str, "%d", lval);
            addparam(cmdarg, &argi, str);
        }
    }

    /* add any additional plugin we want to load */
    array = CFDictionaryGetValue(service, kSCPropNetPPPPlugins);
    if (array && (CFGetTypeID(array) == CFArrayGetTypeID())) {
        lval = CFArrayGetCount(array);
        for (i = 0; i < lval; i++) {
            string = CFArrayGetValueAtIndex(array, i);
            if (string && (CFGetTypeID(string) == CFStringGetTypeID())) {
                addparam(cmdarg, &argi, "plugin");
                CFStringGetCString(string, str, sizeof(str) - 4, kCFStringEncodingUTF8);
                strcat(str, ".ppp");	// add plugin suffix
                addparam(cmdarg, &argi, str);
            }
        }
    }

    // always try to use options defined in /etc/ppp/peers/[service provider] 
    // they can override what have been specified by the PPPController
    // be careful to the conflicts on options
    if (!getString(service, kSCPropUserDefinedName, str, sizeof(str)) && str[0]) {
        addparam(cmdarg, &argi, "call");
        addparam(cmdarg, &argi, str);
    }
    
/* ************************************************************************* */

    CFRelease(service);
#if 0
    printf("/usr/sbin/pppd ");
    for (i = 1; i < MAXARG; i++) {
        if (cmdarg[i]) {
            printf("%d :  %s\n", i, cmdarg[i]);
           //printf("%s ", cmdarg[i]);
        }
    }
    printf("\n");
#endif

    ppp->lastdevstatus = 0;

    if (_SCDPluginExecCommand(exec_callback, (void*)ppp_makeref(ppp), 0, 0, PATH_PPPD, cmdarg) == -1) {
        ppp->laststatus =  EXIT_FATAL_ERROR;
        ppp_display_error(ppp);
    }
    else {
        ppp->laststatus = EXIT_OK;
        ppp->started_link = 1;
        ppp->phase = PPP_INITIALIZE;
        ppp->oldphase = PPP_INITIALIZE;
    }
    
    for (i = 0; i < MAXARG; i++) {
        if (cmdarg[i]) {
            free(cmdarg[i]);
            cmdarg[i] = 0;
        }
    }

    return ppp->laststatus;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void exec_callback(pid_t pid, int status, struct rusage *rusage, void *context)
{
    struct ppp 	*ppp;
    u_int32_t	error;
    
    if (status == 0)	// everything is fine...
        return;
        
    ppp = ppp_findbyref((u_int32_t)context);
    if (ppp == 0)		// the service has disappeared
        return;	
        
    if (ppp->started_link == 0)	// ignore the callback.
        return;
    
    /* an error occured while parsing arguments */
    /* the dynamic store will not be updated */
    /* we also don't want to restart dialondemand */
    ppp->laststatus =  (status >> 8) & 0xFF;
    ppp->phase = PPP_IDLE;
    ppp->started_link = 0;
    ppp_display_error(ppp);

    error = ppp_translate_error(ppp->subtype, ppp->laststatus, 0);
    client_notify(ppp->sid, (u_int32_t)context, PPP_EVT_DISCONNECTED, error);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_dodisconnect(struct ppp *ppp, int sig)
{
    int			pid, dokill = 1;

    if (ppp->phase != PPP_IDLE && !ppp->kill_link && !ppp->kill_sent) {
    
	// anticipate next phase
         switch (ppp->phase) {
            case PPP_INITIALIZE:
                if (ppp->needconnect) {
                    ppp->needconnect = 0;
                    if (ppp->needconnectopts) {
                        free(ppp->needconnectopts);
                        ppp->needconnectopts = 0;
                    }
                }
                dokill = 0;	// we can have race condition with ccl execution, so kill it again later
                // no break;
            case PPP_CONNECTLINK:
                ppp->phase = PPP_DISCONNECTLINK;
                break;
            case PPP_DISCONNECTLINK:
            case PPP_TERMINATE:
                return 0;
            default:
                ppp->phase = PPP_TERMINATE;
        }
   
        if (dokill 
            && !getNumberFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, kSCEntNetPPP, 
                CFSTR("pid") /*kSCPropNetPPPpid*/, &pid)) {
         //printf("ppp_dodisconnect : unit %d, kill pid %d\n", ppp->unit, pid);
            ppp->started_link = 0;
            ppp->kill_sent = 1;
            kill(pid, sig);
            }
        else {
            //printf("ppp_dodisconnect : kill later unit %d\n", ppp->unit);

            ppp->kill_link = sig;	// kill it later
            }
            
   }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_dosuspend(struct ppp *ppp)
{
    int		pid;

    if (ppp->phase != PPP_IDLE
        && !getNumberFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, kSCEntNetPPP, 
                CFSTR("pid") /*kSCPropNetPPPpid*/, &pid)) {
    
        kill(pid, SIGTSTP);
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_doresume(struct ppp *ppp)
{
    int		pid;

    if (ppp->phase != PPP_IDLE
        && !getNumberFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, kSCEntNetPPP, 
                CFSTR("pid") /*kSCPropNetPPPpid*/, &pid)) {
    
        kill(pid, SIGCONT);
    }

    return 0;
}

