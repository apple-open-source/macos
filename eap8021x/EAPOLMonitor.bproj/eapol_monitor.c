/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

/*
 *  eapol_monitor.c
 *  - EAPOLMonitor user event agent that initiates 802.1X authentication
 *    sessions
 */

/* 
 * Modification History
 *
 * September 15, 2010	Dieter Siegmund
 * - created
 */
 
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/param.h>
#include <paths.h>
#include <pthread.h>
#include <mach/mach.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_media.h>
#include <sys/sockio.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <CoreFoundation/CFUserNotification.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFRunLoop.h>
#include <notify.h>
#include "myCFUtil.h"
#include "EAPOLClientConfiguration.h"
#include "EAPOLClientConfigurationPrivate.h"
#include "EAPOLControlTypes.h"
#include "EAPOLControl.h"
#include "EAPOLControlPrivate.h"
#include "EAPOLControlTypesPrivate.h"
#include "SupplicantTypes.h"
#include "EAPClientTypes.h"

#include "UserEventAgentInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#define MY_BUNDLE_ID	CFSTR("com.apple.UserEventAgent.EAPOLMonitor")

typedef enum {
    kMonitorStateIdle,
    kMonitorStatePrompting,
    kMonitorStateStarted,
    kMonitorStateIgnorePermanently
} MonitorState;

struct MonitoredInterface_s;
typedef struct MonitoredInterface_s * MonitoredInterfaceRef;

#define LIST_HEAD_MonitoredInterfaceHead LIST_HEAD(MonitoredInterfaceHead, MonitoredInterface_s)
static LIST_HEAD_MonitoredInterfaceHead S_MonitoredInterfaceHead;
static struct MonitoredInterfaceHead * 	S_MonitoredInterfaceHead_p = &S_MonitoredInterfaceHead;

#define LIST_ENTRY_MonitoredInterface	LIST_ENTRY(MonitoredInterface_s)

struct MonitoredInterface_s {
    LIST_ENTRY_MonitoredInterface	entries;

    char				if_name[IFNAMSIZ];
    CFStringRef				if_name_cf;
    MonitorState			state;
    CFAbsoluteTime			start_time;
    bool				ignore_until_link_status_changes;
    bool				link_active_when_started;
    bool				require_fresh_detection;
    CFUserNotificationRef 		cfun;
    CFRunLoopSourceRef			rls;

    /* EAPOLClientConfigurationRef	cfg; */ /* XXX might need in future */
    CFArrayRef				profiles;
};

typedef struct {
    UserEventAgentInterfaceStruct *	_UserEventAgentInterface;
    CFUUIDRef				_factoryID;
    UInt32 				_refCount;
    SCDynamicStoreRef			store;
    CFRunLoopSourceRef			rls;

    /* notifications for user settings changing */
    struct {
	CFMachPortRef			mp;
	int				token;
    } settings_change;
} MyType;

static bool				S_verbose;
static bool				S_auto_connect;

/**
 ** Utility routines
 **/

static bool
S_on_console(SCDynamicStoreRef store)
{
    bool			on_console;
    uid_t			uid;
    CFStringRef 		user;

    user = SCDynamicStoreCopyConsoleUser(store, &uid, NULL);
    if (user == NULL || uid != getuid()) {
	on_console = FALSE;
    }
    else {
	on_console = TRUE;
    }
    my_CFRelease(&user);
    return (on_console);
}

static CFBundleRef
S_get_bundle()
{
    CFBundleRef		bundle;

    bundle = CFBundleGetBundleWithIdentifier(MY_BUNDLE_ID);
    if (bundle == NULL) {
	bundle = CFBundleGetMainBundle();
    }
    return (bundle);
}

/* 
 * Function: my_CFStringCopyComponent
 * Purpose:
 *    Separates the given string using the given separator, and returns
 *    the component at the specified index.
 * Returns:
 *    NULL if no such component exists, non-NULL component otherwise
 */
static CFStringRef
my_CFStringCopyComponent(CFStringRef path, CFStringRef separator, 
			 CFIndex component_index)
{
    CFArrayRef		arr;
    CFStringRef		component = NULL;

    arr = CFStringCreateArrayBySeparatingStrings(NULL, path, separator);
    if (arr == NULL) {
	goto done;
    }
    if (CFArrayGetCount(arr) <= component_index) {
	goto done;
    }
    component = CFRetain(CFArrayGetValueAtIndex(arr, component_index));

 done:
    my_CFRelease(&arr);
    return (component);

}

static void
my_log(int priority, const char *message, ...)
{
    va_list 		ap;

    if (priority == LOG_DEBUG) {
	if (S_verbose == FALSE)
	    return;
	priority = LOG_NOTICE;
    }

    va_start(ap, message);
    vsyslog(priority, message, ap);
    va_end(ap);

    return;
}

static CFStringRef
copy_localized_if_name(CFStringRef if_name_cf)
{
    int			count;
    int			i;
    CFArrayRef		if_list;
    CFStringRef		loc_name = NULL;

    if_list = SCNetworkInterfaceCopyAll();
    if (if_list == NULL) {
	goto done;
    }
    count = CFArrayGetCount(if_list);
    for (i = 0; i < count; i++) {
	CFStringRef		this_name;
	SCNetworkInterfaceRef	sc_if;

	sc_if = (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(if_list, i);
	this_name = SCNetworkInterfaceGetBSDName(sc_if);
	if (this_name != NULL
	    && CFEqual(this_name, if_name_cf)) {
	    loc_name = SCNetworkInterfaceGetLocalizedDisplayName(sc_if);
	    if (loc_name != NULL) {
		CFRetain(loc_name);
	    }
	    break;
	}
    }
    
 done:
    if (if_list != NULL) {
	CFRelease(if_list);
    }
    return (loc_name);
}

static bool
is_link_active(const char * name)
{
    bool		active = FALSE;
    struct ifmediareq	ifm;
    int			s;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	perror("socket");
	goto done;
    }
    bzero(&ifm, sizeof(ifm));
    strlcpy(ifm.ifm_name, name, sizeof(ifm.ifm_name));
    if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifm) < 0) {
	goto done;
    }
    if ((ifm.ifm_status & IFM_AVALID) == 0
	|| (ifm.ifm_status & IFM_ACTIVE) != 0) {
	active = TRUE;
    }

 done:
    if (s >= 0) {
	close(s);
    }
    return (active);
}

static int
get_ifm_type(const char * name)
{
    struct ifmediareq	ifm;
    int			s;
    int			ifm_type = 0;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	perror("socket");
	goto done;
    }
    bzero(&ifm, sizeof(ifm));
    strlcpy(ifm.ifm_name, name, sizeof(ifm.ifm_name));
    if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifm) < 0) {
	goto done;
    }
    ifm_type = IFM_TYPE(ifm.ifm_current);
 done:
    if (s >= 0) {
	close(s);
    }
    return (ifm_type);
}

static uint32_t
S_get_plist_uint32(CFDictionaryRef plist, CFStringRef key, uint32_t def)
{
    CFNumberRef 	n;
    uint32_t		ret = def;

    n = isA_CFNumber(CFDictionaryGetValue(plist, key));
    if (n) {
	if (CFNumberGetValue(n, kCFNumberSInt32Type, &ret) == FALSE) {
	    ret = def;
	}
    }
    return (ret);
}

static __inline__ CFOptionFlags
S_CFUserNotificationResponse(CFOptionFlags flags)
{
    return (flags & 0x3);
}

#define kNetworkPrefPath "/System/Library/PreferencePanes/Network.prefPane"

static CFURLRef
copy_icon_url(CFStringRef icon)
{
    CFBundleRef		np_bundle;
    CFURLRef		np_url;
    CFURLRef		url = NULL;

    np_url = CFURLCreateWithFileSystemPath(NULL,
					   CFSTR(kNetworkPrefPath),
					   kCFURLPOSIXPathStyle, FALSE);
    if (np_url != NULL) {
	np_bundle = CFBundleCreate(NULL, np_url);
	if (np_bundle != NULL) {
	    url = CFBundleCopyResourceURL(np_bundle, icon, 
					  CFSTR("icns"), NULL);
	    if (url == NULL) {
		url = CFBundleCopyResourceURL(np_bundle, icon, 
					      CFSTR("tiff"), NULL);
	    }
	    CFRelease(np_bundle);
	}
	CFRelease(np_url);
    }
    return (url);
}

/**
 ** PromptStartContext
 **/
typedef struct {
    EAPOLClientConfigurationRef		cfg;
    CFArrayRef				profiles;
    CFArrayRef				profile_names;
} PromptStartContext, * PromptStartContextRef;

static void
PromptStartContextRelease(PromptStartContextRef context)
{
    my_CFRelease(&context->cfg);
    my_CFRelease(&context->profiles);
    my_CFRelease(&context->profile_names);
    return;
}

static void
PromptStartContextInit(PromptStartContextRef context)
{
    context->cfg = EAPOLClientConfigurationCreate(NULL);
    context->profiles = EAPOLClientConfigurationCopyProfiles(context->cfg);
    context->profile_names = NULL;
    if (context->profiles != NULL) {
	CFBundleRef		bundle;
	int			count;
	int			i;
	CFRange			r;
	CFStringRef		use_defaults;
	CFMutableArrayRef	profile_names;
	
	count = CFArrayGetCount(context->profiles);
	profile_names = CFArrayCreateMutable(NULL, count + 1,
					     &kCFTypeArrayCallBacks);
	bundle = S_get_bundle();
	use_defaults 
	    = CFBundleCopyLocalizedString(bundle, 
					  CFSTR("DEFAULT_CONFIGURATION"),
					  NULL, NULL);
	CFArrayAppendValue(profile_names, use_defaults);
	CFRelease(use_defaults);
	r.location = 0;
	r.length = 1;
	for (i = 0; i < count; i++) {
	    int				instance;
	    CFStringRef			name;
	    CFStringRef			new_name;
	    EAPOLClientProfileRef	profile;
	    
	    profile = (EAPOLClientProfileRef)
		CFArrayGetValueAtIndex(context->profiles, i);
	    name = EAPOLClientProfileGetUserDefinedName(profile);
	    if (name == NULL) {
		name = EAPOLClientProfileGetID(profile);
	    }
	    for (instance = 2, new_name = CFRetain(name); 
		 CFArrayContainsValue(profile_names, r, new_name); instance++) {
		CFRelease(new_name);
		new_name
		    = CFStringCreateWithFormat(NULL, NULL,
					       CFSTR("%@ (%d)"), name,
					       instance);
		
	    }
	    CFArrayAppendValue(profile_names, new_name);
	    CFRelease(new_name);
	    r.length++;
	}
	context->profile_names = profile_names;
    }
    return;

}

/**
 ** MonitoredInterfaceRef
 **/

static MonitoredInterfaceRef
MonitoredInterfaceCreate(CFStringRef ifname)
{
    MonitoredInterfaceRef	mon;

    mon = (MonitoredInterfaceRef)malloc(sizeof(*mon));
    bzero(mon, sizeof(*mon));
    mon->if_name_cf = CFRetain(ifname);
    CFStringGetCString(ifname, mon->if_name, sizeof(mon->if_name),
		       kCFStringEncodingASCII);
    LIST_INSERT_HEAD(S_MonitoredInterfaceHead_p, mon, entries);
    return (mon);
}

static void
MonitoredInterfaceReleasePrompt(MonitoredInterfaceRef mon)
{
    if (mon->cfun != NULL) {
	(void)CFUserNotificationCancel(mon->cfun);
	my_CFRelease(&mon->cfun);
    }
    if (mon->rls != NULL) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), mon->rls, 
			      kCFRunLoopDefaultMode);
	my_CFRelease(&mon->rls);
    }
    my_CFRelease(&mon->profiles);
    return;
}

static void
MonitoredInterfaceRelease(MonitoredInterfaceRef * mon_p)
{
    MonitoredInterfaceRef 	mon;

    mon = *mon_p;
    if (mon == NULL) {
	return;
    }
    *mon_p = NULL;
    LIST_REMOVE(mon, entries);
    my_CFRelease(&mon->if_name_cf);
    MonitoredInterfaceReleasePrompt(mon);
    free(mon);
    return;
}

static MonitoredInterfaceRef
MonitoredInterfaceLookupByName(CFStringRef if_name_cf)
{
    MonitoredInterfaceRef	scan;

    LIST_FOREACH(scan, S_MonitoredInterfaceHead_p, entries) {
	if (CFEqual(scan->if_name_cf, if_name_cf)) {
	    return (scan);
	}
    }
    return (NULL);
}

static MonitoredInterfaceRef
MonitoredInterfaceLookupByCFUN(CFUserNotificationRef cfun)
{
    MonitoredInterfaceRef	scan;

    LIST_FOREACH(scan, S_MonitoredInterfaceHead_p, entries) {
	if (scan->cfun == cfun) {
	    return (scan);
	}
    }
    return (NULL);
}

static const CFStringRef	S_manager_name = CFSTR("com.apple.UserEventAgent.EAPOLMonitor");

static void
MonitoredInterfaceStartAuthentication(MonitoredInterfaceRef mon,
				      EAPOLClientItemIDRef itemID)
{
    CFStringRef			key;
    CFDictionaryRef		options;
    EAPOLClientProfileRef	profile = EAPOLClientItemIDGetProfile(itemID);
    int				status;

    key = kEAPOLControlStartOptionManagerName;
    options = CFDictionaryCreate(NULL,
				 (const void * *)&key,
				 (const void * *)&S_manager_name,
				 1, 
				 &kCFTypeDictionaryKeyCallBacks,
				 &kCFTypeDictionaryValueCallBacks);
    my_log(LOG_NOTICE,
	   "EAPOLMonitor: %s starting %s",
	   mon->if_name,
	   (profile == NULL) ? "(Default Settings)" : "(Profile)");
    status = EAPOLControlStartWithOptions(mon->if_name, itemID, options);
    CFRelease(options);
    if (status == 0) {
	mon->state = kMonitorStateStarted;
	mon->start_time = CFAbsoluteTimeGetCurrent();
	mon->link_active_when_started = is_link_active(mon->if_name);
    }
    else {
	/* XXX how to handle this ? */
	my_log(LOG_NOTICE,
	       "EAPOLMonitor: %s start failed %d %s",
	       mon->if_name,
	       (profile == NULL) ? "(Default Settings)" : "(Profile)");
    }
    return;
}

static void
MonitoredInterfaceStartAuthenticationWithProfile(MonitoredInterfaceRef mon,
						 EAPOLClientProfileRef profile)
{
    EAPOLClientItemIDRef 	itemID;

    if (profile == NULL) {
    	itemID = EAPOLClientItemIDCreateDefault();
    }
    else {
	itemID = EAPOLClientItemIDCreateWithProfile(profile);
    }
    MonitoredInterfaceStartAuthentication(mon, itemID);
    CFRelease(itemID);
    return;
}

static void
MonitoredInterfaceStopAuthentication(MonitoredInterfaceRef mon)
{
    int			status;

    my_log(LOG_NOTICE, "EAPOLMonitor: %s stopping", mon->if_name);
    status = EAPOLControlStop(mon->if_name);
    if (status != 0) {
	my_log(LOG_NOTICE,
	       "EAPOLMonitor: %s stop failed with %d",
	       mon->if_name, status);
    }
    return;
}

static void
MonitoredInterfaceStopIfNecessary(MonitoredInterfaceRef mon)
{
    EAPOLControlState 		control_state = kEAPOLControlStateIdle;
    CFDictionaryRef		dict;
    int				error;

    error = EAPOLControlCopyStateAndStatus(mon->if_name, &control_state, &dict);
    if (error != 0) {
	my_log(LOG_DEBUG,
	       "EAPOLMonitor: EAPOLControlCopyStateAndStatus(%s) returned %d",
	       mon->if_name, error);
	return;
    }
    switch (control_state) {
    case kEAPOLControlStateIdle:
    case kEAPOLControlStateStopping:
	break;
    case kEAPOLControlStateStarting:
	/* XXX we really need to wait for the state to be Running */
    case kEAPOLControlStateRunning:
	if (dict != NULL) {
	    CFStringRef		manager_name;

	    manager_name = CFDictionaryGetValue(dict,
						kEAPOLControlManagerName);
	    if (manager_name != NULL
		&& CFEqual(manager_name, S_manager_name)) {
		/* it's ours, so stop it */
		MonitoredInterfaceStopAuthentication(mon);
	    }
	}
	break;
    }
    my_CFRelease(&dict);
    return;
}

static void
MonitoredInterfacePromptComplete(CFUserNotificationRef cfun,
				 CFOptionFlags response_flags)
{
    MonitoredInterfaceRef 	mon;
    EAPOLClientProfileRef	profile;
    int				which_profile = -1;

    mon = MonitoredInterfaceLookupByCFUN(cfun);
    if (mon == NULL) {
	my_log(LOG_ERR, "EAPOLMonitor: can't find user notification");
	return;
    }
    switch (S_CFUserNotificationResponse(response_flags)) {
    case kCFUserNotificationDefaultResponse:
	/* start the authentication */
	which_profile = (response_flags
			 & CFUserNotificationPopUpSelection(-1)) >> 24;
	if (which_profile == 0) {
	    profile = NULL;
	}
	else {
	    profile = (EAPOLClientProfileRef)
		CFArrayGetValueAtIndex(mon->profiles, which_profile - 1);
	}
	MonitoredInterfaceStartAuthenticationWithProfile(mon, profile);
	break;
    default:
	mon->state = kMonitorStateIdle;
	mon->ignore_until_link_status_changes = TRUE;
	break;
    }
    MonitoredInterfaceReleasePrompt(mon);
    return;
}

static void
MonitoredInterfaceDisplayProfilePrompt(MonitoredInterfaceRef mon,
				       PromptStartContextRef context)
{
    CFBundleRef			bundle;
    CFUserNotificationRef 	cfun = NULL;
    CFMutableDictionaryRef	dict = NULL;
    SInt32			error = 0;
    CFStringRef			if_name_loc;
    CFStringRef			notif_header;
    CFStringRef			notif_header_format;
    CFRunLoopSourceRef		rls = NULL;
    CFURLRef			url = NULL;

    MonitoredInterfaceReleasePrompt(mon);
    bundle = S_get_bundle();
    if (bundle == NULL) {
	syslog(LOG_NOTICE, "EAPOLMonitor: can't find bundle");
	return;
    }
    url = CFBundleCopyBundleURL(bundle);
    if (url == NULL) {
	my_log(LOG_ERR, "EAPOLMonitor: can't find bundle URL");
	goto done;
    }
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    CFDictionaryAddValue(dict, kCFUserNotificationAlternateButtonTitleKey,
			 CFSTR("CANCEL"));
    CFDictionaryAddValue(dict, kCFUserNotificationDefaultButtonTitleKey,
			 CFSTR("CONNECT"));
    notif_header_format
	= CFBundleCopyLocalizedString(bundle, 
				      CFSTR("NOTIFICATION_HEADER"),
				      CFSTR("NOTIFICATION_HEADER %@"),
				      NULL);
    if_name_loc = copy_localized_if_name(mon->if_name_cf);
    notif_header = CFStringCreateWithFormat(NULL, NULL,
					    notif_header_format,
					    (if_name_loc != NULL)
					    ? if_name_loc
					    : mon->if_name_cf);
    CFRelease(notif_header_format);
    my_CFRelease(&if_name_loc);
    CFDictionarySetValue(dict, kCFUserNotificationAlertHeaderKey,
			 notif_header);
    CFRelease(notif_header);
    CFDictionarySetValue(dict, kCFUserNotificationPopUpTitlesKey,
			 context->profile_names);
    CFDictionarySetValue(dict, kCFUserNotificationLocalizationURLKey,
			 url);
    CFRelease(url);

    /* icon */
    url = copy_icon_url(CFSTR("Network"));
    if (url != NULL) {
	CFDictionarySetValue(dict, kCFUserNotificationIconURLKey,
			     url);
	CFRelease(url);
    }
    cfun = CFUserNotificationCreate(NULL, 0, 0, &error, dict);
    if (cfun == NULL) {
	my_log(LOG_ERR, "CFUserNotificationCreate() failed, %d", error);
	goto done;
    }
    rls = CFUserNotificationCreateRunLoopSource(NULL, cfun,
						MonitoredInterfacePromptComplete,
						0);
    if (rls == NULL) {
	my_log(LOG_ERR, "CFUserNotificationCreateRunLoopSource() failed");
	my_CFRelease(&cfun);
    }
    else {
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	mon->rls = rls;
	mon->cfun = cfun;
	mon->profiles = CFRetain(context->profiles);
	mon->state = kMonitorStatePrompting;
	my_log(LOG_DEBUG, "EAPOLMonitor: %s prompting user",
	       mon->if_name);
    }

 done:
    my_CFRelease(&dict);
    return;
}


static void
MonitoredInterfaceProcessStatus(MonitoredInterfaceRef mon,
				CFDictionaryRef status_dict)
{
    CFDataRef			authenticator;
    EAPClientStatus		client_status;
    CFStringRef			manager_name;
    bool			our_manager;
    CFStringRef			profileID;
    SupplicantState		supp_state;

    supp_state = S_get_plist_uint32(status_dict, kEAPOLControlSupplicantState,
				    -1);
    client_status = S_get_plist_uint32(status_dict, kEAPOLControlClientStatus,
				       kEAPClientStatusOK);
    /* ignore this if it's not ours */
    if (S_get_plist_uint32(status_dict, kEAPOLControlUID, -1) != getuid()) {
	return;
    }
    my_log(LOG_DEBUG, "EAPOLMonitor: %s is %s", mon->if_name,
	   SupplicantStateString(supp_state));
    manager_name = CFDictionaryGetValue(status_dict,
					kEAPOLControlManagerName);
    authenticator = CFDictionaryGetValue(status_dict,
					 kEAPOLControlAuthenticatorMACAddress);
    profileID = CFDictionaryGetValue(status_dict,
				     kEAPOLControlUniqueIdentifier);
    our_manager = (manager_name != NULL 
		   && CFEqual(manager_name, S_manager_name));
    switch (supp_state) {
    case kSupplicantStateDisconnected:
    case kSupplicantStateConnecting:
    case kSupplicantStateAcquired:
    case kSupplicantStateAuthenticating:
    case kSupplicantStateLogoff:
	break;
    case kSupplicantStateAuthenticated: {
	EAPOLClientItemIDRef	itemID = NULL;

	if (profileID != NULL) {
	    EAPOLClientConfigurationRef	cfg;
	    EAPOLClientProfileRef	profile;

	    cfg = EAPOLClientConfigurationCreate(NULL);
	    profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
	    if (profile != NULL) {
		itemID = EAPOLClientItemIDCreateWithProfile(profile);
	    }
	}
	else {
	    itemID = EAPOLClientItemIDCreateDefault();
	}
	if (itemID != NULL && authenticator != NULL) {
	    EAPOLControlSetItemIDForAuthenticator(authenticator, itemID);
	    CFRelease(itemID);
	}
	break;
    }
    case kSupplicantStateHeld:
	if (our_manager) {
	    switch (client_status) {
	    case kEAPClientStatusOK:
	    case kEAPClientStatusFailed:
		break;
	    default:
		/* clear the binding, since we ran into trouble */
		if (authenticator != NULL) {
		    EAPOLControlSetItemIDForAuthenticator(authenticator, NULL);
		}
		break;
	    }
	}
	break;
    case kSupplicantStateInactive:
	if (our_manager) {
	    bool		stop_it = FALSE;
	    
	    if (mon->state != kMonitorStateStarted) {
		stop_it = TRUE;
	    }
	    else if (mon->link_active_when_started == FALSE) {
		CFAbsoluteTime	now;
	    
		now = CFAbsoluteTimeGetCurrent();
		if ((now - mon->start_time) > 0.9) {
		    stop_it = TRUE;
		}
		else {
		    /* if we *just* started the authentication, ignore this */
		    my_log(LOG_DEBUG, "EAPOLMonitor: %s delay was %g seconds",
			   mon->if_name, now - mon->start_time);
		}
	    }
	    else {
		stop_it = TRUE;
	    }
	    if (stop_it) {
		MonitoredInterfaceStopAuthentication(mon);
	    }
	}
	break;
    case kSupplicantStateNoAuthenticator:
	break;
    }
    return;
}

static void
MonitoredInterfaceCheckStatus(MonitoredInterfaceRef mon)
{
    EAPOLControlState 		control_state = kEAPOLControlStateIdle;
    CFDictionaryRef		dict;
    int				error;

    MonitoredInterfaceReleasePrompt(mon);
    error = EAPOLControlCopyStateAndStatus(mon->if_name, &control_state, &dict);
    if (error != 0) {
	my_log(LOG_DEBUG,
	       "EAPOLMonitor: EAPOLControlCopyStateAndStatus(%s) returned %d",
	       mon->if_name, error);
	return;
    }
    switch (control_state) {
    case kEAPOLControlStateIdle:
    case kEAPOLControlStateStopping:
	mon->state = kMonitorStateIdle;
	if (EAPOLControlDidUserCancel(mon->if_name)) {
	    mon->ignore_until_link_status_changes = TRUE;
	}
	break;
    case kEAPOLControlStateStarting:
	/* not quite ready yet, we'll wait for Running */
	break;
    case kEAPOLControlStateRunning:
	if (mon->state != kMonitorStateStarted) {
	    /* user is controlling things */
	    mon->state = kMonitorStateIdle;
	    mon->ignore_until_link_status_changes = TRUE;
	}
	if (dict != NULL) {
	    MonitoredInterfaceProcessStatus(mon, dict);
	}
	break;
    }
    my_CFRelease(&dict);
    return;
}

static void
MonitoredInterfaceLinkStatusChanged(MonitoredInterfaceRef mon)
{
    bool		link_active;

    link_active = is_link_active(mon->if_name);
    if (link_active == FALSE) {
	if (mon->state == kMonitorStatePrompting) {
	    MonitoredInterfaceReleasePrompt(mon);
	    mon->state = kMonitorStateIdle;
	}
    }
    mon->require_fresh_detection = TRUE;
    mon->ignore_until_link_status_changes = FALSE;
    return;
}

static void 
prompt_or_start(const void * key, const void * value, void * arg)
{
    uint32_t			age_seconds = 0;
    CFDataRef			authenticator = NULL;
    CFDictionaryRef		dict;
    EAPOLClientItemIDRef	itemID = NULL;
    PromptStartContextRef	context_p = (PromptStartContextRef)arg;
    CFStringRef			if_name_cf = (CFStringRef)key;
    MonitoredInterfaceRef	mon;

    mon = MonitoredInterfaceLookupByName(if_name_cf);
    if (mon == NULL) {
	mon = MonitoredInterfaceCreate(if_name_cf);
    }
    my_log(LOG_DEBUG, "EAPOLMonitor: %s auto-detected", mon->if_name);
    if (mon->ignore_until_link_status_changes) {
	my_log(LOG_DEBUG, "EAPOLMonitor: %s ignoring until link status changes",
	       mon->if_name);
	/* wait until the link status changes before dealing with this */
	return;
    }
    if (mon->state != kMonitorStateIdle) {
	return;
    }

    /* check for an existing binding */
    dict = isA_CFDictionary(value);
    if (dict != NULL) {
	CFNumberRef	age_seconds_cf;

	age_seconds_cf
	    = CFDictionaryGetValue(dict,
				   kEAPOLAutoDetectSecondsSinceLastPacket);
	if (isA_CFNumber(age_seconds_cf) != NULL) {
	    (void)CFNumberGetValue(age_seconds_cf,
				   kCFNumberSInt32Type,
				   &age_seconds);
	    if (age_seconds == 0) {
		mon->require_fresh_detection = FALSE;
	    }
	    else {
		if (is_link_active(mon->if_name) == FALSE) {
		    my_log(LOG_DEBUG,
			   "EAPOLMonitor: %s link isn't active",
			   mon->if_name);
		    return;
		}
		if (mon->require_fresh_detection && age_seconds > 2) {
		    /* if the link status changed, wait for next packet */
		    my_log(LOG_DEBUG,
			   "EAPOLMonitor: %s wait for a fresh detect",
			   mon->if_name);
		    return;
		}
		if (age_seconds > 30) {
		    /* if it's been awhile, wait for the next packet */
		    my_log(LOG_DEBUG,
			   "EAPOLMonitor: %s ignoring (%d seconds)",
			   mon->if_name, age_seconds);
		    return;
		}
	    }
	}
	authenticator 
	    = CFDictionaryGetValue(dict,
				   kEAPOLAutoDetectAuthenticatorMACAddress);
	authenticator = isA_CFData(authenticator);
    }
    if (authenticator != NULL) {
	itemID = EAPOLControlCopyItemIDForAuthenticator(authenticator);
    }
    if (itemID != NULL) {
	MonitoredInterfaceStartAuthentication(mon, itemID);
	CFRelease(itemID);
    }
    else if (context_p->profiles != NULL) {
	MonitoredInterfaceDisplayProfilePrompt(mon, context_p);
    }
    else {
	MonitoredInterfaceStartAuthenticationWithProfile(mon, NULL);
    }
    return;
}

static void
process_auto_detect_info(CFDictionaryRef auto_detect_info)
{
    PromptStartContext		context;

    my_log(LOG_DEBUG, "EAPOLMonitor: processing auto-detect information");
    PromptStartContextInit(&context);
    CFDictionaryApplyFunction(auto_detect_info, prompt_or_start, &context);
    PromptStartContextRelease(&context);
    return;
}

static void
process_interface_change_info(CFArrayRef if_status_changed)
{
    int		count;
    int		i;

    count = CFArrayGetCount(if_status_changed);
    for (i = 0; i < count; i++) {
	CFStringRef		if_name_cf;
	MonitoredInterfaceRef	mon;

	if_name_cf = CFArrayGetValueAtIndex(if_status_changed, i);
	mon = MonitoredInterfaceLookupByName(if_name_cf);
	if (mon != NULL) {
	    if (mon->state == kMonitorStateIgnorePermanently) {
		/* ignore non-ethernet interfaces */
		continue;
	    }
	}
	else {
	    mon = MonitoredInterfaceCreate(if_name_cf);
	    if (get_ifm_type(mon->if_name) != IFM_ETHER) {
		mon->state = kMonitorStateIgnorePermanently;
		continue;
	    }
	}
	MonitoredInterfaceCheckStatus(mon);
    }
    return;
}

static void
handle_changes(SCDynamicStoreRef store, CFArrayRef changes, void * info)
{
    bool		check_auto_detect = FALSE;
    int			count;
    int			i;
    CFMutableArrayRef	if_status_changed = NULL;

    count = CFArrayGetCount(changes);
    for (i = 0; i < count; i++) {
	CFStringRef	key = CFArrayGetValueAtIndex(changes, i);

	if (CFEqual(key, kEAPOLControlAutoDetectInformationNotifyKey)) {
	    if (S_auto_connect) {
		check_auto_detect = TRUE;
	    }
	}
	else if (CFStringHasSuffix(key, kSCEntNetLink)) {
	    CFStringRef			if_name_cf;
	    MonitoredInterfaceRef	mon;

	    /* State:/Network/Interface/<ifname>/Link */
	    if_name_cf = my_CFStringCopyComponent(key, CFSTR("/"), 3);
	    if (if_name_cf == NULL) {
		continue;
	    }
	    mon = MonitoredInterfaceLookupByName(if_name_cf);
	    CFRelease(if_name_cf);
	    if (mon != NULL) {
		MonitoredInterfaceLinkStatusChanged(mon);
	    }
	}
	else {
	    CFStringRef		this_if;

	    this_if = EAPOLControlKeyCopyInterface(key);
	    if (this_if != NULL) {
		if (if_status_changed == NULL) {
		    if_status_changed 
			= CFArrayCreateMutable(NULL, count,
					       &kCFTypeArrayCallBacks);
		}
		CFArrayAppendValue(if_status_changed, this_if);
		CFRelease(this_if);
	    }
	}
    }

    /* process EAPOL state changes */
    if (if_status_changed != NULL) {
	process_interface_change_info(if_status_changed);
    }

    /* process interfaces that we've auto-detected 802.1X */
    if (check_auto_detect && S_on_console(store)) {
	CFDictionaryRef    	auto_detect_info = NULL;

	(void)EAPOLControlCopyAutoDetectInformation(&auto_detect_info);
	if (auto_detect_info != NULL) {
	    process_auto_detect_info(auto_detect_info);
	}
	my_CFRelease(&auto_detect_info);
    }
    my_CFRelease(&if_status_changed);
    return;
}

static void
check_settings(SCDynamicStoreRef store)
{
    if (S_auto_connect) {
	/* start auto-connect */
	CFDictionaryRef		auto_detect_info;
	
	if (S_on_console(store)) {
	    my_log(LOG_DEBUG, "EAPOLMonitor: auto-connect enabled");
	    (void)EAPOLControlCopyAutoDetectInformation(&auto_detect_info);
	    if (auto_detect_info != NULL) {
		process_auto_detect_info(auto_detect_info);
		CFRelease(auto_detect_info);
	    }
	}
    }
    else {
	MonitoredInterfaceRef	scan;
	
	/* stop auto-connect */
	my_log(LOG_NOTICE, "EAPOLMonitor: auto-connect disabled");
	LIST_FOREACH(scan, S_MonitoredInterfaceHead_p, entries) {
	    MonitoredInterfaceStopIfNecessary(scan);
	    MonitoredInterfaceReleasePrompt(scan);
	    scan->state = kMonitorStateIdle;
	    scan->ignore_until_link_status_changes = FALSE;
	}

    }
    return;
}

static void
settings_changed(CFMachPortRef port, void * msg, CFIndex size, void * info)
{
    bool		auto_connect;
    MyType *		me = (MyType *)info;

    my_log(LOG_DEBUG, "EAPOLMonitor: settings changed");
    S_verbose = EAPOLControlIsUserAutoConnectVerboseEnabled();
    auto_connect = EAPOLControlIsUserAutoConnectEnabled();
    if (auto_connect != S_auto_connect) {
	S_auto_connect = auto_connect;
	check_settings(me->store);
    }
    return;
}

static void
add_settings_notification(MyType * me)
{
    CFMachPortContext		context = {0, NULL, NULL, NULL, NULL};
    CFMachPortRef		notify_port_cf;
    mach_port_t			notify_port;
    int				notify_token;
    CFRunLoopSourceRef		rls;
    uint32_t			status;

    notify_port = MACH_PORT_NULL;
    status = notify_register_mach_port(kEAPOLControlUserSettingsNotifyKey,
				       &notify_port, 0, &notify_token);
    if (status != NOTIFY_STATUS_OK) {
	my_log(LOG_ERR, "EAPOLMonitor: notify_register_mach_port() failed");
	return;
    }
    context.info = me;
    notify_port_cf = CFMachPortCreateWithPort(NULL, notify_port,
					      settings_changed, 
					      &context,
					      NULL);
    if (notify_port_cf == NULL) {
	my_log(LOG_ERR, "EAPOLMonitor: CFMachPortCreateWithPort() failed");
	(void)notify_cancel(notify_token);
	return;
    }
    rls = CFMachPortCreateRunLoopSource(NULL, notify_port_cf, 0);
    if (rls == NULL) {
	my_log(LOG_ERR, "EAPOLMonitor: CFMachPortCreateRunLoopSource() failed");
	CFRelease(notify_port_cf);
	(void)notify_cancel(notify_token);
	return;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
    me->settings_change.mp = notify_port_cf;
    me->settings_change.token = notify_token;
    return;
}

/**
 ** Plug-in functions
 **/
static void 
myInstall(void * myInstance)
{
    SCDynamicStoreContext    	context = {0, myInstance, NULL, NULL, NULL};
    MyType *			me = (MyType *)myInstance;
    CFStringRef			key[2];
    CFArrayRef			keys;
    CFArrayRef			patterns;
    CFRunLoopSourceRef		rls;
    SCDynamicStoreRef		store;

    store = SCDynamicStoreCreate(NULL, CFSTR("EAPOLMonitor"),
				 handle_changes, &context);
    if (store == NULL) {
	my_log(LOG_ERR,
	       "EAPOLMonitor: SCDynamicStoreCreate failed, %s",
	       SCErrorString(SCError()));
	return;
    }

    key[0] = kEAPOLControlAutoDetectInformationNotifyKey;
    keys = CFArrayCreate(NULL, (const void **)key, 1,
			 &kCFTypeArrayCallBacks);

    key[0] = EAPOLControlAnyInterfaceKeyCreate();
    key[1] = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							   kSCDynamicStoreDomainState,
							   kSCCompAnyRegex,
							   kSCEntNetLink);
    patterns = CFArrayCreate(NULL, (const void **)key, 2,
			     &kCFTypeArrayCallBacks);
    CFRelease(key[0]);
    CFRelease(key[1]);

    SCDynamicStoreSetNotificationKeys(store, keys, patterns);
    CFRelease(keys);
    CFRelease(patterns);

    rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    me->store = store;
    me->rls = rls;
    add_settings_notification(me);
    S_verbose = EAPOLControlIsUserAutoConnectVerboseEnabled();
    S_auto_connect = EAPOLControlIsUserAutoConnectEnabled();
    check_settings(store);
    return;
}

static void _deallocMyType(MyType *myInstance);

static HRESULT myQueryInterface(void *myInstance, REFIID iid, LPVOID *ppv) {
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes(NULL, iid);

    // Test the requested ID against the valid interfaces.
    if (CFEqual(interfaceID, kUserEventAgentInterfaceID)) {
	((MyType *) myInstance)->_UserEventAgentInterface->AddRef(myInstance);
	*ppv = myInstance;
	CFRelease(interfaceID);
	return S_OK;
    }
    if (CFEqual(interfaceID, IUnknownUUID)) {
	((MyType *) myInstance)->_UserEventAgentInterface->AddRef(myInstance);
	*ppv = myInstance;
	CFRelease(interfaceID);
	return S_OK;
    }
    //  Requested interface unknown, bail with error.
    *ppv = NULL;
    CFRelease(interfaceID);
    return E_NOINTERFACE;
}

static ULONG myAddRef(void *myInstance) {
    ((MyType *) myInstance)->_refCount++;
    return ((MyType *) myInstance)->_refCount;
}

static ULONG myRelease(void *myInstance) {
    ((MyType *) myInstance)->_refCount--;
    if (((MyType *) myInstance)->_refCount == 0) {
	_deallocMyType((MyType *) myInstance);
	return 0;
    } else return ((MyType *) myInstance)->_refCount;
}

static UserEventAgentInterfaceStruct UserEventAgentInterfaceFtbl = {
    NULL,                    // Required padding for COM
    myQueryInterface,        // These three are the required COM functions
    myAddRef,
    myRelease,
    myInstall	             // Interface implementation
}; 

static MyType *_allocMyType(CFUUIDRef factoryID) {
    MyType *newOne = (MyType *)malloc(sizeof(MyType));

    bzero(newOne, sizeof(*newOne));
    newOne->_UserEventAgentInterface = &UserEventAgentInterfaceFtbl;
  
    if (factoryID) {
	newOne->_factoryID = (CFUUIDRef)CFRetain(factoryID);
	CFPlugInAddInstanceForFactory(factoryID);
	newOne->store = NULL;
    }
  
    newOne->_refCount = 1;
    return newOne;
}

static void
my_CFRunLoopSourceRelease(CFRunLoopSourceRef * rls_p)
{
    CFRunLoopSourceRef	rls = *rls_p;

    if (rls != NULL) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), rls,
			      kCFRunLoopDefaultMode);
	my_CFRelease(rls_p);
    }
    return;
}

static void _deallocMyType(MyType * me) 
{
    CFUUIDRef factoryID = me->_factoryID;

    if (factoryID != NULL) {
	MonitoredInterfaceRef		mon;

	CFPlugInRemoveInstanceForFactory(factoryID);
	my_CFRelease(&me->store);
	my_CFRunLoopSourceRelease(&me->rls);

	if (me->settings_change.mp != NULL) {
	    CFMachPortInvalidate(me->settings_change.mp);
	    my_CFRelease(&me->settings_change.mp);
	    (void)notify_cancel(me->settings_change.token);
	}

	/* release the list of MonitoredInterfaceRef's */
	while ((mon = LIST_FIRST(S_MonitoredInterfaceHead_p)) != NULL) {
	    MonitoredInterfaceStopIfNecessary(mon);
	    MonitoredInterfaceRelease(&mon);
	}
	CFRelease(factoryID);
    }
    free(me);
    return;
}

void *UserEventAgentFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
    if (CFEqual(typeID, kUserEventAgentTypeID)) {
	MyType *result = _allocMyType(kUserEventAgentFactoryID);
	return result;
    } else return NULL;
}

#ifdef TEST_PLUGIN
int
main()
{
    MyType *newOne = (MyType *)malloc(sizeof(MyType));

    bzero(newOne, sizeof(*newOne));
    myInstall(newOne);
    CFRunLoopRun();
    exit(0);
    return (0);
}
#endif TEST_PLUGIN
