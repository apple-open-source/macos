/*
 * Copyright (c) 2000-2011 Apple Computer, Inc. All rights reserved.
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
#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFUserNotification.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#include <SystemConfiguration/SCPreferencesPathKey.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#include <mach/mach_time.h>
#include <mach/task_special_ports.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/boolean.h>
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#include <notify.h>
#include <sys/sysctl.h>
#include <pthread.h>
#if TARGET_OS_EMBEDDED
#include <MobileInstallation/MobileInstallation.h>
#endif			
#include <NSSystemDirectories.h>
#include <ifaddrs.h>
#include <SystemConfiguration/SCNetworkSignature.h>
#include <sys/proc.h>

#include "scnc_mach_server.h"
#include "scnc_main.h"
#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "scnc_client.h"
#include "ipsec_manager.h"
#include "sbslauncher.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "ppp_socket_server.h"
#include "scnc_utils.h"
#include "if_ppplink.h"
#include "PPPControllerPriv.h"
#include "pppd.h"


/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */

enum {
    do_nothing = 0,
    do_process,
    do_close,
    do_error
};

#define ICON 	"NetworkConnect.icns"


#if TARGET_OS_EMBEDDED
#define TIMEOUT_EDGE	2 /* give 2 second after edge is ready to propagate all network dns notification. */
#endif

/* -----------------------------------------------------------------------------
forward declarations
----------------------------------------------------------------------------- */


static int init_things();
static void iosleep_notifier(void * x, io_service_t y, natural_t messageType, void *messageArgument);
static void store_notifier(SCDynamicStoreRef session, CFArrayRef changedKeys, void *info);
static void print_services();
static int update_service(CFStringRef serviceID);
static void finish_update_services();
static struct service * new_service(CFStringRef serviceID , CFStringRef typeRef, CFStringRef subtypeRef);
static int dispose_service(struct service *serv);
static u_short findfreeunit(u_short type, u_short subtype);
static int ondemand_add_service(struct service *serv);
static int ondemand_remove_service(struct service *serv);
static void post_ondemand_token(uint64_t state64);

static int add_client(struct service *serv, void *client, int autoclose);
static int remove_client(struct service *serv, void *client);
static struct service_client *get_client(struct service *serv, void *client);
static int  remove_all_clients(struct service *serv);

static int can_sleep();
static int will_sleep(int checking);
static void wake_up();
#if	!TARGET_OS_EMBEDDED
static void log_out();
static void log_in();
static void log_switch();
#endif
static void ipv4_state_changed();

static void reorder_services();

extern void proc_name(int pid, char * buf, int size);


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

CFStringRef 		gPluginsDir = 0;
CFStringRef 		gResourcesDir = 0;
CFBundleRef 		gBundleRef = 0;
CFURLRef			gBundleURLRef = 0;
CFStringRef		gBundleDir = 0;
CFURLRef			gIconURLRef = 0;
CFStringRef		gIconDir = 0;
CFMutableArrayRef	gVPNBundlesRef = 0;
io_connect_t		gIOPort;
int					gSleeping;
long				gSleepArgument;
CFUserNotificationRef 	gSleepNotification;
time_t				gSleptAt = -1;
time_t				gWokeAt = -1;
uint64_t			gWakeUpTime = 0;
double				gSleepWakeTimeout = (2 * 3600);	//2 hours... should be configurable
double				gTimeScaleSeconds;
CFRunLoopSourceRef 	gStopRls;
CFStringRef			gLoggedInUser = NULL;
uid_t				gLoggedInUserUID = 0;
SCDynamicStoreRef	gDynamicStore = NULL;
char				*gIPSecAppVersion = NULL;
int					gNotifyOnDemandToken = -1;

int					gSCNCVerbose = 0;
int					gSCNCDebug = 0;
CFRunLoopRef		gControllerRunloop = NULL;
CFRunLoopRef		gPluginRunloop = NULL;
CFRunLoopSourceRef	gTerminalrls = NULL;

#if TARGET_OS_EMBEDDED
int					gNattKeepAliveInterval = -1;
#endif


TAILQ_HEAD(, service) 	service_head;

#if !TARGET_OS_EMBEDDED
static vproc_transaction_t gController_vt = NULL;		/* opaque handle used to track outstanding transactions, used by instant off */
#endif
static u_int32_t           gWaitForPrimaryService = 0;

/* -----------------------------------------------------------------------------
 terminate_all()
 ----------------------------------------------------------------------------- */
void terminate_all()
{
	/* receive signal from  main thread, stop connection */
    struct service 		*serv;
	
	CFRunLoopSourceInvalidate(gTerminalrls);
	CFRelease(gTerminalrls);
    TAILQ_FOREACH(serv, &service_head, next) {
		scnc_stop(serv, 0, SIGTERM);
    }
	
	allow_stop();    	
}

/* -----------------------------------------------------------------------------
 pppcntl_run_thread()
 ----------------------------------------------------------------------------- */
void *pppcntl_run_thread(void *arg)
{
	
	if (ppp_mach_start_server())
		pthread_exit( (void*) EXIT_FAILURE);
    if (ppp_socket_start_server())
		pthread_exit( (void*) EXIT_FAILURE);
    if (client_init_all())
		pthread_exit( (void*) EXIT_FAILURE);
	
	init_things();
	CFRunLoopRun();
	return NULL;
}

/* -----------------------------------------------------------------------------
load plugin entry point, called by configd
----------------------------------------------------------------------------- */
void load(CFBundleRef bundle, Boolean debug)
{
	pthread_attr_t	tattr;
	pthread_t	pppcntl_thread;
	CFRunLoopSourceContext	rlContext = { 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, terminate_all};

    gBundleRef = bundle;
	gSCNCVerbose = _sc_verbose | debug;
	gSCNCDebug = debug;
	
	gPluginRunloop = CFRunLoopGetCurrent();
	
	gTerminalrls = CFRunLoopSourceCreate(NULL, 0, &rlContext);
	if (!gTerminalrls){
		SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: cannot create signal gTerminalrls in load"));
		return;
	}
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&pppcntl_thread, &tattr, pppcntl_run_thread, NULL)) {
		CFRelease(gTerminalrls);
		gTerminalrls = NULL;
		return;
	}

	pthread_attr_destroy(&tattr);
	
    CFRetain(bundle);
}

/* -----------------------------------------------------------------------------
stop plugin entry point, called by configd
----------------------------------------------------------------------------- */
void stop(CFRunLoopSourceRef stopRls)
{
	
	
    if (gStopRls)
        // should not happen
        return;
	
	gStopRls = stopRls;
	if (gTerminalrls)
		CFRunLoopSourceSignal(gTerminalrls);
	if (gControllerRunloop)
		CFRunLoopWakeUp(gControllerRunloop);

}

/* -----------------------------------------------------------------------------
now is a good time to stop if necessary
----------------------------------------------------------------------------- */
int allow_stop()
{
    struct service 	*serv;
	SCNetworkConnectionStatus status;
	
	// check if configd is going away
    if (gStopRls) {
            
		TAILQ_FOREACH(serv, &service_head, next) {
			switch (serv->type) {
				case TYPE_PPP:  status = ppp_getstatus(serv); break;
				case TYPE_IPSEC:  status = ipsec_getstatus(serv); break;
				default: status = kSCNetworkConnectionInvalid; break;
			}
			if (status != kSCNetworkConnectionDisconnected)
				return 0;
		}
			
        // we are done
        CFRunLoopSourceSignal(gStopRls);
		if ( gPluginRunloop )
			CFRunLoopWakeUp(gPluginRunloop);
        return 1;
    }
	return 0;
}

/* -----------------------------------------------------------------------------
service can be disposed now if necessary
----------------------------------------------------------------------------- */
int allow_dispose(struct service *serv)
{
    if (serv->flags & FLAG_FREE) {
        dispose_service(serv);
		return 1;
    }
	return 0;
}



/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int init_things()
{
    CFURLRef 		urlref, absurlref;
	IONotificationPortRef	notify;
    io_object_t			iterator;
    mach_timebase_info_data_t   timebaseInfo;
    CFStringRef         key = NULL, setup = NULL, entity = NULL;
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
        kSCEntNetIPSec,
#if TARGET_OS_EMBEDDED
		CFSTR("com.apple.payload"),
#endif
        NULL,
    };
	
	gBundleURLRef = CFBundleCopyBundleURL(gBundleRef);
 	absurlref = CFURLCopyAbsoluteURL(gBundleURLRef);
	if (absurlref) {
		gBundleDir = CFURLCopyPath(absurlref);
		CFRelease(absurlref);
	}
 
    // create plugins dir
    urlref = CFBundleCopyBuiltInPlugInsURL(gBundleRef);
    if (urlref) {
        absurlref = CFURLCopyAbsoluteURL(urlref);
		if (absurlref) {
            gPluginsDir = CFURLCopyPath(absurlref);
            CFRelease(absurlref);
        }
        CFRelease(urlref);
    }

    // create resources dir
    urlref = CFBundleCopyResourcesDirectoryURL(gBundleRef);
    if (urlref) {
        absurlref = CFURLCopyAbsoluteURL(urlref);
		if (absurlref) {
            gResourcesDir = CFURLCopyPath(absurlref);
            CFRelease(absurlref);
        }
        CFRelease(urlref);
    }

    // create misc notification strings
    gIconURLRef = CFBundleCopyResourceURL(gBundleRef, CFSTR(ICON), NULL, NULL);
 	if (gIconURLRef) {
		absurlref = CFURLCopyAbsoluteURL(gIconURLRef);
		if (absurlref) {
			gIconDir = CFURLCopyPath(absurlref);
			CFRelease(absurlref);
		}
	}
	
	
	post_ondemand_token(0);
	
	/* setup power management callback  */
    gSleeping = 0;
    gSleepNotification = 0;    
    gStopRls = 0;    

    gIOPort = IORegisterForSystemPower(0, &notify, iosleep_notifier, &iterator);
    if (gIOPort == 0) {
        SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: IORegisterForSystemPower failed"));
        goto fail;
    }
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                        IONotificationPortGetRunLoopSource(notify),
                        kCFRunLoopDefaultMode);
                        
	/* init time scale */
    if (mach_timebase_info(&timebaseInfo) != KERN_SUCCESS) {	// returns scale factor for ns
        SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: mach_timebase_info failed"));
        goto fail;
    }
    gTimeScaleSeconds = ((double) timebaseInfo.numer / (double) timebaseInfo.denom) / 1000000000;

    /* opens now our session to the Dynamic Store */
    if ((gDynamicStore = SCDynamicStoreCreate(0, CFSTR("SCNCController"), store_notifier, 0)) == NULL)
        goto fail;
    
    if ((rls = SCDynamicStoreCreateRunLoopSource(0, gDynamicStore, 0)) == NULL) 
        goto fail;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);


#if	!TARGET_OS_EMBEDDED
    gLoggedInUser = SCDynamicStoreCopyConsoleUser(gDynamicStore, &gLoggedInUserUID, 0);
#endif	// !TARGET_OS_EMBEDDED
	
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

#if	!TARGET_OS_EMBEDDED
	/* install the notifier for user login/logout */
    if ((key = SCDynamicStoreKeyCreateConsoleUser(0)) == NULL)
        goto fail;
    CFArrayAppendValue(keys, key);
    CFRelease(key);    
#endif	// !TARGET_OS_EMBEDDED

    /* add all the notification in one chunk */
    SCDynamicStoreSetNotificationKeys(gDynamicStore, keys, patterns);
	

	/* init list of services */
    TAILQ_INIT(&service_head);

    /* read the initial configured interfaces */
	entity = CFStringCreateWithFormat(NULL, NULL, CFSTR("(%@|%@)"), kSCEntNetPPP, kSCEntNetIPSec);
    key = CREATESERVICESETUP(entity);
    setup = CREATEPREFIXSETUP();
    if (key == NULL || setup == NULL)
        goto fail;
        
    services = SCDynamicStoreCopyKeyList(gDynamicStore, key);
    if (services == NULL)
        goto done;	// no service setup

    nb = CFArrayGetCount(services);
    for (i = 0; i < nb; i++) {
        CFStringRef serviceID;
        if (serviceID = parse_component(CFArrayGetValueAtIndex(services, i), setup)) {
		
            update_service(serviceID);            
            CFRelease(serviceID);
        }
    }
    
	reorder_services();
    finish_update_services();

	if (gSCNCVerbose)
		print_services();

done:    
    my_CFRelease(&entity);
    my_CFRelease(&services);
    my_CFRelease(&key);
    my_CFRelease(&setup);
    my_CFRelease(&keys);
    my_CFRelease(&patterns);
    return ret;
	
fail:
    my_CFRelease(&gDynamicStore);
    ret = -1;
    goto done;
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
            time(&gSleptAt);
             
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
                        CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, CFSTR("Network Connection"));
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
            time(&gWokeAt);
            wake_up();            
            break;
    }
    
}

/* -----------------------------------------------------------------------------
the configd cache/setup has changed
----------------------------------------------------------------------------- */
static
void store_notifier(SCDynamicStoreRef session, CFArrayRef changedKeys, void *info)
{
    CFStringRef		setup, ipsetupkey, ipstatekey;
    int				i, nb, doreorder = 0, dopostsetup = 0;
#if	!TARGET_OS_EMBEDDED
    CFStringRef		userkey;
#endif	// !TARGET_OS_EMBEDDED	
	
    if (changedKeys == NULL) 
        return;
    
    setup = CREATEPREFIXSETUP();        
#if	!TARGET_OS_EMBEDDED
    userkey = SCDynamicStoreKeyCreateConsoleUser(0);
#endif	// !TARGET_OS_EMBEDDED
    ipsetupkey = CREATEGLOBALSETUP(kSCEntNetIPv4);
    ipstatekey = CREATEGLOBALSTATE(kSCEntNetIPv4);
    
    if (setup == NULL || ipsetupkey == NULL || ipstatekey == NULL) {
        SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: cache_notifier can't allocate keys"));
        goto done;
    }
#if	!TARGET_OS_EMBEDDED
    if (userkey == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: cache_notifier can't allocate keys"));
		goto done;
    }
#endif	// !TARGET_OS_EMBEDDED	
	
    nb = CFArrayGetCount(changedKeys);
    for (i = 0; i < nb; i++) {

        CFStringRef	change, serviceID;
        
        change = CFArrayGetValueAtIndex(changedKeys, i);

#if	!TARGET_OS_EMBEDDED
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
#endif	// !TARGET_OS_EMBEDDED

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
            update_service(serviceID);
            CFRelease(serviceID);
            dopostsetup = 1;
            continue;
        }
        
    }

    if (doreorder)
        reorder_services();
    if (dopostsetup)
        finish_update_services();

	if (gSCNCVerbose)
		print_services();

done:
    my_CFRelease(&setup);
#if	!TARGET_OS_EMBEDDED
    my_CFRelease(&userkey);
#endif	// !TARGET_OS_EMBEDDED	
    my_CFRelease(&ipsetupkey);
    my_CFRelease(&ipstatekey);
    return;
}

/* -----------------------------------------------------------------------------
force reload services, to parse for new ones
----------------------------------------------------------------------------- */
static 
void reload_services(CFStringRef entity)
{
    CFStringRef         key = NULL, setup = NULL;
    CFArrayRef			services = NULL;
    int					i, nb;
  
    /* read the initial configured interfaces */
    key = CREATESERVICESETUP(entity);
    setup = CREATEPREFIXSETUP();
    if (key == NULL || setup == NULL)
        goto done;

    services = SCDynamicStoreCopyKeyList(gDynamicStore, key);
    if (services == NULL)
        goto done;	// no service setup

    nb = CFArrayGetCount(services);
    for (i = 0; i < nb; i++) {
        CFStringRef serviceID;
        if (serviceID = parse_component(CFArrayGetValueAtIndex(services, i), setup)) {
		
            update_service(serviceID);            
            CFRelease(serviceID);
        }
    }
    
	reorder_services();
    finish_update_services();

	if (gSCNCVerbose)
		print_services();

done:
    my_CFRelease(&key);
    my_CFRelease(&setup);
    my_CFRelease(&services);
}

/* -----------------------------------------------------------------------------
system is asking permission to sleep
return if sleep is authorized
----------------------------------------------------------------------------- */
static 
int can_sleep()
{
    struct service	*serv;
    u_int32_t       prevent = 0;
        
    TAILQ_FOREACH(serv, &service_head, next) {
		
		switch (serv->type) {
			case TYPE_PPP:  prevent = !ppp_can_sleep(serv); break;
			case TYPE_IPSEC:  prevent = !ipsec_can_sleep(serv); break;
		}
			
		if (prevent)
			break;
	}

    return !prevent;
}

/* -----------------------------------------------------------------------------
system is going to sleep
disconnect services and return if a delay is needed
----------------------------------------------------------------------------- */
static 
int will_sleep(int checking)
{
    u_int32_t		ret = 0;
    struct service	*serv;
            
    TAILQ_FOREACH(serv, &service_head, next) {
		switch (serv->type) {
			case TYPE_PPP:  ret |= ppp_will_sleep(serv, checking); break;
			case TYPE_IPSEC:  ret |= ipsec_will_sleep(serv, checking); break;
		}
    }
        
	return ret;
}

/* -----------------------------------------------------------------------------
system is waking up
----------------------------------------------------------------------------- */
static
void wake_up()
{
    struct service	*serv;

    TAILQ_FOREACH(serv, &service_head, next) {
		serv->flags |= FLAG_FIRSTDIAL;
		switch (serv->type) {
			case TYPE_PPP:  ppp_wake_up(serv); break;
			case TYPE_IPSEC:  ipsec_wake_up(serv); break;
		}
    } 
}

/* -----------------------------------------------------------------------------
system is allowed to sleep now
----------------------------------------------------------------------------- */
int allow_sleep()
{
	if (gSleeping && !will_sleep(1)) {
        if (gSleepNotification) {
            CFUserNotificationCancel(gSleepNotification);
            CFRelease(gSleepNotification);
            gSleepNotification = 0;
        }
        IOAllowPowerChange(gIOPort, gSleepArgument);
		return 1;
    }
	return 0;
}

/* -----------------------------------------------------------------------------
 service has started
 ----------------------------------------------------------------------------- */
void service_started(struct service *serv)
{      
	switch (serv->type) {
		case TYPE_PPP:  
#if !TARGET_OS_EMBEDDED
			/* transaction started */
			serv->vt = vproc_transaction_begin(NULL);
			if ( serv->vt == NULL)
				SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: vproc_transaction_begin rts NULL"));
#endif
			break;
		case TYPE_IPSEC:  
			break;
	}
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static void
service_ending_verify_primaryservice(struct service *serv)
{
    CFStringRef		newPrimary;

	if (gWaitForPrimaryService) 
		return; 	// already waiting

	// no need to wait for a new primary service in the case of PPPoE or PPPSerial
	if (serv->type == TYPE_PPP &&
        (serv->subtype == PPP_TYPE_PPPoE ||
         serv->subtype == PPP_TYPE_SERIAL ||
         serv->subtype == PPP_TYPE_SYNCSERIAL)) {
            return;
    }

    newPrimary = copyPrimaryService(gDynamicStore);
	if ((newPrimary == NULL) || 					// primary not back yet, or... 
		_SC_CFEqual(serv->serviceID, newPrimary)) {	// primary is still our service

#if !TARGET_OS_EMBEDDED
		gController_vt = vproc_transaction_begin(NULL);
		if (gController_vt)
#endif
			gWaitForPrimaryService = 1;

		SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: %s, waiting for PrimaryService. status = %x\n"),
			  __FUNCTION__, gWaitForPrimaryService);
    }
	
	if (newPrimary)
		CFRelease(newPrimary);
}

/* -----------------------------------------------------------------------------
service has ended
 ----------------------------------------------------------------------------- */
void service_ended(struct service *serv)
{

	/* perform service level cleanup */
#if TARGET_OS_EMBEDDED
	if (serv->cellular_timerref) {
		CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->cellular_timerref, kCFRunLoopCommonModes);
		my_CFRelease(&serv->cellular_timerref);
	}
	
	if (serv->cellularRLS) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->cellularRLS, kCFRunLoopCommonModes);
		my_CFRelease(&serv->cellularRLS);
	}
	if (serv->cellularPort) {
		CFMachPortInvalidate(serv->cellularPort);
		my_CFRelease(&serv->cellularPort);			
	}
	my_CFRelease(&serv->cellularConnection);
#endif

	
    service_ending_verify_primaryservice(serv);
	switch (serv->type) {
		case TYPE_PPP:  
#if !TARGET_OS_EMBEDDED
			/* transaction ends */
			if ( serv->vt)
				vproc_transaction_end(NULL, serv->vt);
#endif
			break;
		case TYPE_IPSEC:  
			break;
	}
}

#if	!TARGET_OS_EMBEDDED

/* -----------------------------------------------------------------------------
user has looged out
----------------------------------------------------------------------------- */
static 
void log_out()
{
    struct service	*serv;

    TAILQ_FOREACH(serv, &service_head, next) {
		switch (serv->type) {
			case TYPE_PPP:  ppp_log_out(serv); break;
			case TYPE_IPSEC:  ipsec_log_out(serv); break;
		}
    }
}

/* -----------------------------------------------------------------------------
user has logged in
----------------------------------------------------------------------------- */
static 
void log_in()
{
    struct service	*serv;

    TAILQ_FOREACH(serv, &service_head, next) {
		serv->flags |= FLAG_FIRSTDIAL;
		switch (serv->type) {
			case TYPE_PPP:  ppp_log_in(serv); break;
			case TYPE_IPSEC:  ipsec_log_in(serv); break;
		}
    }
}

/* -----------------------------------------------------------------------------
user has switched
----------------------------------------------------------------------------- */
static 
void log_switch()
{
    struct service	*serv;

    TAILQ_FOREACH(serv, &service_head, next) {
		serv->flags |= FLAG_FIRSTDIAL;
		switch (serv->type) {
			case TYPE_PPP:  ppp_log_switch(serv); break;
			case TYPE_IPSEC:  ipsec_log_switch(serv); break;
		}
    }
}

#endif

/* -----------------------------------------------------------------------------
ipv4 state has changed
----------------------------------------------------------------------------- */
static
void ipv4_state_changed()
{
    struct service *serv;
    int             found = 0;
    CFStringRef		newPrimary = NULL;
	
	if (gWaitForPrimaryService)
		newPrimary = copyPrimaryService(gDynamicStore);
	
    TAILQ_FOREACH(serv, &service_head, next) {
		serv->flags |= FLAG_FIRSTDIAL;
		switch (serv->type) {
			case TYPE_PPP:  ppp_ipv4_state_changed(serv); break;
			case TYPE_IPSEC:  ipsec_ipv4_state_changed(serv); break;
		}

        if (!found &&
            newPrimary &&
			_SC_CFEqual(serv->serviceID, newPrimary)) {
            found = 1;
        }
    }

	if (!found &&
        newPrimary &&
        gWaitForPrimaryService) {
		gWaitForPrimaryService = 0;
#if !TARGET_OS_EMBEDDED
		vproc_transaction_end(NULL, gController_vt);
		gController_vt = NULL;
#endif
	   SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: %s, done waiting for ServiceID.\n"),
			  __FUNCTION__);		
    }
	if (newPrimary)
		CFRelease(newPrimary);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int update_service(CFStringRef serviceID)
{
    CFDictionaryRef	service = NULL, interface;
    CFStringRef         subtype = NULL, iftype = NULL;
    struct service 		*serv;

    serv = findbyserviceID(serviceID);

    interface = copyEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serviceID, kSCEntNetInterface);
    if (interface)
        iftype = CFDictionaryGetValue(interface, kSCPropNetInterfaceType);

    if (!interface || (serv &&  !my_CFEqual(iftype, serv->typeRef))) {
        // check to see if service has disappear
        if (serv) {
            //SCDLog(LOG_INFO, CFSTR("Service has disappear : %@"), serviceID);
           dispose_service(serv);
        }
        goto done;
    }

    //SCDLog(LOG_INFO, CFSTR("change appears, subtype = %d, serviceID = %@\n"), subtype, serviceID);

    /* kSCPropNetServiceSubType contains the entity key Modem, PPPoE, or L2TP or VPN plugin id */
    subtype = CFDictionaryGetValue(interface, kSCPropNetInterfaceSubType);

	if (serv && !my_CFEqual(subtype, serv->subtypeRef)) {
        // subtype has changed
        dispose_service(serv);
        serv = 0;
    }


    // check to see if it is a new service
    if (!serv) {
       serv = new_service(serviceID, iftype, subtype);
        if (!serv)
            goto done;
    }

    // delay part or all the setup once all the notification for the service have been received
    serv->flags |= FLAG_SETUP;
       
done:            
    my_CFRelease(&interface);
    my_CFRelease(&service);
    return 0;
}

/* -----------------------------------------------------------------------------
an interface structure needs to be created
unit is the ppp managed unit
----------------------------------------------------------------------------- */
static
struct service * new_service(CFStringRef serviceID , CFStringRef typeRef, CFStringRef subtypeRef)
{
    struct service 		*serv = 0;
    u_short				len;
	int					err = 0;

    serv = malloc(sizeof(struct service));
    if (!serv)
        return 0;	// very bad...

    bzero(serv, sizeof(struct service));

    serv->serviceID = my_CFRetain(serviceID);
	serv->typeRef = my_CFRetain(typeRef);
    serv->subtypeRef = my_CFRetain(subtypeRef);
	
    // keep a C version of the service ID
    len = CFStringGetLength(serviceID) + 1;
    if (serv->sid = malloc(len)) {
        CFStringGetCString(serviceID, (char*)serv->sid, len, kCFStringEncodingUTF8);
    }

	if (my_CFEqual(typeRef, kSCValNetInterfaceTypePPP)) {
		
		serv->type = TYPE_PPP;
		serv->subtype = ppp_subtype(subtypeRef);		
	}
	else if (my_CFEqual(typeRef, kSCValNetInterfaceTypeIPSec)) {
		serv->type = TYPE_IPSEC;
		serv->subtype = ipsec_subtype(subtypeRef);
	}
	else 
		goto failed;

    serv->unit = findfreeunit(serv->type, serv->subtype);
    if (serv->unit == 0xFFFF)
        goto failed;	// no room left...
    
	switch (serv->type) {
		case TYPE_PPP:  err = ppp_new_service(serv); break;
		case TYPE_IPSEC:  err = ipsec_new_service(serv); break;
 	}
	if (err)
		goto failed;
	
    serv->uid = 0;
	serv->flags |= FLAG_FIRSTDIAL;

    TAILQ_INIT(&serv->client_head);

    TAILQ_INSERT_TAIL(&service_head, serv, next);

    client_notify(serv->serviceID, serv->sid, makeref(serv), 0, 0, CLIENT_FLAG_NOTIFY_STATUS, kSCNetworkConnectionDisconnected);
	
    return serv;

failed:
	if (serv) {
		my_CFRelease(&serv->serviceID);	
		my_CFRelease(&serv->typeRef);	
		my_CFRelease(&serv->subtypeRef);	
		if (serv->sid)
			free(serv->sid);
	
		free(serv);
	}
	
	return 0;
}

/* -----------------------------------------------------------------------------
an interface is come down, dispose the ppp structure
----------------------------------------------------------------------------- */
static 
int dispose_service(struct service *serv)
{
	int delay = 0; 
	
    // need to close the protocol first
    scnc_stop(serv, 0, SIGTERM);

	switch (serv->type) {
		case TYPE_PPP: delay = ppp_dispose_service(serv); break;
		case TYPE_IPSEC:  delay = ipsec_dispose_service(serv); break;
	}
    if (delay) {
		serv->flags |= FLAG_FREE;
		return 1;
    }else {
		serv->flags &= ~FLAG_FREE;
	}

    
#if TARGET_OS_EMBEDDED
	my_CFRelease(&serv->profileIdentifier);
#endif

	if (serv->flags & FLAG_SETUP_ONDEMAND) {
		serv->flags &= ~FLAG_SETUP_ONDEMAND;
		ondemand_remove_service(serv);
	}
	
    TAILQ_REMOVE(&service_head, serv, next);    

    client_notify(serv->serviceID, serv->sid, makeref(serv), 0, 0, CLIENT_FLAG_NOTIFY_STATUS, kSCNetworkConnectionInvalid);

    // then free the structure
    if (serv->sid)
        free(serv->sid);
	
	if (serv->userNotificationRef) {
		CFUserNotificationCancel(serv->userNotificationRef);
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->userNotificationRLS, kCFRunLoopDefaultMode);
		my_CFRelease(&serv->userNotificationRef);
		my_CFRelease(&serv->userNotificationRLS);			
	}
    my_CFRelease(&serv->serviceID);
    my_CFRelease(&serv->subtypeRef);
    my_CFRelease(&serv->typeRef);
    my_CFRelease(&serv->environmentVars);
    free(serv);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void reorder_services()
{
    CFDictionaryRef	ip_dict = NULL;
    CFStringRef		key, serviceID;
    CFArrayRef		serviceorder;
    int				i, nb;
    struct service		*serv;

    key = CREATEGLOBALSETUP(kSCEntNetIPv4);
    if (key) {
        ip_dict = (CFDictionaryRef)SCDynamicStoreCopyValue(gDynamicStore, key);
        if (ip_dict) {
            serviceorder = CFDictionaryGetValue(ip_dict, kSCPropNetServiceOrder);        
            if (serviceorder) {
  	        nb = CFArrayGetCount(serviceorder);
	        for (i = 0; i < nb; i++) {
                    serviceID = CFArrayGetValueAtIndex(serviceorder, i);                    
                    if (serv = findbyserviceID(serviceID)) {
						/* move it to the tail */
						TAILQ_REMOVE(&service_head, serv, next);
						TAILQ_INSERT_TAIL(&service_head, serv, next);
					}
                }
            }
            CFRelease(ip_dict);
        }
        CFRelease(key);
    }
}

/* -----------------------------------------------------------------------------
changed for this service occured in configd cache
----------------------------------------------------------------------------- */
static 
void finish_update_services()
{
    struct service 		*serv;
    
    TAILQ_FOREACH(serv, &service_head, next) {
        if (serv->flags & FLAG_SETUP) {
			
            serv->flags &= ~(FLAG_FREE + FLAG_SETUP);
			
			if (serv->flags & FLAG_SETUP_ONDEMAND) {
				serv->flags &= ~FLAG_SETUP_ONDEMAND;
				ondemand_remove_service(serv);
			}
			
			switch (serv->type) {
				case TYPE_PPP: ppp_setup_service(serv); break;
				case TYPE_IPSEC:  ipsec_setup_service(serv); break;
			}
			
#if TARGET_OS_EMBEDDED
			my_CFRelease(&serv->profileIdentifier);
			CFDictionaryRef payloadRoot = copyEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID, CFSTR("com.apple.payload/PayloadRoot"));
			if (payloadRoot) {
				serv->profileIdentifier = CFDictionaryGetValue(payloadRoot, CFSTR("PayloadIdentifier"));
				serv->profileIdentifier = isA_CFString(serv->profileIdentifier);
				if (serv->profileIdentifier)
					CFRetain(serv->profileIdentifier);
				CFRelease(payloadRoot);
			}
#endif
	
			if (serv->flags & FLAG_SETUP_ONDEMAND)
				ondemand_add_service(serv);
		}
    }
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct service *findbyserviceID(CFStringRef serviceID)
{
    struct service		*serv;

    TAILQ_FOREACH(serv, &service_head, next)
        if (CFStringCompare(serv->serviceID, serviceID, 0) == kCFCompareEqualTo) 
            return serv;
	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct service *findbypid(pid_t pid)
{
    struct service		*serv;

    TAILQ_FOREACH(serv, &service_head, next) {
		switch (serv->type) {
			case TYPE_PPP: 
				if (ppp_is_pid(serv, pid))
					return serv;
				break;
			case TYPE_IPSEC: 
				break;
		}
	}
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct service *findbysid(u_char *data, int len)
{
    struct service		*serv;

    TAILQ_FOREACH(serv, &service_head, next) 
		if (serv->sid && (strlen((char*)serv->sid) == len) && !strncmp((char*)serv->sid, (char*)data, len)) 
            return serv;
    return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int32_t makeref(struct service *serv)
{
    return (((u_int32_t)serv->subtype) << 16) + serv->unit;
}

/* -----------------------------------------------------------------------------
find the ppp structure corresponding to the reference, for a given type
if ref == -1, then return the default structure (first in the list)
----------------------------------------------------------------------------- */
struct service *findbyref(u_int16_t type, u_int32_t ref)
{
    u_short		subtype = ref >> 16;
    u_short		unit = ref & 0xFFFF;
    struct service		*serv;

    TAILQ_FOREACH(serv, &service_head, next) {
        if ((type == serv->type)
			&& (((serv->subtype == subtype) || (subtype == 0xFFFF))
				&&  ((serv->unit == unit) || (unit == 0xFFFF)))) {
            return serv;
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
get the first free ref number within a given type
----------------------------------------------------------------------------- */
static 
u_short findfreeunit(u_short type, u_short subtype)
{
    struct service		*serv = TAILQ_FIRST(&service_head);
    u_short		unit = 0;

    while (serv) {
    	if ((type == serv->type)
			&& (subtype == serv->subtype)
            && (serv->unit == unit)) {
            unit++;
            if (unit == 0xFFFF)
                return unit;
            serv = TAILQ_FIRST(&service_head); // restart
        }
        else 
            serv = TAILQ_NEXT(serv, next); // continue
    }

    return unit;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static
void print_services()
{
    struct service		*serv;

    SCLog(TRUE, LOG_INFO, CFSTR("SCNC Controller: Printing list of services : "));
    TAILQ_FOREACH(serv, &service_head, next) {
		switch (serv->type) {
			case TYPE_PPP:
				SCLog(TRUE, LOG_INFO, CFSTR("SCNC Controller: Service = %@, type = PPP, subtype = %@"), serv->serviceID, serv->subtypeRef);
				break;
			case TYPE_IPSEC:
				SCLog(TRUE, LOG_INFO, CFSTR("SCNC Controller: Service = %@, type = IPSec"), serv->serviceID);
				break;
			default:
				SCLog(TRUE, LOG_INFO, CFSTR("SCNC Controller: Service = %@, type = Unknown"), serv->serviceID);
				break;
		}
    }
}

/* -----------------------------------------------------------------------------
 phase change for this config occured
 ----------------------------------------------------------------------------- */
void phase_changed(struct service *serv, int phase)
{
	
	if (serv->flags & FLAG_SETUP_ONDEMAND)
		ondemand_add_service(serv);
	
    client_notify(serv->serviceID, serv->sid, makeref(serv), phase, 0, CLIENT_FLAG_NOTIFY_STATUS, scnc_getstatus(serv));
}

#if TARGET_OS_EMBEDDED
/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int start_profile_janitor(struct service *serv)
{
	CFStringRef 	resourceDir = NULL;
	CFURLRef 		resourceURL = NULL, absoluteURL = NULL;
	char			thepath[MAXPATHLEN], payloadIdentifierStr[256];
	int				ret = 0;
	char 			*cmdarg[4];

	if (serv->profileIdentifier == NULL)
		return ret;
	
	if (!CFStringGetCString(serv->profileIdentifier, payloadIdentifierStr, sizeof(payloadIdentifierStr), kCFStringEncodingUTF8))
		goto done;

	resourceURL = CFBundleCopyResourcesDirectoryURL(gBundleRef);
	if (resourceURL == NULL)
		goto done;
	
	absoluteURL = CFURLCopyAbsoluteURL(resourceURL);
	if (absoluteURL == NULL)
		goto done;
	
	resourceDir = CFURLCopyPath(absoluteURL);
	if (resourceDir == NULL)
		goto done;
				
	if (!CFStringGetCString(resourceDir, thepath, sizeof(thepath), kCFStringEncodingMacRoman))
		goto done;
	
	strlcat(thepath, "sbslauncher", sizeof(thepath));
	
	cmdarg[0] = "sbslauncher";
	cmdarg[1] = SBSLAUNCHER_TYPE_PROFILE_JANITOR;
	cmdarg[2] = payloadIdentifierStr;
	cmdarg[3] = NULL;

	if (_SCDPluginExecCommand(NULL, 0, 0 /*pid*/, 0/*gid*/, thepath, cmdarg) == 0)
		goto done;

	ret = 1;

done:

	if (ret)
		SCLog(TRUE, LOG_NOTICE, CFSTR("SCNC Controller: Started Profile Janitor for profile '%@'"), serv->profileIdentifier);
	else
		SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: Failed to start Profile Janitor for profile '%@'"), serv->profileIdentifier);

	if (resourceDir)
		CFRelease(resourceDir);
	if (absoluteURL)
		CFRelease(absoluteURL);
	if (resourceURL)
		CFRelease(resourceURL);

	return ret;
}
#endif


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void user_notification_callback(CFUserNotificationRef userNotification, CFOptionFlags responseFlags)
{
	
    struct service		*serv;

    TAILQ_FOREACH(serv, &service_head, next) {
        if (serv->userNotificationRef == userNotification) {
			
			// keep this one, and clear the one in the structure, to allow for reuse.
			CFRetain(userNotification);
			
			CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->userNotificationRLS, kCFRunLoopDefaultMode);
			my_CFRelease(&serv->userNotificationRef);
			my_CFRelease(&serv->userNotificationRLS);			

			switch (serv->type) {
				case TYPE_PPP:  ppp_user_notification_callback(serv, userNotification, responseFlags); break;
				case TYPE_IPSEC:  ipsec_user_notification_callback(serv, userNotification, responseFlags); break;
			}
			
			CFRelease(userNotification);
			break;
		}
	}
}


/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static 
void post_ondemand_token(uint64_t state64)
{
	uint32_t status;
	
	if (gNotifyOnDemandToken == -1) {
		status = notify_register_check(kSCNETWORKCONNECTION_ONDEMAND_NOTIFY_KEY, &gNotifyOnDemandToken);
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: notify_register_check failed, status = %d"), status);
			goto fail;
		}
	}
	
	status = notify_set_state(gNotifyOnDemandToken, state64);
	if (status != NOTIFY_STATUS_OK) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: notify_set_state failed, status = %d"), status);
		goto fail;
	}
	
	status = notify_post(kSCNETWORKCONNECTION_ONDEMAND_NOTIFY_KEY);
	if (status != NOTIFY_STATUS_OK) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: notify_post failed, status = %d"), status);
		goto fail;
	}

	return;

fail:
	if (gNotifyOnDemandToken != -1) {
		notify_cancel(gNotifyOnDemandToken);
		gNotifyOnDemandToken = -1;
	}
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static 
int ondemand_add_service(struct service *serv)
{
    CFMutableDictionaryRef	new_ondemand_dict = NULL, new_trigger_dict = NULL;
    CFStringRef			serviceid, ondemand_key = NULL;
    CFNumberRef			num;
	CFDictionaryRef		ondemand_dict = NULL, current_trigger_dict = NULL;
	CFMutableArrayRef	new_triggers_array = NULL;
	CFArrayRef			current_triggers_array = NULL;
	int					val = 0, ret = 0, count = 0, i, found = 0, found_index = 0;
	
	/* create the global plugin key */
	ondemand_key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetOnDemand);
	if (ondemand_key == NULL)
		goto fail;
	
	/* first remove existing trigger if present */ 
	ondemand_dict = SCDynamicStoreCopyValue(gDynamicStore, ondemand_key);
	if (ondemand_dict) {
		
		current_triggers_array = CFDictionaryGetValue(ondemand_dict, kSCNetworkConnectionOnDemandTriggers);
		if (current_triggers_array) {
			
			found = 0;
			count = CFArrayGetCount(current_triggers_array);
			for (i = 0; i < count; i++) {
				current_trigger_dict = CFArrayGetValueAtIndex(current_triggers_array, i);
				if (current_trigger_dict == NULL)
					// should not happen
					continue;
				serviceid = CFDictionaryGetValue(current_trigger_dict, kSCNetworkConnectionOnDemandServiceID);
				if (serviceid == NULL)
					continue;
				if (CFStringCompare(serviceid, serv->serviceID, 0) == kCFCompareEqualTo) {
					found = 1;
					found_index = i;
					break;
				}
			}
			
			new_triggers_array = CFArrayCreateMutableCopy(0, 0, current_triggers_array);
			if (new_triggers_array == NULL)
				goto fail;
			
			if (found) {
				CFArrayRemoveValueAtIndex(new_triggers_array, found_index);
			}
			
		}
		
		new_ondemand_dict = CFDictionaryCreateMutableCopy(0, 0, ondemand_dict);
		if (new_ondemand_dict == NULL)
			goto fail;
		
	}
	
	/* create the new ondemandtriggers_dict if necessary */
	if (new_ondemand_dict == NULL) {
		if ((new_ondemand_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == NULL)
			goto fail;
	}
	
	/* create the new ondemandtriggers_arayif necessary */
	if (new_triggers_array == NULL) {
		if ((new_triggers_array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks)) == NULL)
			goto fail;
	}
	
	/* build the dictionnary for this configuration */
    if ((new_trigger_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        goto fail;
	
	/* add type specicic keys */
	switch (serv->type) {
		case TYPE_PPP: 
			ppp_ondemand_add_service_data(serv, new_trigger_dict);
			break;
		case TYPE_IPSEC:
			ipsec_ondemand_add_service_data(serv, new_trigger_dict);
			break;
	}
	
	/* add generic keys */
	CFDictionarySetValue(new_trigger_dict, kSCNetworkConnectionOnDemandServiceID, serv->serviceID);
	
	val = scnc_getstatus(serv);
	num = CFNumberCreate(NULL, kCFNumberIntType, &val);
	if (num) {
		CFDictionarySetValue(new_trigger_dict, kSCNetworkConnectionOnDemandStatus, num);
		CFRelease(num);
	}
	
	/* Set it in the array */
	
	CFArrayAppendValue(new_triggers_array, new_trigger_dict);
	CFDictionarySetValue(new_ondemand_dict, kSCNetworkConnectionOnDemandTriggers, new_triggers_array);
	
    /* update the store now */
	if (SCDynamicStoreSetValue(gDynamicStore, ondemand_key, new_ondemand_dict) == 0) {
		;//warning("SCDynamicStoreSetValue IP %s failed: %s\n", ifname, SCErrorString(SCError()));
	}
    
	post_ondemand_token(CFArrayGetCount(new_triggers_array));

	ret = 1;
	goto done;
fail:
	ret = 0;	
	
done:
	my_CFRelease(&new_ondemand_dict);
	my_CFRelease(&new_trigger_dict);
	my_CFRelease(&ondemand_key);
	my_CFRelease(&new_triggers_array);
	my_CFRelease(&ondemand_dict);
	return ret;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static 
int ondemand_remove_service(struct service *serv)
{
    CFMutableDictionaryRef	new_ondemand_dict = NULL;
    CFStringRef			ondemand_key = NULL, serviceid;
	CFDictionaryRef		ondemand_dict, current_trigger_dict;
	CFMutableArrayRef	new_triggers_array = NULL;
	CFArrayRef			current_triggers_array = NULL;
	int					count = 0, i, ret = 0, found = 0, found_index = 0;
	
	/* create the global plugin key */
	ondemand_key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetOnDemand);
	if (ondemand_key == NULL)
		goto fail;
	
	ondemand_dict = SCDynamicStoreCopyValue(gDynamicStore, ondemand_key);
	if (ondemand_dict == NULL)
		goto fail;
	
	current_triggers_array = CFDictionaryGetValue(ondemand_dict, kSCNetworkConnectionOnDemandTriggers);
	if (current_triggers_array == NULL)
		goto fail;
	
	found = 0;
	count = CFArrayGetCount(current_triggers_array);
	for (i = 0; i < count; i++) {
		current_trigger_dict = CFArrayGetValueAtIndex(current_triggers_array, i);
		if (current_trigger_dict == NULL)
			// should not happen
			continue;
		serviceid = CFDictionaryGetValue(current_trigger_dict, kSCNetworkConnectionOnDemandServiceID);
		if (serviceid == NULL)
			continue;
		if (CFStringCompare(serviceid, serv->serviceID, 0) == kCFCompareEqualTo) {
			found = 1;
			found_index = i;
			break;
		}
	}
	
	if (!found)
		goto fail;
	
	/* if there was only one configuration, just remove the key */
	if (count == 1) {
		SCDynamicStoreRemoveValue(gDynamicStore, ondemand_key);
		post_ondemand_token(0);
		ret = 1;
		goto done;
	}

	/* otherwise create a new array, and update the key */
	new_triggers_array = CFArrayCreateMutableCopy(0, 0, current_triggers_array);
	if (new_triggers_array == NULL)
		goto fail;
		
	CFArrayRemoveValueAtIndex(new_triggers_array, found_index);
	
	new_ondemand_dict = CFDictionaryCreateMutableCopy(0, 0, ondemand_dict);
	if (new_ondemand_dict == NULL)
		goto fail;
	
	CFDictionarySetValue(new_ondemand_dict, kSCNetworkConnectionOnDemandTriggers, new_triggers_array);
	
	/* update the store now */
	if (SCDynamicStoreSetValue(gDynamicStore, ondemand_key, new_ondemand_dict) == 0) {
		;//warning("SCDynamicStoreSetValue IP %s failed: %s\n", ifname, SCErrorString(SCError()));
	}
    	
	post_ondemand_token(CFArrayGetCount(new_triggers_array));

	ret = 1;
	goto done;
fail:
	ret = 0;	
	
done:
	my_CFRelease(&ondemand_dict);
	my_CFRelease(&ondemand_key);
	my_CFRelease(&new_ondemand_dict);
	my_CFRelease(&new_triggers_array);
	return ret;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */ 
void disable_ondemand(struct service *serv)
{
	if (serv->flags & FLAG_SETUP_ONDEMAND) {
		serv->flags &= ~FLAG_SETUP_ONDEMAND;
		ondemand_remove_service(serv);
	}
}

#if TARGET_OS_EMBEDDED
/* -----------------------------------------------------------------------------
 Bring up EDGE
 ----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static 
void cellular_event(struct service	*serv, int event)
{
	switch (serv->type) {
		case TYPE_IPSEC:  ipsec_cellular_event(serv, event); break;
	}
}

static 
void cellular_timer(CFRunLoopTimerRef timer, void *info)
{
	struct service *serv = info;
	
	my_CFRelease(&serv->cellular_timerref);
	cellular_event(serv, CELLULAR_BRINGUP_SUCCESS_EVENT);
}

static 
void cellular_callback(CTServerConnectionRef connection, CFStringRef notification, CFDictionaryRef notificationInfo, void* info) {
	
	struct service	*serv = (struct service *)info;
	SInt32 which;
    CFRunLoopTimerContext	timer_ctxt = { 0, serv, NULL, NULL, NULL };
	
	// You only want to act on notifications for the main PDP context.
	if (CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(notificationInfo, kCTRegistrationDataContextID), kCFNumberSInt32Type, &which) && (which == 0)) {		
		
		if (CFEqual(notification, kCTRegistrationDataStatusChangedNotification)) {
			if (CFEqual(CFDictionaryGetValue(notificationInfo, kCTRegistrationDataActive), kCFBooleanTrue)) {
				// It's good to go.  Tear down everything and run.
				SCLog(gSCNCVerbose, LOG_INFO, CFSTR("SCNC Controller: cellular_callback activation succeeded"));
				CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->cellularRLS, kCFRunLoopCommonModes);
				my_CFRelease(&serv->cellularRLS);			
				CFMachPortInvalidate(serv->cellularPort);
				my_CFRelease(&serv->cellularPort);			
				my_CFRelease(&serv->cellularConnection);

				serv->cellular_timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + TIMEOUT_EDGE, FAR_FUTURE, 0, 0, cellular_timer, &timer_ctxt);
				if (!serv->cellular_timerref) {
					SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: cellular_callback, cannot create RunLoop timer"));
					cellular_event(serv, CELLULAR_BRINGUP_FATAL_FAILURE_EVENT);
					return;
				}
				CFRunLoopAddTimer(CFRunLoopGetCurrent(), serv->cellular_timerref, kCFRunLoopCommonModes);
			}
		}
		else {
			// Failed.  Tear down everything.
			SCLog(gSCNCVerbose, LOG_INFO, CFSTR("SCNC Controller: callbackEDGE activation failed"));
			cellular_event(serv, CELLULAR_BRINGUP_NETWORK_FAILURE_EVENT);
		}
	}
}

static 
void _ServerConnectionHandleReply(CFMachPortRef port, void *msg, CFIndex size, void *info) {
    
    (void)port;	    /* Unused */
    (void)size;	    /* Unused */
    (void)info;	    /* Unused */
    
    _CTServerConnectionHandleReply(msg);
}
/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */ 
int bringup_cellular(struct service *serv)
{
	_CTServerConnectionContext ctxt = { 0, serv, NULL, NULL, NULL };
	CFMachPortContext mach_ctxt = { 0, serv, NULL, NULL, NULL };
    CFRunLoopTimerContext	timer_ctxt = { 0, serv, NULL, NULL, NULL };
	CTError error = { kCTErrorDomainNoError, 0 };
	Boolean active = FALSE;
	
	serv->cellularConnection = _CTServerConnectionCreate(kCFAllocatorDefault, cellular_callback, &ctxt);
	if (!serv->cellularConnection)
		goto fail;
	
	error = _CTServerConnectionGetPacketContextActive(serv->cellularConnection, 0, &active);
	
	if (error.error)
		goto fail;
	
	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("SCNC Controller: BringUpEDGE active = %d"), active);

	if (active) {
		my_CFRelease(&serv->cellularConnection);
		serv->cellular_timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + TIMEOUT_EDGE, FAR_FUTURE, 0, 0, cellular_timer, &timer_ctxt);
		if (!serv->cellular_timerref) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: BringUpCellular, cannot create RunLoop timer"));
			goto fail;
		}
		CFRunLoopAddTimer(CFRunLoopGetCurrent(), serv->cellular_timerref, kCFRunLoopCommonModes);
		return 1; // timer active, pretend connection in progress
	}
	
	error = _CTServerConnectionRegisterForNotification(serv->cellularConnection, kCTRegistrationDataStatusChangedNotification);
	if (error.error == 0)
		error = _CTServerConnectionRegisterForNotification(serv->cellularConnection, kCTRegistrationDataActivateFailedNotification);
	if (error.error == 0)
		error = _CTServerConnectionSetPacketContextActive(serv->cellularConnection, 0, TRUE);		// Zero is the main PDP context.
	
	if (error.error)
		goto fail;
	
	serv->cellularPort = _SC_CFMachPortCreateWithPort("PPPController/CT", _CTServerConnectionGetPort(serv->cellularConnection), (CFMachPortCallBack)_ServerConnectionHandleReply, &mach_ctxt);
	serv->cellularRLS = CFMachPortCreateRunLoopSource(NULL, serv->cellularPort, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), serv->cellularRLS, kCFRunLoopCommonModes);
	
	return 1; // connection in progress
	
fail:
	SCLog(TRUE, LOG_ERR, CFSTR("SCNC Controller: BringUpEDGE cannot bring up cellular, error = %d"), error.error);
	my_CFRelease(&serv->cellularConnection);
	return 0; // connection failed
}

#endif

// check to see if interface is captive and if it is not ready.
int
check_interface_captive_and_not_ready (SCDynamicStoreRef  dynamicStoreRef,
									   char              *interface_buf)
{
	int rc = 0;

	if (dynamicStoreRef) {
		CFStringRef     captiveState = CFStringCreateWithFormat(NULL, NULL,
																CFSTR("State:/Network/Interface/%s/CaptiveNetwork"),
																interface_buf);
		if (captiveState) {
			CFStringRef key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, captiveState);
			if (key) {
				CFDictionaryRef dict = SCDynamicStoreCopyValue(dynamicStoreRef, key);
				CFRelease(key);
				if (dict) {
					CFStringRef string = CFDictionaryGetValue(dict, CFSTR("Stage"));
					if (string) {
						// if string != Unknown && string != Online
						if (CFStringCompare(string, CFSTR("Uknown"), 0) != kCFCompareEqualTo &&
							CFStringCompare(string, CFSTR("Online"), 0) != kCFCompareEqualTo) {
							SCLog(TRUE, LOG_NOTICE, CFSTR("underlying interface %s is captive and not yet ready."), interface_buf);
							rc = 1;
						} else {
							SCLog(TRUE, LOG_NOTICE, CFSTR("underlying interface %s is either unknown or captive and ready."), interface_buf);
						}
					}
					CFRelease(dict);
				}
			}
			CFRelease(captiveState);
		}
	}
	return rc;
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
static 
int add_client(struct service *serv, void *client, int autoclose)
{
    struct service_client *servclient;
    
    servclient = malloc(sizeof(struct service_client));
    if (servclient == 0)
        return -1; // very bad...

    servclient->client = client;
    servclient->autoclose = autoclose;
    TAILQ_INSERT_TAIL(&serv->client_head, servclient, next);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int remove_client(struct service *serv, void *client)
{
    struct service_client *servclient;

    TAILQ_FOREACH(servclient, &serv->client_head, next) {
        if (servclient->client == client) {
            TAILQ_REMOVE(&serv->client_head, servclient, next);
            free(servclient);
            return 0;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
struct service_client *get_client(struct service *serv, void *client)
{
    struct service_client *servclient;

    TAILQ_FOREACH(servclient, &serv->client_head, next)
        if (servclient->client == client)
            return servclient;

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int remove_all_clients(struct service *serv)
{
    struct service_client *servclient;

    while (servclient = TAILQ_FIRST(&serv->client_head)) {
        TAILQ_REMOVE(&serv->client_head, servclient, next);
        free(servclient);
    }
    return 0;
}

static char *
get_plugin_pid_str (struct service *serv, int pid, char *pid_buf, int pid_buf_len)
{
	char *p;
	int   numchar;
	int   spaceleft;

	numchar = snprintf(pid_buf, pid_buf_len, ", triggered by ");
	if (numchar > 0) {
		p = pid_buf + numchar;
		spaceleft = pid_buf_len - numchar;
		proc_name(pid, p, spaceleft);
	} else {
		pid_buf[0] = 0;
	}
	return pid_buf;
}

static void
log_scnc_stop (struct service *serv)
{
	if (serv->type == TYPE_IPSEC) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("SCNC: stop, type %@"), CFSTR("IPSec"));	
	} else {
		SCLog(TRUE, LOG_NOTICE, CFSTR("SCNC: stop, type %@"), serv->subtypeRef);	
	}
}

static void
log_scnc_start (struct service *serv, int onDemand, CFStringRef onDemandHostName, int pid, int status)
{
	char pid_buf[128];
	char *p = get_plugin_pid_str(serv, pid, pid_buf, sizeof(pid_buf));
	
	if (onDemand) {
		if (serv->type == TYPE_IPSEC) {
			SCLog(TRUE, LOG_NOTICE, CFSTR("SCNC: start%s, type %@, onDemandHostName %@, status %d"),
				  p,
				  CFSTR("IPSec"),
				  onDemandHostName,
				  status);
		} else {
			SCLog(TRUE, LOG_NOTICE, CFSTR("SCNC: start%s, type %@, onDemandHostName %@, status %d"),
				  p,
				  serv->subtypeRef,
				  onDemandHostName,
				  status);
		}
	} else {
		if (serv->type == TYPE_IPSEC) {
			SCLog(TRUE, LOG_NOTICE, CFSTR("SCNC: start%s, type %@, status %d"),
				  p,
				  CFSTR("IPSec"),
				  status);
		} else {
			SCLog(TRUE, LOG_NOTICE, CFSTR("SCNC: start%s, type %@, status %d"),
				  p,
				  serv->subtypeRef,
				  status);
		}
	}
}

/* --------------------------------------------------------------------------
-----------------------------------------------------------------------------
-----------------------------------------------------------------------------
  SCNC API
-----------------------------------------------------------------------------
-----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int scnc_stop(struct service *serv, void *client, int signal)
{
	int ret = -1;
	
    /* arbitration mechanism : disconnects only when no client is using it */
    if (client) {
        if (get_client(serv, client))
            remove_client(serv, client);

        /* check if we have at least one client */
        if (TAILQ_FIRST(&serv->client_head))
            return 0;
    }
    else {
        remove_all_clients(serv);
    }

	log_scnc_stop(serv);

	switch (serv->type) {
		case TYPE_PPP: ret = ppp_stop(serv, signal); break;
		case TYPE_IPSEC:  ret = ipsec_stop(serv, signal); break;
	}

    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int scnc_start(struct service *serv, CFDictionaryRef options, void *client, int autoclose, uid_t uid, gid_t gid, int pid, mach_port_t bootstrap)
{
	int ret = EIO, onDemand = 0;
	CFStringRef    onDemandHostName = NULL;
	
	/* first determine autodial opportunity */
	if (options) {
		CFStringRef		priority;
		int				dialMode = 1; // default is aggressive
		
		//CFShow(options);
		if (!CFDictionaryContainsKey(options, kSCNetworkConnectionSelectionOptionOnDemandHostName)) {
			// option not set, regular dial request
			goto dial;
		} else {
			// initialize ondemand hostname
			onDemandHostName = isA_CFString(CFDictionaryGetValue(options, kSCNetworkConnectionSelectionOptionOnDemandHostName));
		}

#if	!TARGET_OS_EMBEDDED
		/* first, check if the user is he current console user */
		if (!gLoggedInUser 
			|| uid != gLoggedInUserUID) 
			return EIO; // SCNetworkConnection API needs to implement specific status (kSCStatusConnectionOnDemandRetryLater) */
#endif			
			
		priority = CFDictionaryGetValue(options, kSCPropNetPPPOnDemandPriority);
		if (isString(priority)) {
			
			if (CFStringCompare(priority, kSCValNetPPPOnDemandPriorityHigh, 0) == kCFCompareEqualTo)
				dialMode = 1;
			else if (CFStringCompare(priority, kSCValNetPPPOnDemandPriorityLow, 0) == kCFCompareEqualTo) 
				dialMode = 2;
			else if (CFStringCompare(priority, kSCValNetPPPOnDemandPriorityDefault, 0) == kCFCompareEqualTo) {
			
				// Autodial is required in default mode.
				// Use the aggressivity level from the options
				CFDictionaryRef dict;
				if (dict = CFDictionaryGetValue(options, kSCEntNetPPP)) {
					CFStringRef str;
					str = CFDictionaryGetValue(dict, kSCPropNetPPPOnDemandMode);
					if (isString(str)) {
						if (CFStringCompare(str, kSCValNetPPPOnDemandModeAggressive, 0) == kCFCompareEqualTo)
							dialMode = 1;
						else if (CFStringCompare(str, kSCValNetPPPOnDemandModeConservative, 0) == kCFCompareEqualTo)
							dialMode = 2;
						else if (CFStringCompare(str, kSCValNetPPPOnDemandModeCompatible, 0) == kCFCompareEqualTo)
							dialMode = 0;
					}
				}
			}
		}
		
		//printf("dialMode = %d\n", dialMode);
		switch (dialMode) {
			case 1: // high
				// OK, let's dial
				onDemand = 1;
				break;
			case 2: // low
				// Need to implement smart behavior
				if (serv->flags & FLAG_FIRSTDIAL) {
					// First time after major event, let's dial
					onDemand = 1;
				}
				break;
			default:
				break;
		}
		
		if (!onDemand)
			return EIO; // SCNetworkConnection API needs to implement specific status (kSCStatusConnectionOnDemandRetryLater) */
	}

dial:
	
    if (gStopRls ||
        (gSleeping && (serv->flags & FLAG_SETUP_DISCONNECTONSLEEP)))
        return EIO;	// not the right time to dial

	serv->persist_connect = 0;
	serv->persist_connect_status = 0;
	serv->persist_connect_devstatus = 0;

	switch (serv->type) {
		case TYPE_PPP: ret = ppp_start(serv, options, uid, gid, bootstrap, 0, onDemand); break;
		case TYPE_IPSEC:  ret = ipsec_start(serv, options, uid, gid, bootstrap, 0, onDemand); break;
	}
	
	log_scnc_start(serv, onDemand, onDemandHostName, pid, ret);

	if (ret == 0) {
		// reset autodial flag;
		serv->flags &= ~FLAG_FIRSTDIAL;

		if (client)
			add_client(serv, client, autoclose);
	}

    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int scnc_suspend(struct service *serv)
{
	int ret = -1;

	switch (serv->type) {
		case TYPE_PPP: ret = ppp_suspend(serv); break;
		case TYPE_IPSEC:  /* N/A */ break;
	}
	
	return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int scnc_resume(struct service *serv)
{
	int ret = -1;
	
	switch (serv->type) {
		case TYPE_PPP: ret = ppp_resume(serv); break;
		case TYPE_IPSEC:  /* N/A */ break;
	}
	
	return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int scnc_getstatus(struct service *serv)
{
	SCNetworkConnectionStatus	status = kSCNetworkConnectionDisconnected;

	switch (serv->type) {
		case TYPE_PPP: status = ppp_getstatus(serv); break;
		case TYPE_IPSEC:  status = ipsec_getstatus(serv); break;
	}

	return status;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int scnc_copyextendedstatus(struct service *serv, void **reply, u_int16_t *replylen)
{
	int ret = -1;
	
	switch (serv->type) {
		case TYPE_PPP: ret = ppp_copyextendedstatus(serv, reply, replylen); break;
		case TYPE_IPSEC:  ret = ipsec_copyextendedstatus(serv, reply, replylen); break;
	}
	
	return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int scnc_copystatistics(struct service *serv, void **reply, u_int16_t *replylen)
{
	int ret = -1;
	
	switch (serv->type) {
		case TYPE_PPP: ret = ppp_copystatistics(serv, reply, replylen); break;
		case TYPE_IPSEC:  ret = ipsec_copystatistics(serv, reply, replylen); break;
	}
	
	return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int scnc_getconnectdata(struct service *serv, void **reply, u_int16_t *replylen, int all)
{
	int ret = -1;
	
	switch (serv->type) {
		case TYPE_PPP: ret = ppp_getconnectdata(serv, reply, replylen, all); break;
		case TYPE_IPSEC:  ret = ipsec_getconnectdata(serv, reply, replylen, all); break;
	}
	
	return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int scnc_getconnectsystemdata(struct service *serv, void **reply, u_int16_t *replylen)
{
	int ret = -1;
	
	switch (serv->type) {
		case TYPE_PPP: ret = ppp_getconnectsystemdata(serv, reply, replylen); break;
		case TYPE_IPSEC:  /* ret = ipsec_getconnectsystemdata(serv, reply, replylen) */; break;
	}
	
	return ret;
}

#if !TARGET_OS_EMBEDDED
static double scnc_getsleepwaketimeout (struct service *serv)
{
	if (serv->sleepwaketimeout == 0)
		return (double)gSleepWakeTimeout;
	return (double)serv->sleepwaketimeout;
}
#endif

static void scnc_idle_disconnect (struct service *serv)
{
	switch (serv->type) {
	case TYPE_PPP:
		serv->u.ppp.laststatus = EXIT_IDLE_TIMEOUT;
		ppp_stop(serv, SIGTERM);
		break;
	case TYPE_IPSEC:
		serv->u.ipsec.laststatus = IPSEC_IDLETIMEOUT_ERROR;
		ipsec_stop(serv, SIGTERM);
		break;
	}
}

int scnc_disconnectifoverslept (const char *function, struct service *serv, char *if_name)
{
#if TARGET_OS_EMBEDDED
	if (gWakeUpTime != -1 && gSleptAt != -1) {
		double sleptFor = difftime(gWokeAt, gSleptAt);
		SCLog(TRUE, LOG_ERR, CFSTR("%s: System slept for %f secs, interface %s will disconnect\n"),
		      function,
		      sleptFor,
		      if_name);
		scnc_idle_disconnect(serv);
	}
	return 1;
#else
	if (gWakeUpTime != -1 && gSleptAt != -1) {
		double sleptFor = difftime(gWokeAt, gSleptAt);
		double timeout = scnc_getsleepwaketimeout(serv);
		SCLog(gSCNCVerbose, LOG_INFO, CFSTR("%s: System slept for %f secs (interface %s's limit is %f secs)\n"),
		      function,
		      sleptFor,
		      if_name,
		      timeout);
		serv->connectionslepttime += (u_int32_t)sleptFor;
		if (sleptFor > timeout) {
			SCLog(TRUE, LOG_ERR, CFSTR("%s: System slept for %f secs (more than %f secs), interface %s will disconnect\n"),
				  function,
				  sleptFor,
				  timeout,
				  if_name);
			scnc_idle_disconnect(serv);
			return 1;
		}
	}
	return 0;
#endif /* TARGET_OS_EMBEDDED */
}

