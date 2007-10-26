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

#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <paths.h>
#include <net/if.h>
#include <net/ndrv.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#include <mach/mach_time.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/boolean.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <SystemConfiguration/SCPrivate.h>      // for SCLog() and VPN private keys
#include <mach/task_special_ports.h>
#include "pppcontroller_types.h"
#include "pppcontroller.h"
#include <SystemConfiguration/SCValidation.h>
#include <bsm/libbsm.h>

#include "PPPControllerPriv.h"
#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "../Family/ppp_domain.h"
#include "../Helpers/pppd/pppd.h"

#include "ppp_client.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "ppp_socket_server.h"
#include "ppp_utils.h"


/* -----------------------------------------------------------------------------
Definitions
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
#define CREATEGLOBALSTATE(a)	SCDynamicStoreKeyCreateNetworkGlobalEntity(0, \
                    kSCDynamicStoreDomainState, a)

enum {
	READ	= 0,	// read end of standard UNIX pipe
	WRITE	= 1	// write end of standard UNIX pipe
};

#define MAX_EXTRACONNECTTIME 20 /* allows 20 seconds after wakeup */
#define MIN_EXTRACONNECTTIME 3 /* if we do give extra time, give at lease 3 seconds */

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

TAILQ_HEAD(, ppp) 	ppp_head;
io_connect_t		gIOPort;
int					gSleeping;
long				gSleepArgument;
CFUserNotificationRef 	gSleepNotification;
uint64_t			gWakeUpTime = 0;
double				gTimeScaleSeconds;
CFRunLoopSourceRef 	gStopRls;
CFStringRef			gLoggedInUser = NULL;
uid_t				gLoggedInUserUID = 0;
SCDynamicStoreRef	gDynamicStore = NULL;

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static void display_error(struct ppp *ppp);
static void iosleep_notifier(void * x, io_service_t y, natural_t messageType, void *messageArgument);
static void exec_callback(pid_t pid, int status, struct rusage *rusage, void *context);
static void exec_postfork(pid_t pid, void *arg);
static int can_sleep();
static int will_sleep(int checking);
static void wake_up();
static void ipv4_state_changed();
static int is_idle();
static int log_out();
static int log_in();
static int log_switch();
static int send_pppd_params(struct ppp *ppp, CFDictionaryRef service, CFDictionaryRef options, u_int8_t dialondemand);
static int change_pppd_params(struct ppp *ppp, CFDictionaryRef service, CFDictionaryRef options);
static u_short findfreeunit(u_short subtype);

static int add_client(struct ppp *ppp, void *client, int autoclose);
static int remove_client(struct ppp *ppp, void *client);
static struct ppp_client *get_client(struct ppp *ppp, void *client);
static int  remove_all_clients(struct ppp *ppp);

static struct ppp * new_service(CFStringRef serviceID , CFStringRef subtypeRef);
static int dispose_service(struct ppp *ppp);
static int setup_service(CFStringRef serviceID);
static void store_notifier(SCDynamicStoreRef session, CFArrayRef changedKeys, void *info);
static void reorder_services();
static void finish_setup_services();
static void setup_PPPoE(struct ppp *ppp);
static void dispose_PPPoE(struct ppp *ppp);
#ifdef DEBUG
static void print_services();
#endif


/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
    service management and control
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_init_all() {

    IONotificationPortRef	notify;
    io_object_t			iterator;
    mach_timebase_info_data_t   timebaseInfo;
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

	/* init list of services */
    TAILQ_INIT(&ppp_head);

	/* setup power management callback  */
    gSleeping = 0;
    gSleepNotification = 0;    
    gStopRls = 0;    

    gIOPort = IORegisterForSystemPower(0, &notify, iosleep_notifier, &iterator);
    if (gIOPort == 0) {
        printf("IORegisterForSystemPower failed\n");
        goto fail;
    }
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                        IONotificationPortGetRunLoopSource(notify),
                        kCFRunLoopDefaultMode);
                        
	/* init time scale */
    if (mach_timebase_info(&timebaseInfo) != KERN_SUCCESS) {	// returns scale factor for ns
        printf("mach_timebase_info failed\n");
        goto fail;
    }
    gTimeScaleSeconds = ((double) timebaseInfo.numer / (double) timebaseInfo.denom) / 1000000000;

    /* opens now our session to the Dynamic Store */
    if ((gDynamicStore = SCDynamicStoreCreate(0, CFSTR("PPPController"), store_notifier, 0)) == NULL)
        goto fail;
    
    if ((rls = SCDynamicStoreCreateRunLoopSource(0, gDynamicStore, 0)) == NULL) 
        goto fail;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    gLoggedInUser = SCDynamicStoreCopyConsoleUser(gDynamicStore, &gLoggedInUserUID, 0);
    
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
        
    /* install the notifier for the global changes */
    if ((key = CREATEGLOBALSETUP(kSCEntNetIPv4)) == NULL)
        goto fail;
    CFArrayAppendValue(keys, key);
    CFRelease(key);
    
    if ((key = CREATEGLOBALSTATE(kSCEntNetIPv4)) == NULL)
        goto fail;
    CFArrayAppendValue(keys, key);
    CFRelease(key);

    /* install the notifier for user login/logout */
    if ((key = SCDynamicStoreKeyCreateConsoleUser(0)) == NULL)
        goto fail;
    CFArrayAppendValue(keys, key);
    CFRelease(key);    

    /* add all the notification in one chunk */
    SCDynamicStoreSetNotificationKeys(gDynamicStore, keys, patterns);

    /* read the initial configured interfaces */
    key = CREATESERVICESETUP(kSCEntNetPPP);
    setup = CREATEPREFIXSETUP();
    if (key == NULL || setup == NULL)
        goto fail;
        
    services = SCDynamicStoreCopyKeyList(gDynamicStore, key);
    if (services == NULL)
        goto done;	// no PPP service setup

    nb = CFArrayGetCount(services);
    for (i = 0; i < nb; i++) {
        CFStringRef serviceID;
        if (serviceID = parse_component(CFArrayGetValueAtIndex(services, i), setup)) {
            setup_service(serviceID);            
            CFRelease(serviceID);
        }
    }
    
    reorder_services();
    finish_setup_services();
    //print_services();
	
done:    
    my_CFRelease(services);
    my_CFRelease(key);
    my_CFRelease(setup);
    my_CFRelease(keys);
    my_CFRelease(patterns);
    return ret;
	
fail:
    my_CFRelease(gDynamicStore);
    ret = -1;
    goto done;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_dispose_all()
{
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int setup_service(CFStringRef serviceID)
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
            dispose_service(ppp);
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
        dispose_service(ppp);
        ppp = 0;
    }

    // check to see if it is a new service
    if (!ppp) {
        ppp = new_service(serviceID, subtype);
        if (!ppp)
            goto done;
    }

    // delay part or all the setup once all the notification for the service have been received
    ppp->flags |= FLAG_SETUP;
       
done:            
    my_CFRelease(interface);
    my_CFRelease(service);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static
void reorder_services()
{
    CFDictionaryRef	ip_dict = NULL;
    CFStringRef		key, serviceID;
    CFArrayRef		serviceorder;
    int				i, nb;
    struct ppp		*ppp;

    key = CREATEGLOBALSETUP(kSCEntNetIPv4);
    if (key) {
        ip_dict = (CFDictionaryRef)SCDynamicStoreCopyValue(gDynamicStore, key);
        if (ip_dict) {
            serviceorder = CFDictionaryGetValue(ip_dict, kSCPropNetServiceOrder);        
            if (serviceorder) {
  	        nb = CFArrayGetCount(serviceorder);
	        for (i = 0; i < nb; i++) {
                    serviceID = CFArrayGetValueAtIndex(serviceorder, i);                    
                    if (ppp = ppp_findbyserviceID(serviceID)) {
						/* move it to the tail */
						TAILQ_REMOVE(&ppp_head, ppp, next);
						TAILQ_INSERT_TAIL(&ppp_head, ppp, next);
					}
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
static
void store_notifier(SCDynamicStoreRef session, CFArrayRef changedKeys, void *info)
{
    CFStringRef		setup, userkey, ipsetupkey, ipstatekey;
    u_long		i, nb, doreorder = 0, dopostsetup = 0;

    if (changedKeys == NULL) 
        return;
    
    setup = CREATEPREFIXSETUP();        
    userkey = SCDynamicStoreKeyCreateConsoleUser(0);
    ipsetupkey = CREATEGLOBALSETUP(kSCEntNetIPv4);
    ipstatekey = CREATEGLOBALSTATE(kSCEntNetIPv4);
    
    if (setup == NULL || userkey == NULL || ipsetupkey == NULL || ipstatekey == NULL) {
#ifdef DEBUG
        SCLog(TRUE, LOG_ERR, CFSTR("PPPController cache_notifier : can't allocate keys\n"));
#endif
        goto done;
    }

    nb = CFArrayGetCount(changedKeys);
    for (i = 0; i < nb; i++) {

        CFStringRef	change, serviceID;
        
        change = CFArrayGetValueAtIndex(changedKeys, i);

        // --------- Check for change of console user --------- 
        if (CFEqual(change, userkey)) {
            CFStringRef olduser = gLoggedInUser;
            gLoggedInUser = SCDynamicStoreCopyConsoleUser(session, &gLoggedInUserUID, 0);
            if (gLoggedInUser == 0)
                log_out();	// key disappeared, user logged out
            else if (olduser == 0)
                log_in();	// key appeared, user logged in
            else
                log_switch();	// key changed, user has switched
            if (olduser) CFRelease(olduser);
            continue;
        }

        // ---------  Check for change in service order --------- 
        if (CFEqual(change, ipsetupkey)) {
            // can't just reorder the list now 
            // because the list may contain service not already created
            doreorder = 1;
            continue;
        }

        // ---------  Check for change in ipv4 state --------- 
        if (CFEqual(change, ipstatekey)) {
			ipv4_state_changed();
            continue;
        }

        // --------- Check for change in other entities (state or setup) --------- 
        serviceID = parse_component(change, setup);
        if (serviceID) {
            setup_service(serviceID);
            CFRelease(serviceID);
            dopostsetup = 1;
            continue;
        }
        
    }

    if (doreorder) {
        reorder_services();
        //print_services();
    }
    if (dopostsetup)
        finish_setup_services();

done:
    my_CFRelease(setup);
    my_CFRelease(userkey);
    my_CFRelease(ipsetupkey);
    my_CFRelease(ipstatekey);
    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_stop_all(CFRunLoopSourceRef stopRls)
{
    u_int32_t			delay = 0;
    struct ppp 			*ppp;
    
    if (gStopRls)
        // should not happen
        return;
        
    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (ppp->phase != PPP_IDLE) { 
            delay = 1;
            ppp_disconnect(ppp, 0, SIGTERM);
        }
    }
    
    if (delay)
        // need to signal the runloop source later
		gStopRls = stopRls;
    else
        // we are done
        CFRunLoopSourceSignal(stopRls);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int32_t ppp_makeref(struct ppp *ppp)
{
    return (((u_long)ppp->subtype) << 16) + ppp->unit;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct ppp *ppp_findbyserviceID(CFStringRef serviceID)
{
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next)
        if (CFStringCompare(ppp->serviceID, serviceID, 0) == kCFCompareEqualTo) 
            return ppp;
	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct ppp *ppp_findbysid(u_char *data, int len)
{
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) 
        if (ppp->sid && (strlen(ppp->sid) == len) && !strncmp(ppp->sid, data, len)) 
            return ppp;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct ppp *ppp_findbypid(pid_t pid)
{
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) 
        if (ppp->pid == pid)
            return ppp;
    return 0;
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
get the first free ref numer within a family
----------------------------------------------------------------------------- */
static 
u_short findfreeunit(u_short subtype)
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

#ifdef DEBUG
/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static
void print_services()
{
    struct ppp		*ppp;

    SCLog(TRUE, LOG_INFO, CFSTR("Printing list of ppp services : \n"));
    TAILQ_FOREACH(ppp, &ppp_head, next) {
        SCLog(TRUE, LOG_INFO, CFSTR("Service : %@, subtype = %d\n"), ppp->serviceID, ppp->subtype);
    }
}
#endif

/* -----------------------------------------------------------------------------
an interface structure needs to be created
unit is the ppp managed unit
----------------------------------------------------------------------------- */
static 
struct ppp * new_service(CFStringRef serviceID , CFStringRef subtypeRef)
{
    struct ppp 		*ppp;
    u_short 		unit, subtype, len;
    CFURLRef		url;
    u_char 		str[MAXPATHLEN], str2[32];

    //SCLog(LOG_INFO, CFSTR("new_service, subtype = %%@, serviceID = %@\n"), subtypeRef, serviceID);

    if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPSerial)) 
        subtype = PPP_TYPE_SERIAL;
    else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPoE))
        subtype = PPP_TYPE_PPPoE;
    else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPTP))
        subtype = PPP_TYPE_PPTP;
    else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypeL2TP))
        subtype = PPP_TYPE_L2TP;
//  else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPoA))
//      subtype = PPP_TYPE_PPPoA;
//  else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPISDN))
//      subtype = PPP_TYPE_ISDN;
//  else if (CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPOther))
//      subtype = PPP_TYPE_OTHER;
    else 
        subtype = PPP_TYPE_OTHER;

    unit = findfreeunit(subtype);
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
    ppp->pid = -1;
    ppp->subtype = subtype;
    ppp->phase = PPP_IDLE;
    ppp->statusfd[READ] = -1;
    ppp->statusfd[WRITE] = -1;
    ppp->controlfd[READ] = -1;
    ppp->controlfd[WRITE] = -1;
    ppp->uid = 0;
    ppp->ndrv_socket = -1;
	ppp->flags |= FLAG_FIRSTDIAL;

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

    TAILQ_INIT(&ppp->client_head);

    TAILQ_INSERT_TAIL(&ppp_head, ppp, next);
	
    return ppp;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static
void display_error(struct ppp *ppp) 
{
    CFStringRef 	ppp_msg = NULL, dev_msg = NULL, msg = NULL;
    CFPropertyListRef	langRef = NULL;

    if (ppp->laststatus == EXIT_USER_REQUEST)
        return;

    if ((ppp->flags & FLAG_ALERTERRORS) == 0)
        return;

    if (gLoggedInUser) {
		CFPreferencesSynchronize(kCFPreferencesAnyApplication,
            gLoggedInUser, kCFPreferencesAnyHost);
		langRef = CFPreferencesCopyValue(CFSTR("AppleLanguages"), 
            kCFPreferencesAnyApplication, gLoggedInUser, kCFPreferencesAnyHost);
    }

	/* try again for global preferences for the current host */
	if (langRef == NULL) {
		CFPreferencesSynchronize(kCFPreferencesAnyApplication,
            kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
		langRef = CFPreferencesCopyValue(CFSTR("AppleLanguages"), 
            kCFPreferencesAnyApplication, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
	}
	
    if (ppp->lastdevstatus && ppp->bundle) {            
        dev_msg = CFStringCreateWithFormat(0, 0, CFSTR("Device Error %d"), ppp->lastdevstatus);
        if (dev_msg)
            msg = CopyUserLocalizedString(ppp->bundle, dev_msg, dev_msg, langRef);
    }
    
    if (msg == NULL) {
        ppp_msg = CFStringCreateWithFormat(0, 0, CFSTR("PPP Error %d"), ppp->laststatus);
        if (ppp_msg)
            msg = CopyUserLocalizedString(gBundleRef, ppp_msg, ppp_msg, langRef) ;
    }

    if (msg && CFStringGetLength(msg))
        CFUserNotificationDisplayNotice(120, kCFUserNotificationNoteAlertLevel, 
            gIconURLRef, NULL, gBundleURLRef, CFSTR("Internet Connect"), msg, NULL);

    my_CFRelease(msg);
    my_CFRelease(ppp_msg);
    my_CFRelease(dev_msg);
    my_CFRelease(langRef);
}

/* -----------------------------------------------------------------------------
	for PPPoE over Airport or Ethernet services, install a protocol to enable the interface
	even when PPPoE is not connected
----------------------------------------------------------------------------- */
static 
void setup_PPPoE(struct ppp *ppp)
{
    CFDictionaryRef	interface;
	CFStringRef		hardware, device;
    struct sockaddr_ndrv 	ndrv;

	if (ppp->subtype == PPP_TYPE_PPPoE) {

		interface = copyEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, kSCEntNetInterface);
		if (interface) {
		
			device = CFDictionaryGetValue(interface, kSCPropNetInterfaceDeviceName);
			hardware = CFDictionaryGetValue(interface, kSCPropNetInterfaceHardware);

			if (isA_CFString(hardware) && isA_CFString(device) 
				&& ((CFStringCompare(hardware, kSCEntNetAirPort, 0) == kCFCompareEqualTo)
					|| (CFStringCompare(hardware, kSCEntNetEthernet, 0) == kCFCompareEqualTo))) {

				if (ppp->device 
					&& (CFStringCompare(device, ppp->device, 0) != kCFCompareEqualTo)) {
					dispose_PPPoE(ppp);
				}
				
				if (!ppp->device) {
					ppp->ndrv_socket = socket(PF_NDRV, SOCK_RAW, 0);
					if (ppp->ndrv_socket >= 0) {
						ppp->device = device;
						CFRetain(ppp->device);
						CFStringGetCString(device, ndrv.snd_name, sizeof(ndrv.snd_name), kCFStringEncodingMacRoman);
						ndrv.snd_len = sizeof(ndrv);
						ndrv.snd_family = AF_NDRV;
						if (bind(ppp->ndrv_socket, (struct sockaddr *)&ndrv, sizeof(ndrv)) < 0) {
							dispose_PPPoE(ppp);
						}
					}
				}
			}
			else {
				/* not an Airport device */
				dispose_PPPoE(ppp);
			}

			CFRelease(interface);
		}
	}
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void dispose_PPPoE(struct ppp *ppp)
{
	if (ppp->ndrv_socket != -1) {
		close(ppp->ndrv_socket);
		ppp->ndrv_socket = -1;
	}
	if (ppp->device) {
		CFRelease(ppp->device);
		ppp->device = 0;
	}
}

/* -----------------------------------------------------------------------------
changed for this ppp occured in configd cache
----------------------------------------------------------------------------- */
static 
void finish_setup_services()
{
    u_int32_t 		lval;
    struct ppp 		*ppp;
    CFDictionaryRef	service;
    
    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (ppp->flags & FLAG_SETUP) {

            ppp->flags &= ~(FLAG_FREE + FLAG_SETUP);

			setup_PPPoE(ppp);

            switch (ppp->phase) {

                case PPP_IDLE:
                    //printf("ppp_updatesetup : unit %d, PPP_IDLE\n", ppp->unit);
                    if ((getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                        kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &lval) && lval)
                         && (!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                            kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &lval) || !lval
                            || gLoggedInUser)) {
                            ppp_connect(ppp, 0, 1, 0, 0, 0, 0, 0);
                    }
                    break;

                case PPP_DORMANT:
                case PPP_HOLDOFF:
					// config has changed, dialondemand will need to be restarted
					ppp->flags |= FLAG_CONFIGCHANGEDNOW;
					ppp->flags &= ~FLAG_CONFIGCHANGEDLATER;
                    ppp_disconnect(ppp, 0, SIGTERM);
                    break;

                default :
                    // config has changed, dialondemand will need to be restarted
					ppp->flags |= FLAG_CONFIGCHANGEDLATER;
					ppp->flags &= ~FLAG_CONFIGCHANGEDNOW;
                    /* if ppp was started in dialondemand mode, then stop it */
//                    if (ppp->dialondemand)
//                        ppp_disconnect(ppp, 0, SIGTERM);

                    service = copyService(kSCDynamicStoreDomainSetup, ppp->serviceID);
                    if (service) {
                        change_pppd_params(ppp, service, ppp->connectopts);
                        CFRelease(service);
                    }
                    break;
            }
		}
    }
}

/* -----------------------------------------------------------------------------
call back from power management
----------------------------------------------------------------------------- */
static 
void iosleep_notifier(void * x, io_service_t y, natural_t messageType, void *messageArgument)
{
    CFMutableDictionaryRef 	dict;
    SInt32 			error;
    int 			delay;

    //printf("messageType %08lx, arg %08lx\n",(long unsigned int)messageType, (long unsigned int)messageArgument);
    
    switch ( messageType ) {
    
        case kIOMessageSystemWillSleep:
            gSleeping  = 1;	// time to sleep
            gSleepArgument = (long)messageArgument;   
             
            delay = will_sleep(0);
            if (delay == 0)
                IOAllowPowerChange(gIOPort, (long)messageArgument);
            else {
                if (delay & 2) {
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
						CFRelease(dict);
                    }
                }
            }
            break;

        case kIOMessageCanSystemSleep:
            if (can_sleep()) 
                IOAllowPowerChange(gIOPort, (long)messageArgument);
            else
                IOCancelPowerChange(gIOPort, (long)messageArgument);
            break;

        case kIOMessageSystemWillNotSleep:
            /* should get it only if someone refuse an idle sleep 
                but I don't have anything to do here */
            break;
			
        case kIOMessageSystemWillPowerOn:
			gSleeping = 0; // time to wakeup
			gWakeUpTime = mach_absolute_time();
            if (gSleepNotification) {
                CFUserNotificationCancel(gSleepNotification);
                CFRelease(gSleepNotification);
                gSleepNotification = 0;
           }
           break;
            
        case kIOMessageSystemHasPoweredOn:
            wake_up();            
            break;
    }
    
}

/* -----------------------------------------------------------------------------
system is asking permission to sleep
return if sleep is authorized
----------------------------------------------------------------------------- */
static 
int can_sleep()
{
    struct ppp                  *ppp;
    u_int32_t                   prevent;
        
    // I refuse idle sleep if ppp is connected
    TAILQ_FOREACH(ppp, &ppp_head, next) {
                
        if (ppp->phase == PPP_RUNNING) {
                
			// by default, vpn connection don't prevent idle sleep
			switch (ppp->subtype) {
				case PPP_TYPE_PPTP:
				case PPP_TYPE_L2TP:
					prevent = 0;
					break;
				default :
					prevent = 1;
			}
							
			getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
							kSCEntNetPPP, CFSTR("PreventIdleSleep"), &prevent);
			
			if (prevent)
				return 0;
		}
	}

    return 1;
}

/* -----------------------------------------------------------------------------
system is going to sleep
disconnect services and return if a delay is needed
----------------------------------------------------------------------------- */
static 
int will_sleep(int checking)
{
    u_int32_t			disc, delay = 0, alert = 0;
    struct ppp 			*ppp;
            
    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (ppp->phase != PPP_IDLE
            // by default, disconnect on sleep
            && (!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPDisconnectOnSleep, &disc)
                || disc)) { 
            delay = 1;
            if (ppp->phase != PPP_DORMANT || ppp->phase != PPP_HOLDOFF) 
                alert = 2;
            if (!checking)
                ppp_disconnect(ppp, 0, SIGTERM);
        }
    }
        
    return delay + alert;
}

/* -----------------------------------------------------------------------------
check is all session are idle
----------------------------------------------------------------------------- */
static 
int is_idle()
{
    struct ppp 			*ppp;
            
    TAILQ_FOREACH(ppp, &ppp_head, next)
        if (ppp->phase != PPP_IDLE)
            return 0;
        
    return 1;
}

/* -----------------------------------------------------------------------------
ipv4 state has changed
----------------------------------------------------------------------------- */
static
void ipv4_state_changed()
{
    struct ppp		*ppp;
	
    TAILQ_FOREACH(ppp, &ppp_head, next) {
		ppp->flags |= FLAG_FIRSTDIAL;
    }
}

/* -----------------------------------------------------------------------------
system is waking up
need to check the dialondemand flag again
----------------------------------------------------------------------------- */
static
void wake_up()
{
    struct ppp		*ppp;
    u_int32_t		lval;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
		ppp->flags |= FLAG_FIRSTDIAL;
        if (ppp->phase == PPP_IDLE) {
            if ((getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &lval) && lval)
                && (!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                    kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &lval) || !lval
                    || gLoggedInUser)) {
                    ppp_connect(ppp, 0, 1, 0, 0, 0, 0, 0);
            }
        }
    }
}

/* -----------------------------------------------------------------------------
user has looged out
need to check the disconnect on logout flag for the ppp interfaces
----------------------------------------------------------------------------- */
static 
int log_out()
{
    struct ppp		*ppp;
    u_int32_t		disc;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (ppp->phase != PPP_IDLE
            && getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &disc) 
            && disc)
            ppp_disconnect(ppp, 0, SIGTERM);
    }
    return 0;
}

/* -----------------------------------------------------------------------------
user has logged in
need to check the dialondemand flag again
----------------------------------------------------------------------------- */
static 
int log_in()
{
    struct ppp		*ppp;
    u_int32_t		val;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
		ppp->flags |= FLAG_FIRSTDIAL;
        if (ppp->phase == PPP_IDLE
            && getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &val) 
            && val) {
            ppp_connect(ppp, 0, 1, 0, 0, 0, 0, 0);
		}
    }
    return 0;
}

/* -----------------------------------------------------------------------------
user has switched
need to check the disconnect on logout and dial on traffic 
flags for the ppp interfaces
----------------------------------------------------------------------------- */
static 
int log_switch()
{
    struct ppp		*ppp;
    u_int32_t		disc, val, demand;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        
		ppp->flags |= FLAG_FIRSTDIAL;

        demand = getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                    kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &val) && val;
    
        switch (ppp->phase) {
            case PPP_IDLE:
                // rearm dial on demand
                if (demand)
                    ppp_connect(ppp, 0, 1, 0, 0, 0, 0, 0);
                break;
                
            default:
				disc = 0;
				/* if the DisconnectOnFastUserSwitch key does not exist, use kSCPropNetPPPDisconnectOnLogout */
                if (!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                    kSCEntNetPPP, CFSTR("DisconnectOnFastUserSwitch"), &disc))
					getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                    kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &disc);
				if (disc) {
                        
					// if dialondemand is set, it will need to be restarted
					ppp->flags &= ~FLAG_CONFIGCHANGEDLATER;
					if (demand)
						ppp->flags |= FLAG_CONFIGCHANGEDNOW;
					else
						ppp->flags &= ~FLAG_CONFIGCHANGEDNOW;
					ppp_disconnect(ppp, 0, SIGTERM);
                }
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
an interface is come down, dispose the ppp structure
----------------------------------------------------------------------------- */
static 
int dispose_service(struct ppp *ppp)
{

    // need to close the protocol first
    ppp_disconnect(ppp, 0, SIGTERM);

    if (ppp->phase != PPP_IDLE) {
        ppp->flags |= FLAG_FREE;
        return 1;
    }
    
    TAILQ_REMOVE(&ppp_head, ppp, next);    

    // then free the structure
	dispose_PPPoE(ppp);
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
static 
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
static 
void writeparam(int fd, char *param)
{
    
    write(fd, param, strlen(param));
    write(fd, " ", 1);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void writeintparam(int fd, char *param, u_int32_t val)
{
    u_char	str[32];
    
    writeparam(fd, param);
    sprintf(str, "%d", val);
    writeparam(fd, str);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void writedataparam(int fd, char *param, void *data, int len)
{
    
    writeintparam(fd, param, len);
    write(fd, data, len);
    write(fd, " ", 1);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void writestrparam(int fd, char *param, char *val)
{
    
    write(fd, param, strlen(param));

    /* we need to quote and escape the parameter */
    write(fd, " \"", 2);
    while (*val) {
        switch (*val) {
            case '\\':
            case '\"':
				write(fd, "\\", 1);
				break;
            case '\'':
            case ' ':
                //write(fd, "\\", 1);
                ;
        }
        write(fd, val, 1);
        val++;
    }
    write(fd, "\" ", 2);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int send_pppd_params(struct ppp *ppp, CFDictionaryRef service, CFDictionaryRef options, u_int8_t dialondemand)
{
    char 			str[MAXPATHLEN], str2[256];
    int 			needpasswd = 0, tokendone = 0, auth_default = 1, from_service, optfd, awaketime;
    u_int32_t			auth_bits = 0xF; /* PAP + CHAP + MSCHAP1 + MPCHAP2 */
    u_int32_t			len, lval, lval1, i;
    u_char 			sopt[OPT_STR_LEN];
    CFDictionaryRef		pppdict = NULL, dict, modemdict;
    CFArrayRef			array = NULL;
    CFStringRef			string = NULL;
    CFDataRef			dataref = 0;
    void			*dataptr = 0;
    u_int32_t			datalen = 0;

    pppdict = CFDictionaryGetValue(service, kSCEntNetPPP);
    if ((pppdict == 0) || (CFGetTypeID(pppdict) != CFDictionaryGetTypeID()))
        return -1; 	// that's bad...
    
    optfd = ppp->controlfd[WRITE];

    writeparam(optfd, "[OPTIONS]");

    // -----------------
    // add the dialog plugin
    if (gPluginsDir) {
        CFStringGetCString(gPluginsDir, str, sizeof(str), kCFStringEncodingUTF8);
        strcat(str, "PPPDialogs.ppp");
        writestrparam(optfd, "plugin", str);
    }

    // -----------------
    // verbose logging 
    get_int_option(ppp, kSCEntNetPPP, kSCPropNetPPPVerboseLogging, options, service, &lval, 0);
    if (lval)
        writeparam(optfd, "debug");

    // -----------------
    // alert flags 
    ppp->flags &= ~(FLAG_ALERTERRORS + FLAG_ALERTPASSWORDS);
    ppp_getoptval(ppp, options, service, PPP_OPT_ALERTENABLE, &lval, &len);
    if (lval & PPP_ALERT_ERRORS)
        ppp->flags |= FLAG_ALERTERRORS;
    if (lval & PPP_ALERT_PASSWORDS)
        ppp->flags |= FLAG_ALERTPASSWORDS;
                
    if (ppp_getoptval(ppp, options, service, PPP_OPT_LOGFILE, sopt, &len) && sopt[0]) {
        // if logfile start with /, it's a full path
        // otherwise it's relative to the logs folder (convention)
        // we also strongly advise to name the file with the link number
        // for example ppplog0
        // the default path is /var/log
        // it's useful to have the debug option with the logfile option
        // with debug option, pppd will log the negociation
        // debug option is different from kernel debug trace

        sprintf(str, "%s%s", sopt[0] == '/' ? "" : DIR_LOGS, sopt);
        writestrparam(optfd, "logfile", str);
    }

    // -----------------
    // connection plugin
    CFStringGetCString(ppp->subtypeRef, str2, sizeof(str2) - 4, kCFStringEncodingUTF8);
    strcat(str2, ".ppp");	// add plugin suffix
    writestrparam(optfd, "plugin", str2);
        
    // -----------------
    // device name 
    if (ppp_getoptval(ppp, options, service, PPP_OPT_DEV_NAME, sopt, &len) && sopt[0])
        writestrparam(optfd, "device", sopt);

    // -----------------
    // device speed 
    if (ppp_getoptval(ppp, options, service, PPP_OPT_DEV_SPEED, &lval, &len) && lval) {
        sprintf(str, "%d", lval);
        writeparam(optfd, str);
    }
        
    // -----------------
    // subtype specific parameters 

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
            get_str_option(ppp, kSCEntNetInterface, kSCPropNetInterfaceHardware, options, 0, sopt, &lval, "");
            if (strcmp(sopt, "Modem")) {
                // we are done
                break;
            }
        
	#if 1
			/* merge all modem options into modemdict */
			modemdict = copyEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, kSCEntNetModem);
			if (!modemdict) {
                break;
			}

			if (options) {
				dict = CFDictionaryGetValue(options, kSCEntNetModem);
				if (dict && 
					(CFGetTypeID(dict) == CFDictionaryGetTypeID()) && 
					CFDictionaryGetCount(dict)) {
				
					CFMutableDictionaryRef modemdict_mutable = CFDictionaryCreateMutableCopy(NULL, 0, modemdict);
					if (modemdict_mutable) {
						CFTypeRef value;

						// merge it into modemdict only some of the keys
						
						if (value = CFDictionaryGetValue(dict, kSCPropNetModemConnectionScript))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemConnectionScript, value);

						if (value = CFDictionaryGetValue(dict, kSCPropNetModemSpeaker))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemSpeaker, value);
						if (value = CFDictionaryGetValue(dict, kSCPropNetModemErrorCorrection))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemErrorCorrection, value);
						if (value = CFDictionaryGetValue(dict, kSCPropNetModemDataCompression))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemDataCompression, value);
						if (value = CFDictionaryGetValue(dict, kSCPropNetModemPulseDial))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemPulseDial, value);
						if (value = CFDictionaryGetValue(dict, kSCPropNetModemDialMode))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemDialMode, value);
							
						CFRelease(modemdict);
						modemdict = modemdict_mutable;
					}
				}
			}
			
			/* serialize the modem dictionary, and pass it as a parameter */
			if (dataref = Serialize(modemdict, &dataptr, &datalen)) {

				writedataparam(optfd, "modemdict", dataptr, datalen);
				CFRelease(dataref);
			}

			CFRelease(modemdict);
#endif
	#if 0
	
            if (ppp_getoptval(ppp, options, 0, PPP_OPT_DEV_CONNECTSCRIPT, sopt, &len) && sopt[0]) {
                // ---------- connect script parameter ----------
                writestrparam(optfd, "modemscript", sopt);
                
                // add all the ccl flags
                get_int_option(ppp, kSCEntNetModem, kSCPropNetModemSpeaker, options, 0, &lval, 1);
                writeparam(optfd, lval ? "modemsound" : "nomodemsound");
        
                get_int_option(ppp, kSCEntNetModem, kSCPropNetModemErrorCorrection, options, 0, &lval, 1);
                writeparam(optfd, lval ? "modemreliable" : "nomodemreliable");
    
                get_int_option(ppp, kSCEntNetModem, kSCPropNetModemDataCompression, options, 0, &lval, 1);
                writeparam(optfd, lval ? "modemcompress" : "nomodemcompress");
    
                get_int_option(ppp, kSCEntNetModem, kSCPropNetModemPulseDial, options, 0, &lval, 0);
                writeparam(optfd, lval ? "modempulse" : "modemtone");
        
                // dialmode : 0 = normal, 1 = blind(ignoredialtone), 2 = manual
                lval = 0;
                ppp_getoptval(ppp, options, 0, PPP_OPT_DEV_DIALMODE, &lval, &len);
                writeintparam(optfd, "modemdialmode", lval);
            }
#endif
            break;
            
        case PPP_TYPE_L2TP: 
        
            string = get_cf_option(kSCEntNetL2TP, kSCPropNetL2TPTransport, CFStringGetTypeID(), options, service, 0);
            if (string) {
                if (CFStringCompare(string, kSCValNetL2TPTransportIP, 0) == kCFCompareEqualTo)
                    writeparam(optfd, "l2tpnoipsec");
            }
    
			/* check for SharedSecret keys in L2TP dictionary */
            get_str_option(ppp, kSCEntNetL2TP, kSCPropNetL2TPIPSecSharedSecret, options, service, sopt, &lval, "");
            if (sopt[0]) {
                writestrparam(optfd, "l2tpipsecsharedsecret", sopt);                        

				string = get_cf_option(kSCEntNetL2TP, kSCPropNetL2TPIPSecSharedSecretEncryption, CFStringGetTypeID(), options, service, 0);
				if (string) {
					if (CFStringCompare(string, CFSTR("Key"), 0) == kCFCompareEqualTo)
						writestrparam(optfd, "l2tpipsecsharedsecrettype", "key");                        
					else if (CFStringCompare(string, kSCValNetL2TPIPSecSharedSecretEncryptionKeychain, 0) == kCFCompareEqualTo)
						writestrparam(optfd, "l2tpipsecsharedsecrettype", "keychain");                        
				}
            } 
			/* then check IPSec dictionary */
			else {		
				get_str_option(ppp, kSCEntNetIPSec, kSCPropNetIPSecSharedSecret, options, service, sopt, &lval, "");
				if (sopt[0]) {
					writestrparam(optfd, "l2tpipsecsharedsecret", sopt);                        
					string = get_cf_option(kSCEntNetL2TP, kSCPropNetIPSecSharedSecretEncryption, CFStringGetTypeID(), options, service, 0);
					if (string) {
						if (CFStringCompare(string, CFSTR("Key"), 0) == kCFCompareEqualTo)
							writestrparam(optfd, "l2tpipsecsharedsecrettype", "key");                        
						else if (CFStringCompare(string, kSCValNetIPSecSharedSecretEncryptionKeychain, 0) == kCFCompareEqualTo)
							writestrparam(optfd, "l2tpipsecsharedsecrettype", "keychain");                        
					}
				}
			}
			
            get_int_option(ppp, kSCEntNetL2TP, CFSTR("UDPPort"), options, service, &lval, 0 /* Dynamic port */);
            writeintparam(optfd, "l2tpudpport", lval);
            break;
    
        case PPP_TYPE_PPTP: 
            get_int_option(ppp, kSCEntNetPPTP, CFSTR("TCPKeepAlive"), options, service, &lval, 0);
            if (lval) {
                get_int_option(ppp, kSCEntNetPPTP, CFSTR("TCPKeepAliveTimer"), options, service, &lval, 0);
            }
            else {
                /* option doesn't exist, piggy-back on lcp echo option */
                ppp_getoptval(ppp, options, service, PPP_OPT_LCP_ECHO, &lval, &len);
                lval = lval >> 16;
            }
            writeintparam(optfd, "pptp-tcp-keepalive", lval);
            break;
    }
    
    // -----------------
    // terminal option
    if (ppp_getoptval(ppp, options, service, PPP_OPT_COMM_TERMINALMODE, &lval, &len)) {

        /* add the PPPSerial plugin if not already present
         Fix me : terminal mode is only supported in PPPSerial types of connection
         but subtype using ptys can use it the same way */    
        if (lval != PPP_COMM_TERM_NONE && ppp->subtype != PPP_TYPE_SERIAL)
            writestrparam(optfd, "plugin", "PPPSerial.ppp");

        if (lval == PPP_COMM_TERM_WINDOW)
            writeparam(optfd, "terminalwindow");
        else if (lval == PPP_COMM_TERM_SCRIPT)
            if (ppp_getoptval(ppp, options, service, PPP_OPT_COMM_TERMINALSCRIPT, sopt, &len) && sopt[0])
                writestrparam(optfd, "terminalscript", sopt);            
    }

    // -----------------
    // generic phone number option
    if (ppp_getoptval(ppp, options, service, PPP_OPT_COMM_REMOTEADDR, sopt, &len) && sopt[0])
        writestrparam(optfd, "remoteaddress", sopt);
    
    // -----------------
    // redial options 
    get_int_option(ppp, kSCEntNetPPP, kSCPropNetPPPCommRedialEnabled, options, service, &lval, 0);
    if (lval) {
            
        get_str_option(ppp, kSCEntNetPPP, kSCPropNetPPPCommAlternateRemoteAddress, options, service, sopt, &lval, "");
        if (sopt[0])
            writestrparam(optfd, "altremoteaddress", sopt);
        
        get_int_option(ppp, kSCEntNetPPP, kSCPropNetPPPCommRedialCount, options, service, &lval, 0);
        if (lval)
            writeintparam(optfd, "redialcount", lval);

        get_int_option(ppp, kSCEntNetPPP, kSCPropNetPPPCommRedialInterval, options, service, &lval, 0);
        if (lval)
            writeintparam(optfd, "redialtimer", lval);
    }

	awaketime = gSleeping ? 0 : ((mach_absolute_time() - gWakeUpTime) * gTimeScaleSeconds);
	if (awaketime < MAX_EXTRACONNECTTIME) {
        writeintparam(optfd, "extraconnecttime", MAX(MAX_EXTRACONNECTTIME - awaketime, MIN_EXTRACONNECTTIME));
	}
	
	// -----------------
    // idle options 
    if (ppp_getoptval(ppp, options, service, PPP_OPT_COMM_IDLETIMER, &lval, &len) && lval) {
        writeintparam(optfd, "idle", lval);
        writeparam(optfd, "noidlerecv");
    }

    // -----------------
    // connection time option 
    if (ppp_getoptval(ppp, options, service, PPP_OPT_COMM_SESSIONTIMER, &lval, &len) && lval)
        writeintparam(optfd, "maxconnect", lval);
    
    // -----------------
    // dial on demand options 
    if (dialondemand) {
        writeparam(optfd, "demand");
        get_int_option(ppp, kSCEntNetPPP, CFSTR("HoldOffTime"), 0, service, &lval, 30);
        writeintparam(optfd, "holdoff", lval);
		if ((dialondemand & 0x2) && lval)
			writeparam(optfd, "holdfirst");
        get_int_option(ppp, kSCEntNetPPP, CFSTR("MaxFailure"), 0, service, &lval, 3);
        writeintparam(optfd, "maxfail", lval);
    }

    // -----------------
    // lcp echo options 
    // set echo option, so ppp hangup if we pull the modem cable
    // echo option is 2 bytes for interval + 2 bytes for failure
    if (ppp_getoptval(ppp, options, service, PPP_OPT_LCP_ECHO, &lval, &len) && lval) {
        if (lval >> 16)
            writeintparam(optfd, "lcp-echo-interval", lval >> 16);

        if (lval & 0xffff)
            writeintparam(optfd, "lcp-echo-failure", lval & 0xffff);
    }
    
    // -----------------
    // address and protocol field compression options 
    if (ppp_getoptval(ppp, options, service, PPP_OPT_LCP_HDRCOMP, &lval, &len)) {
        if (!(lval & 1))
            writeparam(optfd, "nopcomp");
        if (!(lval & 2))
            writeparam(optfd, "noaccomp");
    }

    // -----------------
    // mru option 
    if (ppp_getoptval(ppp, options, service, PPP_OPT_LCP_MRU, &lval, &len) && lval)
        writeintparam(optfd, "mru", lval);

    // -----------------
    // mtu option 
    if (ppp_getoptval(ppp, options, service, PPP_OPT_LCP_MTU, &lval, &len) && lval)
        writeintparam(optfd, "mtu", lval);

    // -----------------
    // receive async map option 
    if (ppp_getoptval(ppp, options, service, PPP_OPT_LCP_RCACCM, &lval, &len)) {
        if (lval)
			writeintparam(optfd, "asyncmap", lval);
		else 
			writeparam(optfd, "receive-all");
	} 
	else 
		writeparam(optfd, "default-asyncmap");

    // -----------------
    // send async map option 
     if (ppp_getoptval(ppp, options, service, PPP_OPT_LCP_TXACCM, &lval, &len) && lval) {
            writeparam(optfd, "escape");
            str[0] = 0;
            for (lval1 = 0; lval1 < 32; lval1++) {
                if ((lval >> lval1) & 1) {
                    sprintf(str2, "%d,", lval1);
                    strcat(str, str2);
               }
            }
            str[strlen(str)-1] = 0; // remove last ','
            writeparam(optfd, str);
       }

    // -----------------
    // ipcp options 
    if (!existEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, kSCEntNetIPv4)) {
        writeparam(optfd, "noip");
    }
    else {
    
        // -----------------
        // set ip param to be the router address 
        if (getStringFromEntity(kSCDynamicStoreDomainState, 0, 
            kSCEntNetIPv4, kSCPropNetIPv4Router, sopt, OPT_STR_LEN) && sopt[0])
            writestrparam(optfd, "ipparam", sopt);
        
        // OverridePrimary option not handled yet in Setup by IPMonitor
        get_int_option(ppp, kSCEntNetIPv4, kSCPropNetOverridePrimary, 0 /* don't look in options */, service, &lval, 0);
        if (lval)
            writeparam(optfd, "defaultroute");
    
        // -----------------
        // vj compression option 
        if (! (ppp_getoptval(ppp, options, service, PPP_OPT_IPCP_HDRCOMP, &lval, &len) && lval))
            writeparam(optfd, "novj");
    
        // -----------------
        // XXX  enforce the source address
        if (ppp->subtype == PPP_TYPE_L2TP || ppp->subtype == PPP_TYPE_PPTP ) {
            writeintparam(optfd, "ip-src-address-filter", 2);
        }
        
        // -----------------
        // ip addresses options
        if (ppp_getoptval(ppp, options, service, PPP_OPT_IPCP_LOCALADDR, &lval, &len) && lval)
            sprintf(str2, "%d.%d.%d.%d", lval >> 24, (lval >> 16) & 0xFF, (lval >> 8) & 0xFF, lval & 0xFF);
        else 
            strcpy(str2, "0");
    
        strcpy(str, str2);
        strcat(str, ":");
        if (ppp_getoptval(ppp, options, service, PPP_OPT_IPCP_REMOTEADDR, &lval, &len) && lval) 
            sprintf(str2, "%d.%d.%d.%d", lval >> 24, (lval >> 16) & 0xFF, (lval >> 8) & 0xFF, lval & 0xFF);
        else 
            strcpy(str2, "0");
        strcat(str, str2);
        writeparam(optfd, str);
    
        writeparam(optfd, "noipdefault");
        writeparam(optfd, "ipcp-accept-local");
        writeparam(optfd, "ipcp-accept-remote");
    
        // -----------------
		// add a route for the interface subnet
		writeparam(optfd, "addifroute");

    /* ************************************************************************* */
    
        // usepeerdns option
		get_int_option(ppp, kSCEntNetPPP, CFSTR("IPCPUsePeerDNS"), options, service, &lval, 1);
        if (lval)
            writeparam(optfd, "usepeerdns");

		// usepeerwins if a SMB dictionary is present
		// but make sure it is not disabled in PPP
		if (existEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, kSCEntNetSMB)) {
			get_int_option(ppp, kSCEntNetPPP, CFSTR("IPCPUsePeerWINS"), options, service, &lval, 1);
			if (lval)
				writeparam(optfd, "usepeerwins");
		}

    } // of existEntity IPv4
    
    // -----------------
    // ip6cp options 
    if (!existEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, kSCEntNetIPv6)) {
        // ipv6 is not started by default
    }
    else {
        writeparam(optfd, "+ipv6");
        writeparam(optfd, "ipv6cp-use-persistent");
    }

    // -----------------
    // acsp options 
    get_int_option(ppp, kSCEntNetPPP, kSCPropNetPPPACSPEnabled, options, service, &lval, 0);
    if (lval == 0)
        writeparam(optfd, "noacsp");

	// dhcp is on by default for vpn, and off for everything else 
    get_int_option(ppp, kSCEntNetPPP, CFSTR("UseDHCP"), options, service, &lval,  (ppp->subtype == PPP_TYPE_L2TP || ppp->subtype == PPP_TYPE_PPTP) ? 1 : 0);
    if (lval == 1)
        writeparam(optfd, "use-dhcp");
    

    // -----------------
    // authentication options 

    // don't want authentication on our side...
    writeparam(optfd, "noauth");

     if (ppp_getoptval(ppp, options, service, PPP_OPT_AUTH_PROTO, &lval, &len) && (lval != PPP_AUTH_NONE)) {

		CFStringRef			encryption = NULL;
        
		if (ppp_getoptval(ppp, options, service, PPP_OPT_AUTH_NAME, sopt, &len) && sopt[0]) {


            writestrparam(optfd, "user", sopt);
			needpasswd = 1;

            lval1 = get_str_option(ppp, kSCEntNetPPP, kSCPropNetPPPAuthPassword, options, service, sopt, &lval, "");
            if (sopt[0]) {
			
                /* get the encryption method at the same place the password is coming from. */
				encryption = get_cf_option(kSCEntNetPPP, kSCPropNetPPPAuthPasswordEncryption, CFStringGetTypeID(), 
					(lval1 == 3) ? NULL : options, (lval1 == 3) ? service : NULL , NULL);

				if (encryption && (CFStringCompare(encryption, kSCValNetPPPAuthPasswordEncryptionKeychain, 0) == kCFCompareEqualTo)) {
					writestrparam(optfd, (lval1 == 3) ? "keychainpassword" : "userkeychainpassword", sopt);
				}
				else if (encryption && (CFStringCompare(encryption, kSCValNetPPPAuthPasswordEncryptionToken, 0) == kCFCompareEqualTo)) {
					writeintparam(optfd, "tokencard", 1);
					tokendone = 1;
				}
				else 
					writestrparam(optfd, "password", sopt);
            }
            else { 
				encryption = get_cf_option(kSCEntNetPPP, kSCPropNetPPPAuthPasswordEncryption, CFStringGetTypeID(), options, service, NULL);
				if (encryption && (CFStringCompare(encryption, kSCValNetPPPAuthPasswordEncryptionToken, 0) == kCFCompareEqualTo)) {
					writeintparam(optfd, "tokencard", 1);
					tokendone = 1;
				}
            }
        }
		else {
			encryption = get_cf_option(kSCEntNetPPP, kSCPropNetPPPAuthPasswordEncryption, CFStringGetTypeID(), options, service, NULL);
			if (encryption && (CFStringCompare(encryption, kSCValNetPPPAuthPasswordEncryptionToken, 0) == kCFCompareEqualTo)) {
				writeintparam(optfd, "tokencard", 1);
				tokendone = 1;
				needpasswd = 1;
			}
		}
    }

    // load authentication protocols
    array = 0;
    if (options) {
        dict = CFDictionaryGetValue(options, kSCEntNetPPP);
        if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID()))
            array = CFDictionaryGetValue(dict, kSCPropNetPPPAuthProtocol);            
    }
    if ((array == 0) || (CFGetTypeID(array) != CFArrayGetTypeID())) {
        array = CFDictionaryGetValue(pppdict, kSCPropNetPPPAuthProtocol);
    }
    if (array && (CFGetTypeID(array) == CFArrayGetTypeID()) && (lval = CFArrayGetCount(array))) {
        auth_default = 0;
        auth_bits = 0; // clear bits
        for (i = 0; i < lval; i++) {
            string = CFArrayGetValueAtIndex(array, i);
            if (string && (CFGetTypeID(string) == CFStringGetTypeID())) {
                if (CFStringCompare(string, kSCValNetPPPAuthProtocolPAP, 0) == kCFCompareEqualTo)
                    auth_bits |= 1;
                else if (CFStringCompare(string, kSCValNetPPPAuthProtocolCHAP, 0) == kCFCompareEqualTo)
                    auth_bits |= 2;
                else if (CFStringCompare(string, CFSTR("MSCHAP1") /*kSCValNetPPPAuthProtocolMSCHAP1*/ , 0) == kCFCompareEqualTo)
                    auth_bits |= 4;
                else if (CFStringCompare(string,  CFSTR("MSCHAP2") /*kSCValNetPPPAuthProtocolMSCHAP2*/, 0) == kCFCompareEqualTo)
                    auth_bits |= 8;
                else if (CFStringCompare(string,  CFSTR("EAP") /*kSCValNetPPPAuthProtocolEAP*/, 0) == kCFCompareEqualTo)
                    auth_bits |= 0x10;
            }
        }
    }

	

	if (!tokendone) {
		// authentication variation for token card support...
		get_int_option(ppp, kSCEntNetPPP, CFSTR("TokenCard"), options, service, &lval, 0);
		if (lval) {
			writeintparam(optfd, "tokencard", lval);
			needpasswd = 1;
		}
	}
	
    // -----------------
    // eap options 

    if (auth_bits & 0x10) {
        auth_bits &= ~0x10; // clear EAP flag
        array = 0;
        from_service = 0;
        if (options) {
            dict = CFDictionaryGetValue(options, kSCEntNetPPP);
            if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID()))
                array = CFDictionaryGetValue(dict, CFSTR("AuthEAPPlugins") /*kSCPropNetPPPAuthEAPPlugins*/);
        }
        if ((array == 0) || (CFGetTypeID(array) != CFArrayGetTypeID())) {
            array = CFDictionaryGetValue(pppdict, CFSTR("AuthEAPPlugins") /*kSCPropNetPPPAuthEAPPlugins*/);
            from_service = 1;
        }
        if (array && (CFGetTypeID(array) == CFArrayGetTypeID()) && (lval = CFArrayGetCount(array))) {
            for (i = 0; i < lval; i++) {
                string = CFArrayGetValueAtIndex(array, i);
                if (string && (CFGetTypeID(string) == CFStringGetTypeID())) {
                    CFStringGetCString(string, str, sizeof(str) - 4, kCFStringEncodingUTF8);
                    // for user options, we only accept plugin in the EAP directory (/System/Library/Extensions)
                    if (from_service || strchr(str, '\\') == 0) {
                        strcat(str, ".ppp");	// add plugin suffix
                        writestrparam(optfd, "eapplugin", str);
                        auth_bits |= 0x10; // confirm EAP flag
                    }
                }
            }
        }
    }

    // -----------------
    // ccp options 
    get_int_option(ppp, kSCEntNetPPP, kSCPropNetPPPCCPEnabled, options, service, &lval, 0);
    if (lval
        // Fix me : to enforce use of MS-CHAP, refuse any alteration of default auth proto 
        // a dialer specifying PAP or CHAP will works without CCP/MPPE
        // even is CCP is enabled in the configuration.
        // Will be revisited when addition compression modules and
        // authentication modules will be added 
        && ppp_getoptval(ppp, options, service, PPP_OPT_AUTH_PROTO, &lval, &len) 
                && (lval == OPT_AUTH_PROTO_DEF)) {

        // Fix me : mppe is the only currently supported compression 
        // if the CCPAccepted and CCPRequired array are not there, 
        // assume we accept all types of compression we support

        writeparam(optfd, "mppe-stateless");
		get_int_option(ppp, kSCEntNetPPP, CFSTR("CCPMPPE128Enabled"), options, service, &lval, 1);
		writeparam(optfd, lval ? "mppe-128" : "nomppe-128");        
		get_int_option(ppp, kSCEntNetPPP, CFSTR("CCPMPPE40Enabled"), options, service, &lval, 1);
        writeparam(optfd, lval ? "mppe-40" : "nomppe-40");        

        // No authentication specified, also enforce the use of MS-CHAP
        if (auth_default)
            auth_bits = 0xc; /* MSCHAP 1 and 2 only */
    }
    else {
        // no compression protocol
        writeparam(optfd, "noccp");	
    }
    
    // set authentication protocols parameters
    if ((auth_bits & 1) == 0)
        writeparam(optfd, "refuse-pap");
    if ((auth_bits & 2) == 0)
        writeparam(optfd, "refuse-chap-md5");
    if ((auth_bits & 4) == 0)
        writeparam(optfd, "refuse-mschap");
    if ((auth_bits & 8) == 0)
        writeparam(optfd, "refuse-mschap-v2");
    if ((auth_bits & 0x10) == 0)
        writeparam(optfd, "refuse-eap");
        
    // if EAP is the only method, pppd doesn't need to ask for the password
    // let the EAP plugin handle that.
    // if there is an other protocol than EAP, then we still need to prompt for password 
    if (auth_bits == 0x10)
        needpasswd = 0;

    // loop local traffic destined to the local ip address
    // Radar #3124639.
    //writeparam(optfd, "looplocal");       

    if (!(ppp->flags & FLAG_ALERTPASSWORDS) || !needpasswd)
        writeparam(optfd, "noaskpassword");

    get_str_option(ppp, kSCEntNetPPP, kSCPropNetPPPAuthPrompt, options, service, sopt, &lval, "");
    if (sopt[0]) {
        str2[0] = 0;
        CFStringGetCString(kSCValNetPPPAuthPromptAfter, str2, sizeof(str2), kCFStringEncodingUTF8);
        if (!strcmp(sopt, str2))
            writeparam(optfd, "askpasswordafter");
    }
    
    // -----------------
    // no need for pppd to detach.
    writeparam(optfd, "nodetach");

    // -----------------
    // reminder option must be specified after PPPDialogs plugin option
    get_int_option(ppp, kSCEntNetPPP, kSCPropNetPPPIdleReminder, options, service, &lval, 0);
    if (lval) {
        get_int_option(ppp, kSCEntNetPPP, kSCPropNetPPPIdleReminderTimer, options, service, &lval, 0);
        if (lval)
            writeintparam(optfd, "reminder", lval);
    }

    // -----------------
    // add any additional plugin we want to load
    array = CFDictionaryGetValue(pppdict, kSCPropNetPPPPlugins);
    if (array && (CFGetTypeID(array) == CFArrayGetTypeID())) {
        lval = CFArrayGetCount(array);
        for (i = 0; i < lval; i++) {
            string = CFArrayGetValueAtIndex(array, i);
            if (string && (CFGetTypeID(string) == CFStringGetTypeID())) {
                CFStringGetCString(string, str, sizeof(str) - 4, kCFStringEncodingUTF8);
                strcat(str, ".ppp");	// add plugin suffix
                writestrparam(optfd, "plugin", str);
            }
        }
    }
    
    // -----------------
    // always try to use options defined in /etc/ppp/peers/[service provider] 
    // they can override what have been specified by the PPPController
    // be careful to the conflicts on options
    get_str_option(ppp, kSCEntNetPPP, kSCPropUserDefinedName, options, service, sopt, &lval, "");
    if (sopt[0])
        writestrparam(optfd, "call", sopt);
    

    writeparam(optfd, "[EOP]");

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int change_pppd_params(struct ppp *ppp, CFDictionaryRef service, CFDictionaryRef options)
{
    int 			optfd;
    u_int32_t			lval, len;
    CFDictionaryRef		pppdict = NULL;

    pppdict = CFDictionaryGetValue(service, kSCEntNetPPP);
    if ((pppdict == 0) || (CFGetTypeID(pppdict) != CFDictionaryGetTypeID()))
        return -1; 	// that's bad...
    
    optfd = ppp->controlfd[WRITE];

    writeparam(optfd, "[OPTIONS]");

    // -----------------
    // reminder option must be specified after PPPDialogs plugin option
    get_int_option(ppp, kSCEntNetPPP, kSCPropNetPPPIdleReminder, options, service, &lval, 0);
    if (lval)
        get_int_option(ppp, kSCEntNetPPP, kSCPropNetPPPIdleReminderTimer, options, service, &lval, 0);
    writeintparam(optfd, "reminder", lval);

    // -----------------
    ppp_getoptval(ppp, options, service, PPP_OPT_COMM_IDLETIMER, &lval, &len);
    writeintparam(optfd, "idle", lval);

    writeparam(optfd, "[EOP]");

    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
static
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
		printf("pppcontroller_start error: %s\n", mach_error_string(status));
		if (status != MACH_SEND_INVALID_DEST)
			printf("pppcontroller_start error NOT MACH_SEND_INVALID_DEST: %s\n", mach_error_string(status));
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
		printf("pppcontroller_start cannot authenticate bootstrap port from controller\n");
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
int ppp_connect(struct ppp *ppp, CFDictionaryRef options, u_int8_t dialondemand, void *client, int autoclose, uid_t uid, gid_t gid, mach_port_t bootstrap)
{
#define MAXARG 10
    char 			*cmdarg[MAXARG];
    u_int32_t			lval, i, argi = 0;
    CFDictionaryRef		service;
    CFStringRef			autodial;

	/* first determine autodial opportunity */
	if (options) {
		int willdial = 0, autodialval = 0;

		autodial = CFDictionaryGetValue(options, kSCPropNetPPPOnDemandPriority);
		if (!autodial) {
			// option not set, regualar dial request
			willdial = 1;
		}
		else if (CFGetTypeID(autodial) == CFStringGetTypeID()) {
			
			/* first, check if the user is he current console user */
			if (gLoggedInUser && uid == gLoggedInUserUID) {
				if (CFStringCompare(autodial, kSCValNetPPPOnDemandPriorityHigh, 0) == kCFCompareEqualTo)
					autodialval = 1;
				else if (CFStringCompare(autodial, kSCValNetPPPOnDemandPriorityLow, 0) == kCFCompareEqualTo) 
					autodialval = 2;
				else if (CFStringCompare(autodial, kSCValNetPPPOnDemandPriorityDefault, 0) == kCFCompareEqualTo) {
				
					// Autodial is required in default mode.
					// Use the aggressivity level from the options
					CFDictionaryRef dict;
					if (dict = CFDictionaryGetValue(options, kSCEntNetPPP)) {
						CFStringRef str;
						str = CFDictionaryGetValue(dict, kSCPropNetPPPOnDemandMode);
						if (str && CFGetTypeID(str) == CFStringGetTypeID()) {
							if (CFStringCompare(str, kSCValNetPPPOnDemandModeAggressive, 0) == kCFCompareEqualTo)
								autodialval = 1;
							else if (CFStringCompare(str, kSCValNetPPPOnDemandModeConservative, 0) == kCFCompareEqualTo)
								autodialval = 2;
							else /* kSCValNetPPPOnDemandModeCompatible */
								autodialval = 0;
						}
					}
					
				}
			}
			
			//printf("autodial = %d\n", autodial);
			switch (autodialval) {
				case 1: // high
					// OK, let's dial
					willdial = 1;
					break;
				case 2: // low
					// Need to implement smart behavior
					if (ppp->flags & FLAG_FIRSTDIAL) {
						// First time after major event, let's dial
						willdial = 1;
					}
					break;
				default:
					break;
			}
		}
		
		if (!willdial)
			return EIO; // SCNetworkConnection API needs to implement specific status (kSCStatusConnectionOnDemandRetryLater) */
	}
	    
    if (gStopRls ||
        (gSleeping 
        && (!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPDisconnectOnSleep, &lval)
            || lval)))
        return EIO;	// not the right time to dial

    // reset setup flag
    ppp->flags &= ~FLAG_CONFIGCHANGEDNOW;
    ppp->flags &= ~FLAG_CONFIGCHANGEDLATER;

	// reset autodial flag;
    ppp->flags &= ~FLAG_FIRSTDIAL;

    switch (ppp->phase) {
        case PPP_IDLE:
            break;

        case PPP_DORMANT:	// kill dormant process and post connection flag
        case PPP_HOLDOFF:
            my_CFRelease(ppp->newconnectopts);
            ppp->newconnectopts = options;
            ppp->newconnectuid = uid;
            ppp->newconnectuid = gid;
            ppp->newconnectbootstrap = bootstrap;
            my_CFRetain(ppp->newconnectopts);

            ppp_disconnect(ppp, 0, SIGTERM);
            ppp->flags |= FLAG_CONNECT;
            if (client)
                add_client(ppp, client, autoclose);
            return 0;

        default:
            if (client) {
                if (my_CFEqual(options, ppp->connectopts)) {
                    add_client(ppp, client, autoclose);
                    return 0;
                }
            }
            return EIO;	// not the right time to dial
    }

    ppp->laststatus =  EXIT_FATAL_ERROR;
    ppp->lastdevstatus = 0;

    service = copyService(kSCDynamicStoreDomainSetup, ppp->serviceID);
    if (!service)
        goto end;	// that's bad...

    // create arguments and fork pppd 
    for (i = 0; i < MAXARG; i++) 
        cmdarg[i] = 0;
    addparam(cmdarg, &argi, PPPD_PRGM);
    addparam(cmdarg, &argi, "serviceid");
    addparam(cmdarg, &argi, ppp->sid);
    addparam(cmdarg, &argi, "controlled");

    if ((socketpair(AF_LOCAL, SOCK_STREAM, 0, ppp->controlfd) == -1) 
		|| (socketpair(AF_LOCAL, SOCK_STREAM, 0, ppp->statusfd) == -1))
        goto end;

	ppp->pid = _SCDPluginExecCommand2(exec_callback, (void*)ppp_makeref(ppp), uid, gid, PATH_PPPD, cmdarg, exec_postfork, (void*)ppp_makeref(ppp));
    if (ppp->pid == -1)
        goto end;

    // send options to pppd using the pipe
    if (send_pppd_params(ppp, service, options, dialondemand) == -1) {
        kill(ppp->pid, SIGTERM);
        goto end;
    }
    
    // all options have been sent, close the pipe.
    //my_close(ppp->controlfd[WRITE]);
    //ppp->controlfd[WRITE] = -1;

    // add the pipe to runloop
    ppp_socket_create_client(ppp->statusfd[READ], 1, 0, 0);

    if (client)
        add_client(ppp, client, autoclose);
        
    ppp->laststatus = EXIT_OK;
    ppp_updatephase(ppp, PPP_INITIALIZE);

    if (dialondemand)
        ppp->flags |= FLAG_DIALONDEMAND;
	else
		ppp->flags &= ~FLAG_DIALONDEMAND;

    ppp->connectopts = options;
    my_CFRetain(ppp->connectopts);

    ppp->uid = uid;
    ppp->gid = gid;
    ppp->bootstrap = bootstrap;

end:
    
    if (service)
        CFRelease(service);

    for (i = 0; i < argi; i++)
        free(cmdarg[i]);

    if (ppp->pid == -1) {
        
        my_close(ppp->statusfd[READ]);
        ppp->statusfd[READ] = -1;
        my_close(ppp->statusfd[WRITE]);
        ppp->statusfd[WRITE] = -1;
        my_close(ppp->controlfd[READ]);
        ppp->controlfd[READ] = -1;
        my_close(ppp->controlfd[WRITE]);
        ppp->controlfd[WRITE] = -1;
        
        display_error(ppp);
    }

    return ppp->laststatus;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
exec_postfork(pid_t pid, void *arg)
{
    struct ppp 	*ppp = ppp_findbyref((u_int32_t)arg);

    if (pid) {
        /* if parent */

        int	yes	= 1;

        my_close(ppp->controlfd[READ]);
        ppp->controlfd[READ] = -1;
        my_close(ppp->statusfd[WRITE]);
        ppp->statusfd[WRITE] = -1;
        if (ioctl(ppp->controlfd[WRITE], FIONBIO, &yes) == -1) {
//           printf("ioctl(,FIONBIO,): %s\n", strerror(errno));
        }

    } else {
        /* if child */

        int	i;

		setup_bootstrap_port();

        my_close(ppp->controlfd[WRITE]);
        ppp->controlfd[WRITE] = -1;
        my_close(ppp->statusfd[READ]);
        ppp->statusfd[READ] = -1;

        if (ppp->controlfd[READ] != STDIN_FILENO) {
            dup2(ppp->controlfd[READ], STDIN_FILENO);
        }

        if (ppp->statusfd[WRITE] != STDOUT_FILENO) {
            dup2(ppp->statusfd[WRITE], STDOUT_FILENO);
        }

        close(STDERR_FILENO);
        open(_PATH_DEVNULL, O_RDWR, 0);

        /* close any other open FDs */
        for (i = getdtablesize() - 1; i > STDERR_FILENO; i--) close(i);
    }

    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void exec_callback(pid_t pid, int status, struct rusage *rusage, void *context)
{
    struct ppp 	*ppp = ppp_findbyref((u_int32_t)context);
    u_int32_t	lval, failed = 0;
	int exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	
	if (exitcode < 0) {
		// ignore this case, just change phase
	}
	else if (exitcode > EXIT_PEER_NOT_AUTHORIZED) {
		// pppd exited because of a crash
		ppp_updatestatus(ppp, 127, 0);
	}
	else if (ppp->phase == PPP_INITIALIZE) {
        // an error occured and status has not been updated
        // happens for example when an error is encountered while parsing arguments
		failed = 1;
        ppp_updatestatus(ppp, exitcode, 0);
    }

    // call the change phae function
    ppp_updatephase(ppp, PPP_IDLE);

    // close file descriptors
    //statusfd is closed by the run loop
    //my_close(ppp->statusfd[READ]);
    ppp->statusfd[READ] = -1;
    my_close(ppp->controlfd[WRITE]);
    ppp->controlfd[WRITE] = -1;
    my_CFRelease(ppp->connectopts);
    ppp->connectopts = 0;

    if (ppp->flags & FLAG_FREE) {
        ppp->flags &= ~FLAG_FREE;
        dispose_service(ppp);
        ppp = 0;
    }

    if (gSleeping && !will_sleep(1)) {
        if (gSleepNotification) {
            CFUserNotificationCancel(gSleepNotification);
            CFRelease(gSleepNotification);
            gSleepNotification = 0;
        }
        IOAllowPowerChange(gIOPort, gSleepArgument);
    }

    if (ppp == 0)
        return;
        
    // check if configd is going away
    if (gStopRls && is_idle()) {
        // we are done
        CFRunLoopSourceSignal(gStopRls);
        return;
    }
    
    // now reconnect if necessary    
	
    if (ppp->flags & FLAG_CONNECT) {        
        ppp_connect(ppp, ppp->newconnectopts, 0, 0, 0, ppp->newconnectuid, ppp->newconnectgid, ppp->newconnectbootstrap);
        my_CFRelease(ppp->newconnectopts);
        ppp->newconnectopts = 0;
		ppp->newconnectuid = 0;
		ppp->newconnectgid = 0;
		ppp->newconnectbootstrap = 0;
        ppp->flags &= ~FLAG_CONNECT;
    }
    else {
        // if config has changed, or ppp was previously a manual connection, then rearm dialondemand if necessary
		if (failed == 0
			&& ((ppp->flags & (FLAG_CONFIGCHANGEDNOW + FLAG_CONFIGCHANGEDLATER)) || !(ppp->flags & FLAG_DIALONDEMAND))
            && (getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
            kSCEntNetPPP, kSCPropNetPPPDialOnDemand, &lval) && lval)
                && (!getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPDisconnectOnLogout, &lval) || !lval
                || gLoggedInUser)) {
            ppp_connect(ppp, 0, ppp->flags & FLAG_CONFIGCHANGEDNOW ? 1 : 3, 0, 0, 0, 0, 0);
        }
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_disconnect(struct ppp *ppp, void *client, int signal)
{

    /* arbitration mechanism : disconnects only when no client is using it */
    if (client) {
        if (get_client(ppp, client))
            remove_client(ppp, client);

        /* check if we have at least one client */
        if (TAILQ_FIRST(&ppp->client_head))
            return 0;
    }
    else {
        remove_all_clients(ppp);
    }
    
	/* 
		signal is either SIGHUP or SIGTERM 
		SIGHUP will only disconnect the link
		SIGTERM will terminate pppd
	*/
	if (ppp->flags & (FLAG_CONFIGCHANGEDNOW + FLAG_CONFIGCHANGEDLATER))
		signal = SIGTERM;
	
    // anticipate next phase
    switch (ppp->phase) {
    
        case PPP_IDLE:
            return 0;

        case PPP_DORMANT:
			/* SIGHUP has no effect on effect on DORMANT phase */
			if (signal == SIGHUP)
				return 0;
            break;

        case PPP_HOLDOFF:
			/* we don't want SIGHUP to stop the HOLDOFF phase */
			if (signal == SIGHUP)
				return 0;
            break;
            
        case PPP_DISCONNECTLINK:
        case PPP_TERMINATE:
            break;
            
        case PPP_INITIALIZE:
            ppp->flags &= ~FLAG_CONNECT;
            // no break;
            
        case PPP_CONNECTLINK:
            ppp_updatephase(ppp, PPP_DISCONNECTLINK);
            break;
        
        default:
            ppp_updatephase(ppp, PPP_TERMINATE);
    }

    kill(ppp->pid, signal);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_suspend(struct ppp *ppp)
{

    if (ppp->phase != PPP_IDLE)
        kill(ppp->pid, SIGTSTP);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_resume(struct ppp *ppp)
{
    if (ppp->phase != PPP_IDLE)
        kill(ppp->pid, SIGCONT);

    return 0;
}

/* -----------------------------------------------------------------------------
status change for this ppp occured
----------------------------------------------------------------------------- */
void ppp_updatestatus(struct ppp *ppp, int status, int devstatus)
{

    ppp->laststatus = status;
    ppp->lastdevstatus = devstatus;

    display_error(ppp);
}

/* -----------------------------------------------------------------------------
phase change for this ppp occured
----------------------------------------------------------------------------- */
void ppp_updatephase(struct ppp *ppp, int phase)
{

  /* check if update is received pppd has  exited */
  if (ppp->statusfd[READ] == -1)
      return;


    /* check for new phase */
    if (phase == ppp->phase)
        return;
    
    ppp->phase = phase;
    client_notify(ppp->serviceID, ppp->sid, ppp_makeref(ppp), ppp->phase, 0, CLIENT_FLAG_NOTIFY_STATUS);

    switch (ppp->phase) {
            
        case PPP_RUNNING:
            ppp->ifname[0] = 0;
            getStringFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
                    kSCEntNetPPP, kSCPropInterfaceName, ppp->ifname, sizeof(ppp->ifname));
            break;
            
        case PPP_DORMANT:
            ppp->ifname[0] = 0;
            getStringFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
                    kSCEntNetPPP, kSCPropInterfaceName, ppp->ifname, sizeof(ppp->ifname));
            // no break;

        case PPP_HOLDOFF:

            /* check if setup has changed */
            if (ppp->flags & FLAG_CONFIGCHANGEDLATER)
                ppp_disconnect(ppp, 0, SIGTERM);
            break;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_getstatus(struct ppp *ppp, void **reply, u_int16_t *replylen)
{
    struct ppp_status 		*stat;
    struct ifpppstatsreq 	rq;
    int		 		s;
    u_int32_t			retrytime, conntime, disconntime, curtime;

    *reply = my_Allocate(sizeof(struct ppp_status));
    if (*reply == 0) {
        return ENOMEM;
    }
    stat = (struct ppp_status *)*reply;

    bzero (stat, sizeof (struct ppp_status));
    switch (ppp->phase) {
        case PPP_DORMANT:
        case PPP_HOLDOFF:
            stat->status = PPP_IDLE;		// Dial on demand does not exist in the api
            break;
        default:
            stat->status = ppp->phase;
    }

    switch (stat->status) {
        case PPP_RUNNING:
        case PPP_ONHOLD:

            s = socket(AF_INET, SOCK_DGRAM, 0);
            if (s < 0) {
            	my_Deallocate(*reply, sizeof(struct ppp_status));
                return errno;
            }
    
            bzero (&rq, sizeof (rq));
    
            strncpy(rq.ifr_name, ppp->ifname, IFNAMSIZ);
            if (ioctl(s, SIOCGPPPSTATS, &rq) < 0) {
                close(s);
            	my_Deallocate(*reply, sizeof(struct ppp_status));
                return errno;
            }
    
            close(s);

            conntime = 0;
            getNumberFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPConnectTime, &conntime);
            disconntime = 0;
            getNumberFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPDisconnectTime, &disconntime);

            curtime = mach_absolute_time() * gTimeScaleSeconds;
            if (conntime)
				stat->s.run.timeElapsed = curtime - conntime;
            if (!disconntime)	// no limit...
     	       stat->s.run.timeRemaining = 0xFFFFFFFF;
            else
      	      stat->s.run.timeRemaining = (disconntime > curtime) ? disconntime - curtime : 0;

            stat->s.run.outBytes = rq.stats.p.ppp_obytes;
            stat->s.run.inBytes = rq.stats.p.ppp_ibytes;
            stat->s.run.inPackets = rq.stats.p.ppp_ipackets;
            stat->s.run.outPackets = rq.stats.p.ppp_opackets;
            stat->s.run.inErrors = rq.stats.p.ppp_ierrors;
            stat->s.run.outErrors = rq.stats.p.ppp_ierrors;
            break;
            
        case PPP_WAITONBUSY:
        
            stat->s.busy.timeRemaining = 0;
            retrytime = 0;
            getNumberFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPRetryConnectTime, &retrytime);
            if (retrytime) {
                curtime = mach_absolute_time() * gTimeScaleSeconds;
                stat->s.busy.timeRemaining = (curtime < retrytime) ? retrytime - curtime : 0;
            }
            break;
         
        default:
            stat->s.disc.lastDiscCause = ppp_translate_error(ppp->subtype, ppp->laststatus, ppp->lastdevstatus);
    }

    *replylen = sizeof(struct ppp_status);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_copyextendedstatus(struct ppp *ppp, void **reply, u_int16_t *replylen)
{
    CFMutableDictionaryRef	statusdict = 0, dict = 0;
    CFDataRef			dataref = 0;
    void			*dataptr = 0;
    u_int32_t			datalen = 0;
    
    if ((statusdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        goto fail;

    /* create and add PPP dictionary */
    if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        goto fail;
    
    AddNumber(dict, kSCPropNetPPPStatus, ppp->phase);
    
    if (ppp->phase != PPP_IDLE)
        AddStringFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPCommRemoteAddress, dict);

    switch (ppp->phase) {
        case PPP_RUNNING:
        case PPP_ONHOLD:

            AddNumberFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPConnectTime, dict);
            AddNumberFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPDisconnectTime, dict);
            AddNumberFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPCompressionPField, dict);
            AddNumberFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPCompressionACField, dict);
            AddNumberFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPMRU, dict);
            AddNumberFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPMTU, dict);
            AddNumberFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPReceiveACCM, dict);
            AddNumberFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPTransmitACCM, dict);
            AddNumberFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPIPCPCompressionVJ, dict);
            break;
            
        case PPP_WAITONBUSY:
            AddNumberFromState(ppp->serviceID, kSCEntNetPPP, kSCPropNetPPPRetryConnectTime, dict);
            break;
         
        case PPP_DORMANT:
            break;
            
        default:
            AddNumber(dict, kSCPropNetPPPLastCause, ppp->laststatus);
            AddNumber(dict, kSCPropNetPPPDeviceLastCause, ppp->lastdevstatus);
    }

    CFDictionaryAddValue(statusdict, kSCEntNetPPP, dict);
    CFRelease(dict);

    /* create and add Modem dictionary */
    if (ppp->subtype == PPP_TYPE_SERIAL
        && (ppp->phase == PPP_RUNNING || ppp->phase == PPP_ONHOLD)) {
        if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
            goto fail;

        AddNumberFromState(ppp->serviceID, kSCEntNetModem, kSCPropNetModemConnectSpeed, dict);
            
        CFDictionaryAddValue(statusdict, kSCEntNetModem, dict);
        CFRelease(dict);
    }

    /* create and add IPv4 dictionary */
    if (ppp->phase == PPP_RUNNING || ppp->phase == PPP_ONHOLD) {
        dict = (CFMutableDictionaryRef)copyEntity(kSCDynamicStoreDomainState, ppp->serviceID, kSCEntNetIPv4);
        if (dict) {
            CFDictionaryAddValue(statusdict, kSCEntNetIPv4, dict);
            CFRelease(dict);
        }
    }

    /* We are done, now serialize it */
    if ((dataref = Serialize(statusdict, &dataptr, &datalen)) == 0)
        goto fail;
    
    *reply = my_Allocate(datalen);
    if (*reply == 0)
        goto fail;

    bcopy(dataptr, *reply, datalen);    
    CFRelease(statusdict);
    CFRelease(dataref);
    *replylen = datalen;
    return 0;

fail:
    if (statusdict)
        CFRelease(statusdict);
    if (dict)
        CFRelease(dict);
    if (dataref)
        CFRelease(dataref);
    return ENOMEM;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_copystatistics(struct ppp *ppp, void **reply, u_int16_t *replylen)
{
    CFMutableDictionaryRef	statsdict = 0, dict = 0;
    CFDataRef			dataref = 0;
    void				*dataptr = 0;
    u_int32_t			datalen = 0;
    int					s = -1;
    struct ifpppstatsreq 	rq;
	int					error = 0;
	
	if (ppp->phase != PPP_RUNNING
		&& ppp->phase != PPP_ONHOLD)
			return EINVAL;
			
    if ((statsdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
		goto fail;
	}

    /* create and add PPP dictionary */
    if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
		goto fail;
	}
    
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		error = errno;
        goto fail;
	}

	bzero (&rq, sizeof (rq));

	strncpy(rq.ifr_name, ppp->ifname, IFNAMSIZ);
	if (ioctl(s, SIOCGPPPSTATS, &rq) < 0) {
		error = errno;
        goto fail;
	}

	close(s);
	s = -1;

	AddNumber(dict, kSCNetworkConnectionBytesIn, rq.stats.p.ppp_ibytes);
	AddNumber(dict, kSCNetworkConnectionBytesOut, rq.stats.p.ppp_obytes);
	AddNumber(dict, kSCNetworkConnectionPacketsIn, rq.stats.p.ppp_ipackets);
	AddNumber(dict, kSCNetworkConnectionPacketsOut, rq.stats.p.ppp_opackets);
	AddNumber(dict, kSCNetworkConnectionErrorsIn, rq.stats.p.ppp_ierrors);
	AddNumber(dict, kSCNetworkConnectionErrorsOut, rq.stats.p.ppp_ierrors);

    CFDictionaryAddValue(statsdict, kSCEntNetPPP, dict);
    CFRelease(dict);

    /* We are done, now serialize it */
    if ((dataref = Serialize(statsdict, &dataptr, &datalen)) == 0) {
		error = ENOMEM;
        goto fail;
	}
    
    *reply = my_Allocate(datalen);
    if (*reply == 0) {
		error = ENOMEM;
        goto fail;
	}

    bcopy(dataptr, *reply, datalen);    

    CFRelease(statsdict);
    CFRelease(dataref);
    *replylen = datalen;
    return 0;

fail:
	if (s != -1)
		close(s);
    if (statsdict)
        CFRelease(statsdict);
    if (dict)
        CFRelease(dict);
    if (dataref)
        CFRelease(dataref);
    return error;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_getconnectsystemdata(struct ppp *ppp, void **reply, u_int16_t *replylen)
{
	CFDictionaryRef	service = NULL;
    CFDataRef			dataref = NULL;
    void			*dataptr = 0;
    u_int32_t			datalen = 0;
	int				err = 0;

	service = copyService(kSCDynamicStoreDomainSetup, ppp->serviceID);
    if (service == 0) {
        // no data
        *replylen = 0;
        return 0;
    }
    
    if ((dataref = Serialize(service, &dataptr, &datalen)) == 0) {
		err = ENOMEM;
		goto end;
    }
    
    *reply = my_Allocate(datalen);
    if (*reply == 0) {
		err = ENOMEM;
		goto end;
    }
    else {
        bcopy(dataptr, *reply, datalen);
        *replylen = datalen;
    }

end:
    if (service)
		CFRelease(service);
    if (dataref)
		CFRelease(dataref);
    return err;
 }

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_getconnectdata(struct ppp *ppp, void **reply, u_int16_t *replylen, int all)
{
    CFDataRef			dataref = NULL;
    void			*dataptr = 0;
    u_int32_t			datalen = 0;
    CFDictionaryRef		opts;
    CFMutableDictionaryRef	mdict = NULL, mdict1;
    CFDictionaryRef	dict;
	int err = 0;
        
    /* return saved data */
    opts = ppp->connectopts;

    if (opts == 0) {
        // no data
        *replylen = 0;
        return 0;
    }
    
	if (!all) {
		/* special code to remove secret information */

		mdict = CFDictionaryCreateMutableCopy(0, 0, opts);
		if (mdict == 0) {
			// no data
			*replylen = 0;
			return 0;
		}
		
		dict = CFDictionaryGetValue(mdict, kSCEntNetPPP);
		if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
			mdict1 = CFDictionaryCreateMutableCopy(0, 0, dict);
			if (mdict1) {
				CFDictionaryRemoveValue(mdict1, kSCPropNetPPPAuthPassword);
				CFDictionarySetValue(mdict, kSCEntNetPPP, mdict1);
				CFRelease(mdict1);
			}
		}

		dict = CFDictionaryGetValue(mdict, kSCEntNetL2TP);
		if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
			mdict1 = CFDictionaryCreateMutableCopy(0, 0, dict);
			if (mdict1) {
				CFDictionaryRemoveValue(mdict1, kSCPropNetL2TPIPSecSharedSecret);
				CFDictionarySetValue(mdict, kSCEntNetL2TP, mdict1);
				CFRelease(mdict1);
			}
		}

		dict = CFDictionaryGetValue(mdict, kSCEntNetIPSec);
		if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
			mdict1 = CFDictionaryCreateMutableCopy(0, 0, dict);
			if (mdict1) {
				CFDictionaryRemoveValue(mdict1, kSCPropNetIPSecSharedSecret);
				CFDictionarySetValue(mdict, kSCEntNetIPSec, mdict1);
				CFRelease(mdict1);
			}
		}
	}

    if ((dataref = Serialize(all ? opts : mdict, &dataptr, &datalen)) == 0) {
		err = ENOMEM;
        goto end;
    }
    
    *reply = my_Allocate(datalen);
    if (*reply == 0) {
		err = ENOMEM;
        goto end;
    }
    else {
        bcopy(dataptr, *reply, datalen);
        *replylen = datalen;
    }

end:
    if (mdict)
		CFRelease(mdict);
    if (dataref)
		CFRelease(dataref);
    return err;
}

/* -----------------------------------------------------------------------------
translate a pppd native cause into a PPP API cause
----------------------------------------------------------------------------- */
u_int32_t ppp_translate_error(u_int16_t subtype, u_int32_t native_ppp_error, u_int32_t native_dev_error)
{
    u_int32_t	error = PPP_ERR_GEN_ERROR; 
    
    switch (native_ppp_error) {
        case EXIT_USER_REQUEST:
            error = 0;
            break;
        case EXIT_CONNECT_FAILED:
            error = PPP_ERR_GEN_ERROR;
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
            error = PPP_ERR_SESSIONTIMEOUT;
            break;
        case EXIT_LOOPBACK:
            error = PPP_ERR_LOOPBACK;
            break;
        case EXIT_PEER_DEAD:
            error = PPP_ERR_PEERDEAD;
            break;
        case EXIT_OK:
            error = PPP_ERR_DISCBYPEER;
            break;
        case EXIT_HANGUP:
            error = PPP_ERR_DISCBYDEVICE;
            break;
    }
    
    // override with a more specific error
    if (native_dev_error) {
        switch (subtype) {
            case PPP_TYPE_SERIAL:
                switch (native_dev_error) {
                    case EXIT_PPPSERIAL_NOCARRIER:
                        error = PPP_ERR_MOD_NOCARRIER;
                        break;
                    case EXIT_PPPSERIAL_NONUMBER:
                        error = PPP_ERR_MOD_NONUMBER;
                        break;
                    case EXIT_PPPSERIAL_BADSCRIPT:
                        error = PPP_ERR_MOD_BADSCRIPT;
                        break;
                    case EXIT_PPPSERIAL_BUSY:
                        error = PPP_ERR_MOD_BUSY;
                        break;
                    case EXIT_PPPSERIAL_NODIALTONE:
                        error = PPP_ERR_MOD_NODIALTONE;
                        break;
                    case EXIT_PPPSERIAL_ERROR:
                        error = PPP_ERR_MOD_ERROR;
                        break;
                    case EXIT_PPPSERIAL_NOANSWER:
                        error = PPP_ERR_MOD_NOANSWER;
                        break;
                    case EXIT_PPPSERIAL_HANGUP:
                        error = PPP_ERR_MOD_HANGUP;
                        break;
                    default :
                        error = PPP_ERR_CONNSCRIPTFAILED;
                }
                break;
    
            case PPP_TYPE_PPPoE:
                // need to handle PPPoE specific error codes
                break;
        }
    }
    
    return error;
}

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
    client arbritration mechanism 
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_clientgone(void *client)
{
    struct ppp		*ppp;
    struct ppp_client 	*pppclient;

    /* arbitration mechanism */
    TAILQ_FOREACH(ppp, &ppp_head, next) {
        pppclient = get_client(ppp, client);
        if (pppclient && pppclient->autoclose) {
            ppp_disconnect(ppp, client, SIGTERM);
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int add_client(struct ppp *ppp, void *client, int autoclose)
{
    struct ppp_client *pppclient;
    
    pppclient = malloc(sizeof(struct ppp_client));
    if (pppclient == 0)
        return -1; // very bad...

    pppclient->client = client;
    pppclient->autoclose = autoclose;
    TAILQ_INSERT_TAIL(&ppp->client_head, pppclient, next);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int remove_client(struct ppp *ppp, void *client)
{
    struct ppp_client *pppclient;

    TAILQ_FOREACH(pppclient, &ppp->client_head, next) {
        if (pppclient->client == client) {
            TAILQ_REMOVE(&ppp->client_head, pppclient, next);
            free(pppclient);
            return 0;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
struct ppp_client *get_client(struct ppp *ppp, void *client)
{
    struct ppp_client *pppclient;

    TAILQ_FOREACH(pppclient, &ppp->client_head, next)
        if (pppclient->client == client)
            return pppclient;

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int remove_all_clients(struct ppp *ppp)
{
    struct ppp_client *pppclient;

    while (pppclient = TAILQ_FIRST(&ppp->client_head)) {
        TAILQ_REMOVE(&ppp->client_head, pppclient, next);
        free(pppclient);
    }
    return 0;
}
