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

#ifdef	USE_SYSTEMCONFIGURATION_PUBLIC_APIS
#include <SystemConfiguration/SystemConfiguration.h>
#else	/* USE_SYSTEMCONFIGURATION_PUBLIC_APIS */
#include <SystemConfiguration/v1Compatibility.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#endif	/* USE_SYSTEMCONFIGURATION_PUBLIC_APIS */

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

#define PATH_PPPD 		"/usr/sbin/pppd"
#define PPPD_PRGM 		"pppd"
#define PATH_CCL	  	"/usr/libexec/CCLEngine"
#define PATH_MINITERM	  	"/usr/libexec/MiniTerm.app/Contents/MacOS/MiniTerm"


// pppd error codes
#define EXIT_OK                  	0
#define EXIT_FATAL_ERROR 		1
#define EXIT_OPTION_ERROR        	2
#define EXIT_NOT_ROOT            	3
#define EXIT_NO_KERNEL_SUPPORT   	4
#define EXIT_USER_REQUEST        	5
#define EXIT_LOCK_FAILED 		6
#define EXIT_OPEN_FAILED 		7
#define EXIT_CONNECT_FAILED      	8
#define EXIT_PTYCMD_FAILED       	9
#define EXIT_NEGOTIATION_FAILED  	10
#define EXIT_PEER_AUTH_FAILED    	11
#define EXIT_IDLE_TIMEOUT        	12
#define EXIT_CONNECT_TIME        	13
#define EXIT_CALLBACK            	14
#define EXIT_PEER_DEAD           	15
#define EXIT_HANGUP              	16
#define EXIT_LOOPBACK            	17
#define EXIT_INIT_FAILED 		18
#define EXIT_AUTH_TOPEER_FAILED  	19
#define EXIT_TERMINAL_FAILED  		20

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

TAILQ_HEAD(, ppp) 	ppp_head;
SCDSessionRef		gCfgCache;
extern char 		*gPluginsDir;
io_connect_t		gIOPort;
io_connect_t		gSleeping;
long			gSleepArgument;
CFUserNotificationRef 	gSleepNotification;

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static int start_program(char *program, char **cmdarg);
static u_int32_t ppp_translate_error(struct ppp *ppp, u_int32_t native_error);
static int ppp_getdeviceerror(struct ppp *ppp, u_int32_t *error);
static void ppp_display_error(struct ppp *ppp);
static void ppp_sleep(void * x, io_service_t y, natural_t messageType, void *messageArgument);

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_init_all() {

    SCDStatus			status;
    IONotificationPortRef	notify;
    io_object_t			iterator;

    gSleeping = 0;	// obviously we are not sleeping :-)
    gSleepNotification = 0;    
    
    TAILQ_INIT(&ppp_head);

    printf("size of ppp = %ld\n", sizeof(struct ppp));
    printf("MAXPATHLEN = %d\n", MAXPATHLEN);
    printf("sizeof client = %ld\n", sizeof(struct client));
    printf("sizeof options = %ld\n", sizeof(struct options));

    /* opens now our session to the cache */
    status = SCDOpen(&gCfgCache, CFSTR("PPPController"));
    if (status != SCD_OK) {
        printf("SCDOpen failed,  %s\n", SCDError(status));
        return 1;
    }

    /* install the power management callback */
    gIOPort = IORegisterForSystemPower(0, &notify, ppp_sleep, &iterator);
    if (gIOPort == 0) {
        printf("IORegisterForSystemPower failed\n");
        return 1;
    }
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                        IONotificationPortGetRunLoopSource(notify),
                        kCFRunLoopDefaultMode);
                        
    /* read configuration from database */
    options_init_all(gCfgCache);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_dispose_all()
{

    // dispose ppp data structures
    SCDClose(&gCfgCache);
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
        if (CFStringCompare(ppp->serviceID, serviceID, 0) ==  kCFCompareEqualTo) 
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
----------------------------------------------------------------------------- */
void ppp_printlist()
{
    struct ppp		*ppp;

    SCDLog(LOG_INFO, CFSTR("Printing list of ppp services : \n"));
    TAILQ_FOREACH(ppp, &ppp_head, next) {
        SCDLog(LOG_INFO, CFSTR("Service : %@, subtype = %d\n"), ppp->serviceID, ppp->subtype);
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
    u_short 		unit, subtype;

    //SCDLog(LOG_INFO, CFSTR("ppp_new, subtype = %%@, serviceID = %@\n"), subtypeRef, serviceID);

    if (CFStringCompare(subtypeRef, kSCValNetInterfaceSubTypePPPSerial, 0) == kCFCompareEqualTo) 
        subtype = PPP_TYPE_SERIAL;
    else if (CFStringCompare(subtypeRef, kSCValNetInterfaceSubTypePPPoE, 0) == kCFCompareEqualTo)
        subtype = PPP_TYPE_PPPoE;
//  else if (CFStringCompare(subtypeRef, kSCValNetInterfaceSubTypePPPoA, 0) == kCFCompareEqualTo)
//      subtype = PPP_TYPE_PPPoA;
//  else if (CFStringCompare(subtypeRef, kSCValNetInterfaceSubTypePPPISDN, 0) == kCFCompareEqualTo)
//      subtype = PPP_TYPE_ISDN;
//  else if (CFStringCompare(subtypeRef, kSCValNetInterfaceSubTypePPPOther, 0) == kCFCompareEqualTo)
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

    ppp->serviceID = CFStringCreateCopy(NULL, serviceID);
    if (!ppp->serviceID) {
        free(ppp);
        return 0;	// very bad...
    }

    ppp->subtypeRef = CFStringCreateCopy(NULL, subtypeRef);
    if (!ppp->subtypeRef) {
        CFRelease(ppp->serviceID);
        free(ppp);
        return 0;	// very bad...
    }
    
    ppp->unit = unit;
    ppp->ifunit = 0xFFFF;		// no real unit yet
    strncpy(ppp->name, "ppp", IFNAMSIZ);
    ppp->subtype = subtype;
    ppp->phase = PPP_IDLE;
    ppp->alertenable = PPP_ALERT_ENABLEALL;

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

    ppp->phase = PPP_IDLE;
    if (!getNumber(service, CFSTR("Status"), &lval)) {
        ppp->phase = lval;
       
        //CFStringGetCString(ppp->serviceID, str, sizeof(str), kCFStringEncodingUTF8);
        //printf("service '%s', state = %d\n", str, lval);
        
        ppp->ifunit = 0xFFFF;
        if (ppp->phase == PPP_RUNNING || ppp->phase == PPP_STATERESERVED) {
            // should get the interfce name at the right place 
            if (!getString(service, CFSTR("InterfaceName"), str, sizeof(str))) {
                sscanf(str, "ppp%d", &val);
                ppp->ifunit = val;
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

    if (getNumber(service, CFSTR("LastCause"), &lval))
        ppp->laststatus = 0;
    else {
        ppp->laststatus = ppp_translate_error(ppp, lval);
    }
    
    if (getNumber(service, CFSTR("ConnectTime"), &ppp->conntime)) 
        ppp->conntime = 0;

    if (getNumber(service, CFSTR("SessionTimer")/*kSCPropNetPPPSessionTimer*/, &ppp->maxtime)) 
        ppp->maxtime = 0;
        
    if (ppp->phase != ppp->oldphase) {
        switch (ppp->phase) {
 	    case PPP_STATERESERVED:
	      // forget about the last error when we rearm the dial on demand
	      if (ppp->oldphase != PPP_INITIALIZE)
                ppp_display_error(ppp);
	      break;
 
            case PPP_IDLE:
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
                    ppp_doconnect(ppp, 0, 0);
                    if (ppp->needconnectopts) {
                        free(ppp->needconnectopts);
                        ppp->needconnectopts = 0;
                    }
                }
                else {
                    // check if last error != MAXFAIL
                    if ((!getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                        kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &lval) && lval)
                         && (getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                            kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &lval) || !lval
                            || gLoggedInUser[0])) {
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
void ppp_display_error(struct ppp *ppp) 
{
    CFStringRef 	msg = NULL;
    
    switch (ppp->laststatus) {
        /* PPP errors */
        case PPP_ERR_GEN_ERROR:
            msg = CFSTR("PPP_ERR_GEN_ERROR");
            break;
        case PPP_ERR_CONNSCRIPTFAILED:
            msg = CFSTR("PPP_ERR_CONNSCRIPTFAILED");
            break;
        case PPP_ERR_TERMSCRIPTFAILED:
            msg = CFSTR("PPP_ERR_TERMSCRIPTFAILED");
            break;
        case PPP_ERR_LCPFAILED:
            msg = CFSTR("PPP_ERR_LCPFAILED");
            break;
        case PPP_ERR_AUTHFAILED:
            msg = CFSTR("PPP_ERR_AUTHFAILED");
            break;
        case PPP_ERR_IDLETIMEOUT:
            msg = CFSTR("PPP_ERR_IDLETIMEOUT");
            break;
        case PPP_ERR_SESSIONTIMEOUT:
            msg = CFSTR("PPP_ERR_SESSIONTIMEOUT");
            break;
        case PPP_ERR_LOOPBACK:
            msg = CFSTR("PPP_ERR_LOOPBACK");
            break;
        case PPP_ERR_PEERDEAD:
            msg = CFSTR("PPP_ERR_PEERDEAD");
            break;
        case PPP_ERR_DISCSCRIPTFAILED:
            msg = CFSTR("PPP_ERR_DISCSCRIPTFAILED");
            break;
        case PPP_ERR_DISCBYPEER:
            msg = CFSTR("PPP_ERR_DISCBYPEER");
            break;
        case PPP_ERR_DISCBYDEVICE:
            msg = CFSTR("PPP_ERR_DISCBYDEVICE");
            break;

        /* Modem errors */
        case PPP_ERR_MOD_NOCARRIER:
            msg = CFSTR("PPP_ERR_MOD_NOCARRIER");
            break;
        case PPP_ERR_MOD_BUSY:
            msg = CFSTR("PPP_ERR_MOD_BUSY");
            break;
        case PPP_ERR_MOD_NODIALTONE:
            msg = CFSTR("PPP_ERR_MOD_NODIALTONE");
            break;
        case PPP_ERR_MOD_ERROR:
            msg = CFSTR("PPP_ERR_MOD_ERROR");
            break;
        case PPP_ERR_MOD_HANGUP:
            msg = CFSTR("PPP_ERR_MOD_HANGUP");
            break;
    }

    if (msg && (ppp->alertenable & PPP_ALERT_ERRORS))
        CFUserNotificationDisplayNotice(30, kCFUserNotificationNoteAlertLevel, 
            gIconURLRef, NULL, gBundleURLRef, CFSTR("Internet Connect"), msg, NULL);

}

/* -----------------------------------------------------------------------------
translate a pppd native cause into a PPP API cause
----------------------------------------------------------------------------- */
u_int32_t ppp_translate_error(struct ppp *ppp, u_int32_t native_error)
{
    u_int32_t	error = PPP_ERR_GEN_ERROR; 
    
    switch (native_error) {
        case EXIT_USER_REQUEST:
            error = 0;
            break;
        case EXIT_CONNECT_FAILED:
            ppp_getdeviceerror(ppp, &error);
            break;
        case EXIT_TERMINAL_FAILED:
            error = PPP_ERR_TERMSCRIPTFAILED;
            break;
        case EXIT_NEGOTIATION_FAILED:
            error = PPP_ERR_LCPFAILED;
            break;
        case EXIT_AUTH_TOPEER_FAILED:
            error = PPP_ERR_AUTHFAILED;
            break;
        case EXIT_IDLE_TIMEOUT:
            error = PPP_ERR_IDLETIMEOUT;
            break;
        case EXIT_CONNECT_TIME:
            error =  PPP_ERR_SESSIONTIMEOUT;
            break;
        case EXIT_LOOPBACK:
            error =  PPP_ERR_LOOPBACK;
            break;
        case EXIT_PEER_DEAD:
            error =  PPP_ERR_PEERDEAD;
            break;
        case EXIT_OK:
            error = PPP_ERR_DISCBYPEER;
            break;
        case EXIT_HANGUP:
            error = PPP_ERR_DISCBYDEVICE;
            if (ppp->subtype == PPP_TYPE_SERIAL)
                error = PPP_ERR_MOD_HANGUP;
            break;
    }
    
    return error;
}

/* -----------------------------------------------------------------------------
translate a pppd native cause into a PPP API cause
----------------------------------------------------------------------------- */
int ppp_getdeviceerror(struct ppp *ppp, u_int32_t *error)
{
    int 	ret = ENODEV;	// ???
    u_int32_t 	lval;

    switch (ppp->subtype) {
        case PPP_TYPE_SERIAL:
            ret = 0;
            *error = PPP_ERR_CONNSCRIPTFAILED;
            if (!getNumberFromEntity(gCfgCache, kSCCacheDomainState, ppp->serviceID, 
                kSCEntNetModem, CFSTR("LastCause"), &lval)) {
                switch (lval) {
                    case cclErr_NoCarrierErr:
                        *error = PPP_ERR_MOD_NOCARRIER;
                        break;
                    case cclErr_LineBusyErr:
                        *error = PPP_ERR_MOD_BUSY;
                        break;
                    case cclErr_NoDialTone:
                        *error = PPP_ERR_MOD_NODIALTONE;
                        break;
                    case cclErr_ModemErr:
                        *error = PPP_ERR_MOD_ERROR;
                        break;
                    default:
                        ret = ENODEV; // ???
                }
            }
            break;

        case PPP_TYPE_PPPoE:
            ret = 0;
            // nee to handle PPPoE specific error codes
            break;
        default:
            ret = 0;
    }

    return ret;
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
            switch (ppp->phase) {
                case PPP_IDLE:
                    //printf("ppp_updatesetup : unit %d, PPP_IDLE\n", ppp->unit);
                    if ((!getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                        kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &lval) && lval)
                         && (getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                            kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &lval) || !lval
                            || gLoggedInUser[0])) {
                     
                            ppp_doconnect(ppp, 0, 1);
                    }
                    break;
                default :
                    //printf("ppp_updatesetup : unit %d, NOT IDLE, state = %d\n", ppp->unit, ppp->phase);
                    ppp_dodisconnect(ppp, 15);
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
                    && (getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                        kSCEntNetPPP, CFSTR("DisconnectOnSleep")/*kSCPropNetPPPDisconnectOnSleep*/, &disc)
			|| disc	
                        || ppp->phase == PPP_STATERESERVED)) { 
                    allow = 0;
                    if (ppp->phase != PPP_STATERESERVED)
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
                    && ppp->phase != PPP_STATERESERVED) {
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
                service = getEntity(gCfgCache, kSCCacheDomainState, ppp->serviceID, kSCEntNetPPP);
                if (service) {
                    ppp_updatestate(ppp, service);
                    CFRelease(service);
                    if (ppp->phase == PPP_IDLE) {
                        if ((!getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                            kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &lval) && lval)
                            && (getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                                kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &lval) || !lval
                                || gLoggedInUser[0])) {
                     
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
            && !getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
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
            && !getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
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
void ppp_dispose(struct ppp *ppp)
{

    TAILQ_REMOVE(&ppp_head, ppp, next);
    
    // need to close the protocol first
    ppp_dodisconnect(ppp, 15);

    // then free the structure
    CFRelease(ppp->serviceID);
    CFRelease(ppp->subtypeRef);
    free(ppp);
    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_event(struct client *client, struct msg *msg)
{
    u_int32_t		event = *(u_int32_t *)&msg->data[0];
    u_int32_t		error = *(u_int32_t *)&msg->data[4];
    u_char 		*serviceid = &msg->data[8];
    CFStringRef		ref;
    struct ppp		*ppp;
    CFDictionaryRef	service = NULL;

    serviceid[msg->hdr.m_len - 8] = 0;	// need to zeroterminate the serviceid
    msg->hdr.m_len = 0xFFFFFFFF; // no reply
    //printf("ppp_event, event = 0x%x, cause = 0x%x, serviceid = '%s'\n", event, error, serviceid);

    ref = CFStringCreateWithCString(NULL, serviceid, kCFStringEncodingUTF8);
    if (ref) {
        ppp = ppp_findbyserviceID(ref);
        if (ppp) {
        
           // update status information first
            service = getEntity(gCfgCache, kSCCacheDomainState, ref, kSCEntNetPPP);
            if (service) {
                ppp_updatestate(ppp, service);
                CFRelease(service);
            }
        
            if (event == PPP_EVT_DISCONNECTED) {
                //if (error == EXIT_USER_REQUEST)
                //    return;	// PPP API generates PPP_EVT_DISCONNECTED only for unrequested disconnections
                error = ppp_translate_error(ppp, error);
            }
            else 
                error = 0;
            client_notify(ppp_makeref(ppp), event, error);
        }
        CFRelease(ref);
    }
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
    int 			error, ret, needpasswd = 0;
    u_char 			sopt[OPT_STR_LEN];
    u_int32_t			len, lval, lval1, i, argi = 0;
    CFDictionaryRef		service = NULL;
    CFStringRef			ref = NULL;

    // (!getNumberFromEntity(gCfgCache, kSCCacheDomainState, ppp->serviceID, kSCEntNetPPP, CFSTR("pid") /*kSCPropNetPPPpid*/, &lval)) 
    if (gSleeping) 
        return EIO;	// not the right time to dial

    switch (ppp->phase) {
        case PPP_IDLE:
            break;
        case PPP_STATERESERVED:	// dial on demand waiting, just trigger the connection
            //return ppp_triggerdemand(ppp);
            // since pppd is in dormant phase, it should immediatly quit.
            // FIx me : there is a small window of conflict where pppd is changing state, but it's 
            // not yet published
            if (opts) {
                ppp->needconnectopts = malloc(sizeof(struct client_opts));
                if (!ppp->needconnectopts) {
                    ppp->laststatus = ENOMEM;
                    return ENOMEM;
                }
                bcopy(opts, ppp->needconnectopts, sizeof(struct client_opts));
            }
            ppp_dodisconnect(ppp, 15);
            ppp->needconnect = 1;
            ppp->phase = PPP_INITIALIZE;
            return 0;
            break;
        default:
            return EIO;	// not the right time to dial
    }

    service = getEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, kSCEntNetPPP);
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
    
    if (!getNumber(service, kSCPropNetPPPVerboseLogging, &lval) && lval)
        addparam(cmdarg, &argi, "debug");
        
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
    
    switch (ppp->subtype) {
    
    case PPP_TYPE_SERIAL: 
           // the controller has a built-in knowledge of serial connections
    
#if 0
        if (!getString(service, CFSTR("PtyCommand"), str, sizeof(str)) && str[0]) {
        
            addparam(cmdarg, &argi, "pty");
            addparam(cmdarg, &argi, str);

            if (!getNumber(service, CFSTR("PtyDelay"), &lval)) {
                addparam(cmdarg, &argi, "pty-delay");
                sprintf(str, "%d", lval*1000);		// let's give some time to connect the pty
                addparam(cmdarg, &argi, str);
            }

            break;
            // we are done
        }
#endif
        /*  PPPSerial currently supports Modem
            in case of Modem, the DeviceName key will contain the actual device,
            while the Modem dictionnary will contain the modem settings.
            if Hardware is undefined, or different from Modem, 
            we expect to find external configuration files. 
            (This is the case for ppp over ssh or pptp) 
        */
        if (getStringFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
            kSCEntNetInterface, kSCPropNetInterfaceHardware, sopt, OPT_STR_LEN) 
            || strcmp(sopt, "Modem")) {
                // we are done
                break;
        }

        if (!ppp_getoptval(ppp, opts, PPP_OPT_DEV_NAME, sopt, &len, service)) {
            str[0] = 0;
            if (sopt[0] != '/') {
                strcat(str, DIR_TTYS);
                if ((sopt[0] != 't')
                    || (sopt[1] != 't')
                    || (sopt[2] != 'y')
                    || (sopt[3] != 'd'))
                    strcat(str, "cu.");
            }
            strcat(str, sopt);
            addparam(cmdarg, &argi, str);
    
            if (!ppp_getoptval(ppp, opts, PPP_OPT_DEV_SPEED, &lval, &len, service) && lval) {
                sprintf(str, "%d", lval);
                addparam(cmdarg, &argi, str);
            }
        }
        
        if (!ppp_getoptval(ppp, opts, PPP_OPT_DEV_CONNECTSCRIPT, sopt, &len, service) && sopt[0]) {
            // ---------- connect script parameter ----------
            addparam(cmdarg, &argi, "connect");
    
            CFStringGetCString(ppp->serviceID, str2, sizeof(str2), kCFStringEncodingUTF8);
            sprintf(str, "%s -l %s -f '%s%s' ", PATH_CCL, str2, sopt[0] == '/' ? "" :  DIR_MODEMS, sopt);
    
            if (!getNumber(service, kSCPropNetPPPVerboseLogging, &lval) && lval)
                strcat(str, " -v ");
    
            if (!ppp_getoptval(ppp, opts, PPP_OPT_COMM_REMOTEADDR, sopt, &len, service) && sopt[0]) {
                sprintf(str2, " -T '%s' ", sopt);
                strcat(str, str2);
            }
    
            if (!ppp_getoptval(ppp, opts, PPP_OPT_LOGFILE, sopt, &len, service) && sopt[0]) {
                // if logfile start with /, it's a full path
                // otherwise it's relative to the logs folder (convention)
                // the default path is /var/log
    
                sprintf(str2, " -E ");
                strcat(str, str2);
            }
            
            // specify syslog facility
            sprintf(str2, " -S %d ", LOG_INFO | LOG_LOCAL2);// LOG_PPP == LOG_LOCAL2
            strcat(str, str2); 
    
            // add all the ccl flags
            lval = 1;
            getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                kSCEntNetModem, kSCPropNetModemSpeaker, &lval) ;
            sprintf(str2, " -s %d ",lval ? 1 : 0);
            strcat(str, str2);
    
             lval = 1;
            getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                kSCEntNetModem, CFSTR("ErrorCorrection"), &lval);
            sprintf(str2, " -e %d ",lval ? 1 : 0);
            strcat(str, str2);

            lval = 1;
            getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                kSCEntNetModem, CFSTR("DataCompression"), &lval);
            sprintf(str2, " -c %d ",lval ? 1 : 0);
            strcat(str, str2);

            lval = 0;
            getNumberFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                kSCEntNetModem, kSCPropNetModemPulseDial, &lval);
            sprintf(str2, " -p %d ",lval ? 1 : 0);
            strcat(str, str2);
    
            // dialmode : 0 = normal, 1 = blind(ignoredialtone), 2 = manual
            lval = 0;
            if (!getStringFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
                kSCEntNetModem, kSCPropNetModemDialMode, sopt, OPT_STR_LEN)) {
                str2[0] = 0;
                CFStringGetCString(kSCValNetModemDialModeIgnoreDialTone, str2, sizeof(str2), kCFStringEncodingUTF8);
                if (!strcmp(str2, sopt))
                    lval = 1;
                else {
                    str2[0] = 0;
                    CFStringGetCString(kSCValNetModemDialModeManual, str2, sizeof(str2), kCFStringEncodingUTF8);
                    if (!strcmp(str2, sopt))
                        lval = 2;
                }
            }
            sprintf(str2, " -d %d ",lval);
            strcat(str, str2);
    
           // add the notification strings
            if (gCancelRef) {
                strcat(str, " -C '");
                CFStringGetCString(gCancelRef, str2, sizeof(str2), kCFStringEncodingUTF8);
                strcat(str, str2);
                strcat(str, "'");
            }
    
            if (gInternetConnectRef && (ppp->alertenable & PPP_ALERT_PASSWORDS)) {
                strcat(str, " -I '");
                CFStringGetCString(gInternetConnectRef, str2, sizeof(str2), kCFStringEncodingUTF8);
                strcat(str, str2);
                strcat(str, "'");
            }
            
            if (gIconURLRef && (ref = CFURLGetString(gIconURLRef))) {
                strcat(str, " -i '");
                CFStringGetCString(ref, str2, sizeof(str2), kCFStringEncodingUTF8);
                strcat(str, str2);
                strcat(str, "'");
            }

            addparam(cmdarg, &argi, str);
            
            // ---------- add the redial options ----------
            // right now, this option only works in the scope of the ppp modem connection
            if (!getNumber(service, kSCPropNetPPPCommRedialEnabled, &lval) && lval) {
                    
                if (!getString(service, kSCPropNetPPPCommAlternateRemoteAddress, sopt, sizeof(sopt)) && sopt[0]) {
                    addparam(cmdarg, &argi, "redialalternate");
                    addparam(cmdarg, &argi, "altconnect");
                    // just reuse the same connect command and add an other remote addres option at the end
                    sprintf(str2, " -T '%s' ", sopt);
                    strcat(str, str2);
                    addparam(cmdarg, &argi, str);
                }
        
                addparam(cmdarg, &argi, "busycode");
                //Fix me : we only get the 8 low bits return code from the wait_pid
                sprintf(str, "%d", 122 /*cclErr_LineBusyErr*/);
                addparam(cmdarg, &argi, str);
        
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

        }
        
        // ---------- disconnect script parameter ----------
        if (!ppp_getoptval(ppp, opts, PPP_OPT_DEV_CONNECTSCRIPT, sopt, &len, service) && sopt[0]) {
            addparam(cmdarg, &argi, "disconnect");
    
            CFStringGetCString(ppp->serviceID, str2, sizeof(str2), kCFStringEncodingUTF8);
            sprintf(str, "%s -m 1 -l %s -f '%s%s' ", PATH_CCL, str2, sopt[0] == '/' ? "" :  DIR_MODEMS, sopt);
    
            if (!getNumber(service, kSCPropNetPPPVerboseLogging, &lval) && lval)
                strcat(str, " -v ");
    
            if (!ppp_getoptval(ppp, opts, PPP_OPT_LOGFILE, sopt, &len, service) && sopt[0]) {
                // if logfile start with /, it's a full path
                // otherwise it's relative to the logs folder (convention)
                // the default path is /var/log
    
                sprintf(str2, " -E ");
                strcat(str, str2);
            }
            
            // specify syslog facility
            sprintf(str2, " -S %d ", LOG_INFO | LOG_LOCAL2);// LOG_PPP == LOG_LOCAL2
            strcat(str, str2); 
    
            // add the notification strings
            if (gCancelRef) {
                strcat(str, " -C '");
                CFStringGetCString(gCancelRef, str2, sizeof(str2), kCFStringEncodingUTF8);
                strcat(str, str2);
                strcat(str, "'");
            }
    
            if (gInternetConnectRef && (ppp->alertenable & PPP_ALERT_PASSWORDS)) {
                strcat(str, " -I '");
                CFStringGetCString(gInternetConnectRef, str2, sizeof(str2), kCFStringEncodingUTF8);
                strcat(str, str2);
                strcat(str, "'");
            }

            if (gIconURLRef && (ref = CFURLGetString(gIconURLRef))) {
                strcat(str, " -i '");
                CFStringGetCString(ref, str2, sizeof(str2), kCFStringEncodingUTF8);
                strcat(str, str2);
                strcat(str, "'");
            }

            addparam(cmdarg, &argi, str);
        }
     break;

    case PPP_TYPE_PPPoE: 
 	// ---------- connect plugin parameter ----------
        addparam(cmdarg, &argi, "plugin");
        CFStringGetCString(ppp->subtypeRef, str2, sizeof(str2) - 4, kCFStringEncodingUTF8);
        strcat(str2, ".ppp");	// add plugin suffix
        addparam(cmdarg, &argi, str2);
        
        if (!ppp_getoptval(ppp, opts, PPP_OPT_DEV_NAME, sopt, &len, service)) {
            addparam(cmdarg, &argi, "pppoeinterface");
            addparam(cmdarg, &argi, sopt);
        }
        break;
        
    default:

 	// ---------- connect plugin parameter ----------
        addparam(cmdarg, &argi, "plugin");
        CFStringGetCString(ppp->subtypeRef, str2, sizeof(str2) - 4, kCFStringEncodingUTF8);
        strcat(str2, ".ppp");	// add plugin suffix
        addparam(cmdarg, &argi, str2);
/*        
    if (ppp->subtype == PPP_TYPE_PPPoE) {
 	// ---------- connect plugin parameter ----------
       addparam(cmdarg, &argi, "plugin");
       addparam(cmdarg, &argi, "PPPoE.ppp");

        if (!ppp_getoptval(ppp, opts, PPP_OPT_COMM_REMOTEADDR, sopt, &len, service) && sopt[0]) {
            addparam(cmdarg, &argi, "pppoeservicename");
            sprintf(str, "%s", sopt);
            addparam(cmdarg, &argi, str);
       }
       }
 */       
    }

/* ************************************************************************* */
    /* then COMM options */

    if (!ppp_getoptval(ppp, opts, PPP_OPT_COMM_TERMINALMODE, &lval, &len, service)) {
    
        if (lval == PPP_COMM_TERM_WINDOW) {
            addparam(cmdarg, &argi, "terminal");
            addparam(cmdarg, &argi, PATH_MINITERM);
            // use console user to run the terminal window application
            addparam(cmdarg, &argi, "useconsoleuser");	
        }
        else if (lval == PPP_COMM_TERM_SCRIPT) {
            if (!ppp_getoptval(ppp, opts, PPP_OPT_COMM_TERMINALSCRIPT, sopt, &len, service) && sopt[0]) {
                addparam(cmdarg, &argi, "terminal");
        
                // shall we use service id for the terminal script ???
                //CFStringGetCString(ppp->serviceID, str2, sizeof(str2), kCFStringEncodingUTF8);
                //sprintf(str, "%s -l %s -f '%s%s' ", PATH_CCL, str2,
                //    sopt[0] == '/' ? "" :  DIR_MODEMS, sopt);
                sprintf(str, "%s -f '%s%s' ", PATH_CCL,
                    sopt[0] == '/' ? "" :  DIR_MODEMS, sopt);
        
                if (!getNumber(service, kSCPropNetPPPVerboseLogging, &lval) && lval)
                    strcat(str, " -v ");
        
                if (!ppp_getoptval(ppp, opts, PPP_OPT_AUTH_NAME, sopt, &len, service) && sopt[0]) {
                    sprintf(str2, " -U '%s' ", sopt);
                    strcat(str, str2);
                }
        
                if (!ppp_getoptval(ppp, opts, PPP_OPT_AUTH_PASSWD, sopt, &len, service) && sopt[0]) {
                    sprintf(str2, " -P '%s' ", sopt);
                    strcat(str, str2);
                }
        
                if (!ppp_getoptval(ppp, opts, PPP_OPT_LOGFILE, sopt, &len, service) && sopt[0]) {
                    // if logfile start with /, it's a full path
                    // otherwise it's relative to the logs folder (convention)
                    // the default path is /var/log
        
                    sprintf(str2, " -E ");
                    strcat(str, str2);
                }
                        
                // specify syslog facility
                sprintf(str2, " -S %d ", LOG_INFO | LOG_LOCAL2);// LOG_PPP == LOG_LOCAL2
                strcat(str, str2); 
        
                // add the notification strings
                if (gCancelRef) {
                    strcat(str, " -C '");
                    CFStringGetCString(gCancelRef, str2, sizeof(str2), kCFStringEncodingUTF8);
                    strcat(str, str2);
                    strcat(str, "'");
                }
        
                if (gInternetConnectRef && (ppp->alertenable & PPP_ALERT_PASSWORDS)) {
                    strcat(str, " -I '");
                    CFStringGetCString(gInternetConnectRef, str2, sizeof(str2), kCFStringEncodingUTF8);
                    strcat(str, str2);
                    strcat(str, "'");
                }

                if (gIconURLRef && (ref = CFURLGetString(gIconURLRef))) {
                    strcat(str, " -i '");
                    CFStringGetCString(ref, str2, sizeof(str2), kCFStringEncodingUTF8);
                    strcat(str, str2);
                    strcat(str, "'");
                }

                addparam(cmdarg, &argi, str);
            }
        }
    }

#if 0
    if (!getNumber(service, CFSTR("CommConnectDelay"), &lval)) {
        addparam(cmdarg, &argi, "connect-delay");
        sprintf(str, "%d", lval*1000);		// let's give some time to connect the pty
        addparam(cmdarg, &argi,str);
    }
#endif

/* ************************************************************************* */

    // add generic phonenumber option
    if (!ppp_getoptval(ppp, opts, PPP_OPT_COMM_REMOTEADDR, sopt, &len, service) && sopt[0]) {
        addparam(cmdarg, &argi, "remoteaddress");
        sprintf(str, "%s", sopt);
        addparam(cmdarg, &argi, str);
    }

    addparam(cmdarg, &argi, "cancelcode");
    //Fix me : we only get the 8 low bits return code from the wait_pid
    sprintf(str, "%d", 136 /*cclErr_ScriptCancelled*/);
    addparam(cmdarg, &argi, str);

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
        sprintf(str, "%d", 0);
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
    
    if (!getStringFromEntity(gCfgCache, kSCCacheDomainState, 0, 
        kSCEntNetIPv4, kSCPropNetIPv4Router, sopt, OPT_STR_LEN) && sopt[0]) {
        addparam(cmdarg, &argi, "ipparam");
        addparam(cmdarg, &argi, sopt);
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
    if (getAddressFromEntity(gCfgCache, kSCCacheDomainSetup, ppp->serviceID, 
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
            if (!ppp_getoptval(ppp, opts, PPP_OPT_AUTH_PASSWD, sopt, &len, service) && sopt[0]) {
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

    // no compression protocol
    addparam(cmdarg, &argi, "noccp");	

    // add the dialog plugin
    if (gPluginsDir) {
        addparam(cmdarg, &argi, "plugin");
        sprintf(str, "%sPPPDialogs.ppp", gPluginsDir);
        addparam(cmdarg, &argi, str);
        
        //ppp_getoptval(ppp, opts, PPP_OPT_ALERTENABLE, &lval, &len, service);

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

    error = start_program(PATH_PPPD, cmdarg);

    for (i = 0; i < MAXARG; i++) {
        if (cmdarg[i]) {
            free(cmdarg[i]);
            cmdarg[i] = 0;
        }
    }

    //if (error != EXIT_OK)
    //    SCDLog(LOG_INFO, CFSTR("Error starting pppd for serviceID '%@', error code = %d \n"), ppp->serviceID, error);

    ret = 0;
    // translate into the appropriate error code
    // only a subset of the error code can happen at this time
    switch (error) {
        case EXIT_OK:
            ppp->phase = PPP_INITIALIZE;
            break;
        case EXIT_NO_KERNEL_SUPPORT:
            ret = EPROTONOSUPPORT;
            break;
        case EXIT_OPTION_ERROR:
            ret = EINVAL;
            break;
        default:
            ret = EIO;
    }

    ppp->laststatus = ret;
    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void closeall()
{
    int i;

    for (i = getdtablesize() - 1; i >= 0; i--) close(i);
    open("/dev/null", O_RDWR, 0);
    dup(0);
    dup(0);
    return;
}

/* -----------------------------------------------------------------------------
* run a program to talk to the serial device
* (e.g. to run the connector or disconnector script).
program contain only the path to the program
command contains the full commanf to execute (prgrname+parameters)
----------------------------------------------------------------------------- */
int start_program(char *program, char **cmdarg)
{
    int 	pid;
    int 	error;

#if 0
    pid = fork();
    if (pid < 0) {
        //       error(ppp, "Failed to create child process: %m");
        return -1;
    }

    if (pid == 0) {

        closeall();
#endif        
        // double fork, because configd doesn't handle SIGCHLD
        pid = fork();
        if (pid < 0) {
            return -1;
        }

        if (pid == 0) {
        closeall();
            // need to exec a tool, with complete parameters list
            execv(program, cmdarg);

            // grandchild exits
            exit(0);
            /* NOTREACHED */
        }
#if 0
        // child exits
        exit(0);
        /* NOTREACHED */
    }
#endif
    // grand parents wait for child's completion, that occurs immediatly since grandchild does the job
    while (waitpid(pid, &error, 0) < 0) {
	if (errno == EINTR)
            continue;
        return -1;
    }
    
    error = (error >> 8) & 0xFF;
    return error;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_dodisconnect(struct ppp *ppp, int sig)
{
    int			pid, dokill = 1;

    if (ppp->phase != PPP_IDLE && !ppp->kill_link) {
    
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
            default:
                ppp->phase = PPP_TERMINATE;
        }
   
        if (dokill 
            && !getNumberFromEntity(gCfgCache, kSCCacheDomainState, ppp->serviceID, kSCEntNetPPP, 
                CFSTR("pid") /*kSCPropNetPPPpid*/, &pid)) {
         //printf("ppp_dodisconnect : unit %d, kill pid %d\n", ppp->unit, pid);
            kill(pid, sig);
            }
        else {
            //printf("ppp_dodisconnect : kill later unit %d\n", ppp->unit);

            ppp->kill_link = sig;	// kill it later
            }
            
   }

    return 0;
}

