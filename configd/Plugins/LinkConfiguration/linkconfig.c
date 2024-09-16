/*
 * Copyright (c) 2002-2023 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * October 21, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <net/if.h>
#include <net/if_media.h>
#include <os/feature_private.h>
#include "WiFiUtil.h"

#define __SYSCONFIG	"com.apple.SystemConfiguration"
#define __LINKCONFIG 	"LinkConfiguration"
#define LINKCONFIG	__SYSCONFIG "." __LINKCONFIG

#define	SC_LOG_HANDLE		__log_LinkConfiguration
#define SC_LOG_HANDLE_TYPE	static
#include "SCNetworkConfigurationInternal.h"

static CFMutableDictionaryRef	baseSettings		= NULL;
static CFStringRef		interfacesKey		= NULL;
static SCDynamicStoreRef	store			= NULL;
static CFMutableDictionaryRef	wantSettings		= NULL;


#pragma mark -
#pragma mark Logging


static os_log_t
__log_LinkConfiguration(void)
{
	static os_log_t	log	= NULL;

	if (log == NULL) {
		log = os_log_create(__SYSCONFIG, __LINKCONFIG);
	}

	return log;
}

static inline dispatch_queue_t
linkconfig_queue(void)
{
	static dispatch_queue_t		q;

	if (q == NULL) {
		q = dispatch_queue_create(LINKCONFIG, NULL);
	}
	return (q);
}


#pragma mark -

#pragma mark ioctls

static int S_socket = -1;

static int
ioctl_socket_get(const char * msg)
{
	if (S_socket < 0) {
		S_socket = socket(AF_INET, SOCK_DGRAM, 0);
		if (S_socket < 0) {
			SC_log(LOG_ERR, "%s: socket() failed: %s",
			       msg, strerror(errno));
		}
	}
	return (S_socket);
}

static void
ioctl_socket_close(void)
{
	if (S_socket < 0) {
		return;
	}
	close(S_socket);
	S_socket = -1;
}

static int
ioctl_siocsifconstrained(int s, const char * ifname, Boolean enable)
{
	struct ifreq	ifr;
	int		ret;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_constrained = enable ? 1 : 0;
	ret = ioctl(s, SIOCSIFCONSTRAINED, &ifr);
	if (ret < 0) {
		SC_log(LOG_ERR,
		       "ioctl(%s, SIOCSIFCONSTRAINED %u failed, %s",
		       ifname, ifr.ifr_constrained, strerror(errno));
	}
	return (ret);
}

static int
ioctl_siocsifexpensive(int s, const char * ifname, Boolean is_expensive)
{
	struct ifreq	ifr;
	int		ret;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_expensive = is_expensive ? 1 : 0;
	ret = ioctl(s, SIOCSIFEXPENSIVE, &ifr);
	if (ret < 0) {
		SC_log(LOG_ERR,
		       "ioctl(%s, SIOCSIFEXPENSIVE %u failed, %s",
		       ifname, ifr.ifr_constrained, strerror(errno));
	}
	return (ret);
}

#pragma mark -
#pragma mark Capabilities


#define	CAPABILITIES_KEY	CFSTR("_CAPABILITIES_")


static Boolean
_SCNetworkInterfaceSetCapabilities(SCNetworkInterfaceRef	interface,
				   CFDictionaryRef		options)
{
	CFDictionaryRef	baseOptions;
	int		cap_base;
	int		cap_current;
	int		cap_requested;
	CFStringRef	interfaceName;

#ifdef	SIOCSIFCAP
	struct ifreq	ifr;
	int		ret;
	int		sock;
#endif	// SIOCSIFCAP

	interfaceName = SCNetworkInterfaceGetBSDName(interface);
	if (interfaceName == NULL) {
		/* if no BSD interface name */
		return FALSE;
	}

	cap_current = __SCNetworkInterfaceCreateCapabilities(interface, -1, NULL);
	if (cap_current == -1) {
		/* could not get current capabilities */
		return FALSE;
	}

	// get base capabilities
	cap_base = cap_current;
	baseOptions = CFDictionaryGetValue(baseSettings, interfaceName);
	if (baseOptions != NULL) {
		CFNumberRef	num;

		num = CFDictionaryGetValue(baseOptions, CAPABILITIES_KEY);
		if (num != NULL) {
			CFNumberGetValue(num, kCFNumberIntType, &cap_base);
		}
	}

	cap_requested = __SCNetworkInterfaceCreateCapabilities(interface, cap_base, options);

#ifdef	SIOCSIFCAP
	if (cap_requested == cap_current) {
		/* if current setting is as requested */
		return TRUE;
	}

	memset((char *)&ifr, 0, sizeof(ifr));
	(void)_SC_cfstring_to_cstring(interfaceName, ifr.ifr_name, sizeof(ifr.ifr_name), kCFStringEncodingASCII);
	ifr.ifr_curcap = cap_current;
	ifr.ifr_reqcap = cap_requested;

	sock = ioctl_socket_get(__func__);
	if (sock == -1) {
		return FALSE;
	}

	ret = ioctl(sock, SIOCSIFCAP, (caddr_t)&ifr);
	if (ret == -1) {
		SC_log(LOG_ERR, "%@: ioctl(SIOCSIFCAP) failed: %s", interfaceName, strerror(errno));
		return FALSE;
	}
#endif	// SIOCSIFCAP

	return TRUE;
}


#pragma mark -
#pragma mark Media options


static CFDictionaryRef
__copyMediaOptions(CFDictionaryRef options)
{
	CFMutableDictionaryRef	requested	= NULL;
	CFTypeRef		val;

	if (!isA_CFDictionary(options)) {
		return NULL;
	}

	val = CFDictionaryGetValue(options, kSCPropNetEthernetMediaSubType);
	if (isA_CFString(val)) {
		requested = CFDictionaryCreateMutable(NULL,
						      0,
						      &kCFTypeDictionaryKeyCallBacks,
						      &kCFTypeDictionaryValueCallBacks);
		CFDictionaryAddValue(requested, kSCPropNetEthernetMediaSubType, val);
	} else {
		/* if garbage */;
		return NULL;
	}

	val = CFDictionaryGetValue(options, kSCPropNetEthernetMediaOptions);
	if (isA_CFArray(val)) {
		CFDictionaryAddValue(requested, kSCPropNetEthernetMediaOptions, val);
	} else {
		/* if garbage */;
		CFRelease(requested);
		return NULL;
	}

	return requested;
}


static Boolean
_SCNetworkInterfaceSetMediaOptions(SCNetworkInterfaceRef	interface,
				   CFDictionaryRef		options)
{
	CFArrayRef		available	= NULL;
	CFDictionaryRef		current		= NULL;
	struct ifmediareq	ifm;
	struct ifreq		ifr;
	CFStringRef		interfaceName;
	Boolean			ok		= FALSE;
	int			newOptions;
	CFDictionaryRef		requested;
	int			sock		= -1;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	interfaceName = SCNetworkInterfaceGetBSDName(interface);
	if (interfaceName == NULL) {
		/* if no BSD interface name */
		SC_log(LOG_INFO, "no BSD interface name for %@", interface);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	/* get current & available options */
	if (!SCNetworkInterfaceCopyMediaOptions(interface, &current, NULL, &available, FALSE)) {
		/* could not get current media options */
		SC_log(LOG_INFO, "no media options for %@", interfaceName);
		return FALSE;
	}

	/* extract just the dictionary key/value pairs of interest */
	requested = __copyMediaOptions(options);
	if (requested == NULL) {
		CFDictionaryRef	baseOptions;

		/* get base options */
		baseOptions = CFDictionaryGetValue(baseSettings, interfaceName);
		requested = __copyMediaOptions(baseOptions);
	}
	if (requested == NULL) {
		/* get base options */
		requested = __copyMediaOptions(current);
	}
	if (requested == NULL) {
		/* if no media options to set */
		goto done;
	}

	if ((current != NULL) && CFEqual(current, requested)) {
		/* if current settings are as requested */
		ok = TRUE;
		goto done;
	}

	if (!CFArrayContainsValue(available, CFRangeMake(0, CFArrayGetCount(available)), requested)) {
		/* if requested settings not currently available */
		SC_log(LOG_INFO, "requested media settings unavailable for %@", interfaceName);
		goto done;
	}

	newOptions = __SCNetworkInterfaceCreateMediaOptions(interface, requested);
	if (newOptions == -1) {
		/* since we have just validated, this should never happen */
		goto done;
	}

	sock = ioctl_socket_get(__func__);
	if (sock == -1) {
		goto done;
	}

	memset((char *)&ifm, 0, sizeof(ifm));
	(void)_SC_cfstring_to_cstring(interfaceName, ifm.ifm_name, sizeof(ifm.ifm_name), kCFStringEncodingASCII);

	if (ioctl(sock, SIOCGIFXMEDIA, (caddr_t)&ifm) == -1) {
		SC_log(LOG_ERR, "%@: ioctl(SIOCGIFXMEDIA) failed: %s", interfaceName, strerror(errno));
		goto done;
	}

	memset((char *)&ifr, 0, sizeof(ifr));
	memcpy(ifr.ifr_name, ifm.ifm_name, sizeof(ifr.ifr_name));
	ifr.ifr_media =  ifm.ifm_current & ~(IFM_NMASK|IFM_TMASK|IFM_OMASK|IFM_GMASK);
	ifr.ifr_media |= newOptions;

	SC_log(LOG_INFO, "old media settings: 0x%8.8x (0x%8.8x)", 
	       (unsigned int)ifm.ifm_current, (unsigned int)ifm.ifm_active);
	SC_log(LOG_INFO, "new media settings: 0x%8.8x", (unsigned int)ifr.ifr_media);

	if (ioctl(sock, SIOCSIFMEDIA, (caddr_t)&ifr) == -1) {
		SC_log(LOG_ERR, "%@: ioctl(SIOCSIFMEDIA) failed: %s", interfaceName, strerror(errno));
		goto done;
	}

	ok = TRUE;

    done :

	if (available != NULL)	CFRelease(available);
	if (current != NULL)	CFRelease(current);
	if (requested != NULL)	CFRelease(requested);

	return ok;
}


#pragma mark -
#pragma mark MTU


static Boolean
interfaceSetMTU(CFStringRef interfaceName, int mtu)
{
	struct ifreq	ifr;
	int		ret;
	int		sock;

	memset((char *)&ifr, 0, sizeof(ifr));
	(void)_SC_cfstring_to_cstring(interfaceName, ifr.ifr_name, sizeof(ifr.ifr_name), kCFStringEncodingASCII);
	ifr.ifr_mtu = mtu;

	sock = ioctl_socket_get(__func__);
	if (sock == -1) {
		return FALSE;
	}

	ret = ioctl(sock, SIOCSIFMTU, (caddr_t)&ifr);
	if (ret == -1) {
		SC_log(LOG_ERR, "%@: ioctl(SIOCSIFMTU) failed: %s", interfaceName, strerror(errno));
		return FALSE;
	}
	SC_log(LOG_NOTICE, "%@: set MTU to %d", interfaceName, mtu);
	return TRUE;
}


static Boolean
_SCNetworkInterfaceSetMTU(SCNetworkInterfaceRef	interface,
			  CFDictionaryRef	options)
{
	CFArrayRef			bridge_members		= NULL;
	Boolean				bridge_updated		= FALSE;
	CFStringRef			interfaceName;
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
	CFStringRef			interfaceType;
	int				mtu_cur			= -1;
	int				mtu_max			= -1;
	int				mtu_min			= -1;
	Boolean				ok			= TRUE;
	int				requested;
	CFNumberRef			val;

	interfaceName = SCNetworkInterfaceGetBSDName(interface);
	if (interfaceName == NULL) {
		/* if no BSD interface name */
		return FALSE;
	}

	if (!SCNetworkInterfaceCopyMTU(interface, &mtu_cur, &mtu_min, &mtu_max)) {
		/* could not get current MTU */
		return FALSE;
	}

	val = NULL;
	if (isA_CFDictionary(options)) {
		val = CFDictionaryGetValue(options, kSCPropNetEthernetMTU);
		val = isA_CFNumber(val);
	}
	if (val == NULL) {
		CFDictionaryRef	baseOptions;

		/* get base MTU */
		baseOptions = CFDictionaryGetValue(baseSettings, interfaceName);
		if (baseOptions != NULL) {
			val = CFDictionaryGetValue(baseOptions, kSCPropNetEthernetMTU);
		}
	}
	if (val != NULL) {
		CFNumberGetValue(val, kCFNumberIntType, &requested);
	} else {
		requested = mtu_cur;
	}

	if (requested == mtu_cur) {
		/* if current setting is as requested */
		return TRUE;
	}

	if (((mtu_min >= 0) && (requested < mtu_min)) ||
	    ((mtu_max >= 0) && (requested > mtu_max))) {
		/* if requested MTU outside of the valid range */
		return FALSE;
	}

	interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
	if (CFEqual(interfaceType, kSCNetworkInterfaceTypeBridge)) {
		bridge_members = SCBridgeInterfaceGetMemberInterfaces(interface);
		if ((bridge_members != NULL) && (CFArrayGetCount(bridge_members) == 0)) {
			/* if no members */
			bridge_members = NULL;
		}
		if (bridge_members != NULL) {
			/* temporarily, remove all bridge members */
			CFRetain(bridge_members);
			ok = SCBridgeInterfaceSetMemberInterfaces(interface, NULL);
			if (!ok) {
				goto done;
			}

			/* and update the (bridge) configuration */
			ok = _SCBridgeInterfaceUpdateConfiguration(interfacePrivate->prefs);
			if (!ok) {
				goto done;
			}

			bridge_updated = TRUE;
		}
	}

	/* set MTU on the bridge interface */
	(void) interfaceSetMTU(interfaceName, requested);

    done :

	if (bridge_members != NULL) {
		CFIndex	n_members	= CFArrayGetCount(bridge_members);

		/* set MTU for each of the bridge members */
		for (CFIndex i = 0; i < n_members; i++) {
			SCNetworkInterfaceRef	member;
			CFStringRef		memberName;

			member = CFArrayGetValueAtIndex(bridge_members, i);
			memberName = SCNetworkInterfaceGetBSDName(member);
			(void) interfaceSetMTU(memberName, requested);
		}

		/* add the members back into the bridge */
		(void) SCBridgeInterfaceSetMemberInterfaces(interface, bridge_members);
		CFRelease(bridge_members);

		if (bridge_updated) {
			/* and update the (bridge) configuration */
			(void) _SCBridgeInterfaceUpdateConfiguration(interfacePrivate->prefs);
		}
	}

	return ok;
}


#pragma mark -
#pragma mark Wi-Fi Expensive Overide

static dispatch_source_t	S_expensive_timer;

static SCNetworkInterfaceCost
getInterfaceTypeCost(SCDynamicStoreRef store, CFStringRef type,
		     CFDateRef * ret_expire)
{
	return __SCDynamicStoreGetNetworkOverrideInterfaceTypeCost(store,
								   type,
								   ret_expire);
}

static CFStringRef
copy_wifi_interface_name(void)
{
	CFArrayRef	if_list;
	CFStringRef	ret_name = NULL;

	if_list = SCNetworkInterfaceCopyAll();
	if (if_list == NULL) {
		goto done;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(if_list);
	     i < count; i++) {
		SCNetworkInterfaceRef	netif;

		netif = (SCNetworkInterfaceRef)
			CFArrayGetValueAtIndex(if_list, i);
		if (_SCNetworkInterfaceIsWiFiInfra(netif)) {
			ret_name = SCNetworkInterfaceGetBSDName(netif);
			CFRetain(ret_name);
			break;
		}
	}
 done:
	if (if_list != NULL) {
		CFRelease(if_list);
	}
	return (ret_name);
}

static void
expensive_timer_cancel(void)
{
	if (S_expensive_timer != NULL) {
		SC_log(LOG_NOTICE, "Wi-Fi expensive timer cancelled");
		dispatch_source_cancel(S_expensive_timer);
		dispatch_release(S_expensive_timer);
		S_expensive_timer = NULL;
	}
	return;
}

static CFStringRef
expensive_get_wifi_interface(void)
{
	static CFStringRef	interface;

	if (interface == NULL) {
		interface = copy_wifi_interface_name();
		if (interface != NULL) {
			SC_log(LOG_NOTICE, "Wi-Fi is %@",
			       interface);
		}
	}
	return (interface);
}

static void
set_expensive(CFStringRef interfaceName, Boolean enable)
{
	char	name[IFNAMSIZ];
	int	s;

	s = ioctl_socket_get(__func__);
	if (s < 0) {
		return;
	}
	if (!CFStringGetCString(interfaceName, name, sizeof(name),
				kCFStringEncodingUTF8)) {
		SC_log(LOG_NOTICE, "%s: can't convert %@ to string",
		       __func__, interfaceName);
		return;
	}
	if (ioctl_siocsifexpensive(s, name, enable) >= 0) {
		SC_log(LOG_NOTICE, "%s expensive on %s success",
		       enable ? "enable" : "disable", name);
	}
}

static void
expensive_set_wifi_cost(SCNetworkInterfaceCost cost)
{
	Boolean		is_expensive;
	CFStringRef	name;

	name = expensive_get_wifi_interface();
	if (name == NULL) {
		return;
	}
	if (cost == kSCNetworkInterfaceCostUnspecified) {
		is_expensive = WiFiIsExpensive();
		SC_log(LOG_NOTICE, "%@: Wi-Fi is %sexpensive",
		       name, is_expensive ? "" : "in");
	}
	else {
		is_expensive = (cost == kSCNetworkInterfaceCostExpensive);
		SC_log(LOG_NOTICE,
		       "%@: Wi-Fi using %sexpensive override",
		       name, is_expensive ? "" : "in");
	}
	set_expensive(name, is_expensive);
	return;
}

static void
expensive_override_expired(void)
{
	expensive_set_wifi_cost(kSCNetworkInterfaceCostUnspecified);
	expensive_timer_cancel();
	ioctl_socket_close();
}


static void
expensive_timer_start(CFDateRef expiration)
{
	CFTimeInterval		delta;
	CFAbsoluteTime		expiration_time;
	CFAbsoluteTime		now;
	dispatch_time_t		t;

	/* cancel existing timer */
	expensive_timer_cancel();

	/* schedule new timer */
	SC_log(LOG_NOTICE, "Wi-Fi expensive expiration time %@", expiration);
	now = CFAbsoluteTimeGetCurrent();
	expiration_time = CFDateGetAbsoluteTime(expiration);
	delta = expiration_time - now;
	SC_log(LOG_DEBUG, "expiration %g - now %g = %g",
	       expiration_time, now, delta);
#define NANOSECS_PER_SEC	(1000 * 1000 * 1000)
	t = dispatch_time(DISPATCH_WALLTIME_NOW,
			  delta * NANOSECS_PER_SEC);
	S_expensive_timer
		= dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
					 linkconfig_queue());
	dispatch_source_set_event_handler(S_expensive_timer,
					  ^{ expensive_override_expired(); });
	dispatch_source_set_timer(S_expensive_timer, t,
				  DISPATCH_TIME_FOREVER, 0);
	dispatch_resume(S_expensive_timer);
}

static void
updateWiFiExpensiveOverride(SCDynamicStoreRef store)
{
	SCNetworkInterfaceCost	cost;
	CFDateRef		expiration = NULL;

	cost = getInterfaceTypeCost(store, kSCNetworkInterfaceTypeIEEE80211,
				    &expiration);
	expensive_set_wifi_cost(cost);
	if (cost == kSCNetworkInterfaceCostUnspecified) {
		expensive_timer_cancel();
	}
	else if (expiration != NULL) {
		expensive_timer_start(expiration);
	}
	if (expiration != NULL) {
		CFRelease(expiration);
	}
}


#pragma mark -
#pragma mark Low Data Mode

static inline bool
lowDataModeFeatureEnabled(void)
{
	return os_feature_enabled(Network, low_data_mode);
}

static Boolean
get_number_as_bool(CFDictionaryRef info, CFStringRef prop)
{
	CFNumberRef	val = NULL;
	Boolean		ret = FALSE;

	if (isA_CFDictionary(info) != NULL) {
		int	enable = 0;

		val = CFDictionaryGetValue(info, prop);
		if (isA_CFNumber(val) != NULL
		    && CFNumberGetValue(val, kCFNumberIntType, &enable)) {
			ret = (enable != 0) ? TRUE : FALSE;
		}
	}
	return (ret);
}

static void
set_low_data_mode(CFStringRef interfaceName, Boolean enable)
{
	char	name[IFNAMSIZ];
	int	s;

	s = ioctl_socket_get(__func__);
	if (s < 0) {
		return;
	}
	if (!CFStringGetCString(interfaceName, name, sizeof(name),
				kCFStringEncodingUTF8)) {
		SC_log(LOG_NOTICE, "%s: can't convert %@ to string",
		       __func__, interfaceName);
		return;
	}
	if (ioctl_siocsifconstrained(s, name, enable) >= 0) {
		SC_log(LOG_NOTICE, "%s LowDataMode on %s",
		       enable ? "enable" : "disable", name);
	}
}

static void
updateLowDataMode(CFStringRef ifname, CFDictionaryRef info)
{
	Boolean			low_data_mode = FALSE;
	SCNetworkInterfaceRef	netif;
	Boolean			supported;

	netif =  _SCNetworkInterfaceCreateWithBSDName(NULL, ifname, 0);
	if (netif == NULL) {
		SC_log(LOG_NOTICE,
		       "Failed to create SCNetworkInterface for %@",
		       ifname);
		return;
	}
	supported = SCNetworkInterfaceSupportsLowDataMode(netif);
	CFRelease(netif);
	if (!supported) {
		SC_log(LOG_INFO, "LowDataMode not supported with %@",
		       ifname);
		return;
	}
	low_data_mode = get_number_as_bool(info, kSCPropEnableLowDataMode);
	set_low_data_mode(ifname, low_data_mode);
}

#pragma mark -
#pragma mark Update link configuration


/*
 * Function: copyUniqueIDAndProtocol
 * Purpose:
 *   Parse either of the following two formats:
 * 	<domain>:/<component1>/<component2>/<uniqueID>
 *	<domain>:/<component1>/<component2>/<uniqueID>/<protocol>
 *   returning <uniqueID>, and if available and required, <protocol>.
 */
static CFStringRef
copyUniqueIDAndProtocol(CFStringRef str, CFStringRef *ret_protocol)
{
	CFArrayRef	components;
	CFIndex		count;
	CFStringRef 	protocol = NULL;
	CFStringRef	uniqueID = NULL;

	/*
	 * str = "<domain>:/<component1>/<component2>/<uniqueID>"
	 *   OR
	 * str = "<domain>:/<component1>/<component2>/<uniqueID>/<protocol>"
	 */
	components = CFStringCreateArrayBySeparatingStrings(NULL, str,
							    CFSTR("/"));
	count = CFArrayGetCount(components);
	if (count >= 4) {
		/* we have a uniqueID */
		uniqueID = CFArrayGetValueAtIndex(components, 3);
		CFRetain(uniqueID);
		if (count >= 5 && ret_protocol != NULL) {
			/* we have and want a protocol */
			protocol = CFArrayGetValueAtIndex(components, 4);
			CFRetain(protocol);
		}
	}
	if (ret_protocol != NULL) {
		*ret_protocol = protocol;
	}
	CFRelease(components);
	return uniqueID;
}


static void
updateLink(CFStringRef interfaceName, CFDictionaryRef options);

static CFStringRef
copy_interface_setup_key(CFStringRef ifname)
{
	CFStringRef	key;

	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainSetup,
							    ifname,
							    NULL);
	return key;
}

static CFArrayRef
copy_interface_list_setup_keys(CFArrayRef list)
{
	CFIndex			count = CFArrayGetCount(list);
	CFMutableArrayRef	keys;

	keys = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
	for (CFIndex i = 0; i < count; i++) {
		CFStringRef	key;
		CFStringRef	ifname = CFArrayGetValueAtIndex(list, i);

		key = copy_interface_setup_key(ifname);
		CFArrayAppendValue(keys, key);
		CFRelease(key);
	}
	return (keys);
}

static void
updateInterfaces(SCDynamicStoreRef store, CFArrayRef newInterfaces)
{
	CFIndex			i;
	CFArrayRef		keys = NULL;
	CFDictionaryRef		low_data_mode_info = NULL;
	CFIndex			n_old;
	CFIndex			n_new;
	static CFArrayRef	oldInterfaces	= NULL;

	n_old = (oldInterfaces != NULL) ? CFArrayGetCount(oldInterfaces) : 0;
	n_new = CFArrayGetCount(newInterfaces);
	if (lowDataModeFeatureEnabled() && n_new > 0) {
		/* create a parallel array of SCDynamicStore keys */
		keys = copy_interface_list_setup_keys(newInterfaces);
		/* copy the values for those keys */
		low_data_mode_info
			= SCDynamicStoreCopyMultiple(store, keys, NULL);
	}

	for (i = 0; i < n_new; i++) {
		CFStringRef	interfaceName;

		interfaceName = CFArrayGetValueAtIndex(newInterfaces, i);

		/* Do not update pktap interface */
		if (CFStringHasPrefix(interfaceName, CFSTR("pktap"))) {
			continue;
		}

		if ((n_old == 0) ||
		    !CFArrayContainsValue(oldInterfaces,
					  CFRangeMake(0, n_old),
					  interfaceName)) {
			CFDictionaryRef	options;

			// if new interface
			options = CFDictionaryGetValue(wantSettings, interfaceName);
			updateLink(interfaceName, options);
			if (keys != NULL && low_data_mode_info != NULL) {
				CFStringRef	key;
				CFDictionaryRef	info;

				key = CFArrayGetValueAtIndex(keys, i);
				info = CFDictionaryGetValue(low_data_mode_info,
							    key);
				info = isA_CFDictionary(info);
				updateLowDataMode(interfaceName, info);
			}
		}
	}

	if (oldInterfaces != NULL) CFRelease(oldInterfaces);
	oldInterfaces = CFRetain(newInterfaces);
	if (keys != NULL) {
		CFRelease(keys);
	}
	if (low_data_mode_info != NULL) {
		CFRelease(low_data_mode_info);
	}
}


static void
updateLink(CFStringRef interfaceName, CFDictionaryRef options)
{
	SCNetworkInterfaceRef	interface;

	/* retain requested configuration */
	if (options != NULL) {
		CFDictionarySetValue(wantSettings, interfaceName, options);
	} else {
		CFDictionaryRemoveValue(wantSettings, interfaceName);
	}

	/* apply requested configuration */
	interface = _SCNetworkInterfaceCreateWithBSDName(NULL, interfaceName,
							 kIncludeAllVirtualInterfaces);
	if (interface == NULL) {
		return;
	}

	if (options != NULL) {
		if (!CFDictionaryContainsKey(baseSettings, interfaceName)) {
			int			cur_cap		= -1;
			CFDictionaryRef		cur_media	= NULL;
			CFMutableDictionaryRef	new_media	= NULL;
			int			cur_mtu		= -1;

			/* preserve current media options */
			if (SCNetworkInterfaceCopyMediaOptions(interface, &cur_media, NULL, NULL, FALSE)) {
				if (cur_media != NULL) {
					new_media = CFDictionaryCreateMutableCopy(NULL, 0, cur_media);
					CFRelease(cur_media);
				}
			}

			/* preserve current MTU */
			if (SCNetworkInterfaceCopyMTU(interface, &cur_mtu, NULL, NULL)) {
				if (cur_mtu != -1) {
					CFNumberRef	num;

					if (new_media == NULL) {
						new_media = CFDictionaryCreateMutable(NULL,
										      0,
										      &kCFTypeDictionaryKeyCallBacks,
										      &kCFTypeDictionaryValueCallBacks);
					}

					num = CFNumberCreate(NULL, kCFNumberIntType, &cur_mtu);
					CFDictionaryAddValue(new_media, kSCPropNetEthernetMTU, num);
					CFRelease(num);
				}
			}

			/* preserve capabilities */
			cur_cap = __SCNetworkInterfaceCreateCapabilities(interface, -1, NULL);
			if (cur_cap != -1) {
				CFNumberRef	num;

				if (new_media == NULL) {
					new_media = CFDictionaryCreateMutable(NULL,
									      0,
									      &kCFTypeDictionaryKeyCallBacks,
									      &kCFTypeDictionaryValueCallBacks);
				}

				num = CFNumberCreate(NULL, kCFNumberIntType, &cur_cap);
				CFDictionaryAddValue(new_media, CAPABILITIES_KEY, num);
				CFRelease(num);
			}

			if (new_media != NULL) {
				CFDictionarySetValue(baseSettings, interfaceName, new_media);
				CFRelease(new_media);
			}
		}

		/* establish new settings */
		(void)_SCNetworkInterfaceSetCapabilities(interface, options);
		(void)_SCNetworkInterfaceSetMediaOptions(interface, options);
		(void)_SCNetworkInterfaceSetMTU         (interface, options);
	} else {
		/* no requested settings */
		options = CFDictionaryGetValue(baseSettings, interfaceName);
		if (options != NULL) {
			/* restore original settings */
			(void)_SCNetworkInterfaceSetCapabilities(interface, options);
			(void)_SCNetworkInterfaceSetMediaOptions(interface, options);
			(void)_SCNetworkInterfaceSetMTU         (interface, options);
			CFDictionaryRemoveValue(baseSettings, interfaceName);
		}
	}

	CFRelease(interface);
	return;
}

static void
linkConfigChangedCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void *arg)
{
#pragma unused(arg)
	CFDictionaryRef		changes;
	CFIndex			i;
	CFIndex			n;

	changes = SCDynamicStoreCopyMultiple(store, changedKeys, NULL);

	n = (changes != NULL) ? CFArrayGetCount(changedKeys) : 0;
	for (i = 0; i < n; i++) {
		CFStringRef	key;
		CFDictionaryRef	info;

		key  = CFArrayGetValueAtIndex(changedKeys, i);
		info = CFDictionaryGetValue(changes, key);
		info = isA_CFDictionary(info);
		if (CFEqual(key, interfacesKey)) {
			if (info != NULL) {
				CFArrayRef	interfaces;

				interfaces = CFDictionaryGetValue(info, kSCPropNetInterfaces);
				if (isA_CFArray(interfaces)) {
					updateInterfaces(store, interfaces);
				}
			}
		} else if (CFStringHasSuffix(key,
					     kSCNetworkInterfaceTypeIEEE80211)) {
			/* override settings changed */
			updateWiFiExpensiveOverride(store);
		} else {
			CFStringRef	interfaceName;
			CFStringRef	protocol = NULL;

			interfaceName = copyUniqueIDAndProtocol(key, &protocol);
			if (interfaceName != NULL) {
				if (protocol != NULL) {
					updateLink(interfaceName, info);
					CFRelease(protocol);
				} else {
					updateLowDataMode(interfaceName, info);
				}
				CFRelease(interfaceName);
			}
		}
	}

	if (changes != NULL) {
		CFRelease(changes);
	}
	ioctl_socket_close();
	return;
}

__private_extern__
void
load_LinkConfiguration(CFBundleRef bundle, Boolean bundleVerbose)
{
#pragma unused(bundleVerbose)
	CFStringRef		key;
	CFMutableArrayRef	keys		= NULL;
	Boolean			ok;
	CFMutableArrayRef	patterns	= NULL;

	SC_log(LOG_DEBUG, "load() called");
	SC_log(LOG_DEBUG, "  bundle ID = %@", CFBundleGetIdentifier(bundle));

	/* initialize a few globals */

	baseSettings = CFDictionaryCreateMutable(NULL,
						 0,
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);
	wantSettings = CFDictionaryCreateMutable(NULL,
						 0,
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);

	/* open a "configd" store to allow cache updates */
	store = SCDynamicStoreCreate(NULL,
				     CFSTR("Link Configuraton plug-in"),
				     linkConfigChangedCallback,
				     NULL);
	if (store == NULL) {
		SC_log(LOG_ERR, "SCDynamicStoreCreate() failed: %s", SCErrorString(SCError()));
		goto error;
	}

	/* establish notification keys and patterns */
	keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	/* ...watch for a change in the list of network interfaces */
	interfacesKey = SCDynamicStoreKeyCreateNetworkInterface(NULL,
								kSCDynamicStoreDomainState);
	CFArrayAppendValue(keys, interfacesKey);

	/* ...watch for changes to Wi-Fi interface cost override */
	key = __SCDynamicStoreKeyCreateNetworkOverrideInterfaceType(NULL,
								    kSCNetworkInterfaceTypeIEEE80211);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/* ...watch for (per-interface) AirPort configuration changes */
	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainSetup,
							    kSCCompAnyRegex,
							    kSCEntNetAirPort);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);

	/* ...watch for (per-interface) Ethernet configuration changes */
	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainSetup,
							    kSCCompAnyRegex,
							    kSCEntNetEthernet);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);

#if	TARGET_OS_OSX
	/* ...watch for (per-interface) FireWire configuration changes */
	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainSetup,
							    kSCCompAnyRegex,
							    kSCEntNetFireWire);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);
#endif	// TARGET_OS_OSX

	if (lowDataModeFeatureEnabled()) {
		key = copy_interface_setup_key(kSCCompAnyRegex);
		CFArrayAppendValue(patterns, key);
		CFRelease(key);
	}

	/* register the keys/patterns */
	ok = SCDynamicStoreSetNotificationKeys(store, keys, patterns);
	CFRelease(keys);
	CFRelease(patterns);
	if (!ok) {
		SC_log(LOG_NOTICE, "SCDynamicStoreSetNotificationKeys() failed: %s",
		       SCErrorString(SCError()));
		goto error;
	}

	ok = SCDynamicStoreSetDispatchQueue(store, linkconfig_queue());
	if (!ok) {
		SC_log(LOG_NOTICE, "SCDynamicStoreSetDispatchQueue() failed: %s",
		       SCErrorString(SCError()));
		goto error;
	}

	return;

    error :

	if (baseSettings != NULL)	CFRelease(baseSettings);
	if (wantSettings != NULL)	CFRelease(wantSettings);
	if (store != NULL) 		CFRelease(store);
	return;
}


#ifdef TEST_LINKCONFIG


#pragma mark -
#pragma mark Standalone test code


int
main(int argc, char * const argv[])
{
	SCPreferencesRef	prefs;

	_sc_log     = kSCLogDestinationFile;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	prefs = SCPreferencesCreate(NULL, CFSTR("linkconfig"), NULL);
	if (prefs != NULL) {
		SCNetworkSetRef	set;

		set = SCNetworkSetCopyCurrent(prefs);
		if (set != NULL) {
			CFMutableSetRef	seen;
			CFArrayRef	services;

			services = SCNetworkSetCopyServices(set);
			if (services != NULL) {
				CFIndex		i;
				CFIndex		n;

				seen = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);

				n = CFArrayGetCount(services);
				for (i = 0; i < n; i++) {
					SCNetworkInterfaceRef	interface;
					SCNetworkServiceRef	service;

					service = CFArrayGetValueAtIndex(services, i);
					interface = SCNetworkServiceGetInterface(service);
					if ((interface != NULL) &&
					    !CFSetContainsValue(seen, interface)) {
						CFDictionaryRef	capabilities;

						capabilities = SCNetworkInterfaceCopyCapability(interface, NULL);
						if (capabilities != NULL) {
							int		cap_current;
							int		cap_requested;
							CFDictionaryRef	options;

							options = SCNetworkInterfaceGetConfiguration(interface);
							cap_current   = __SCNetworkInterfaceCreateCapabilities(interface, -1, NULL);
							cap_requested = __SCNetworkInterfaceCreateCapabilities(interface, cap_current, options);

							SCPrint(TRUE, stdout,
								CFSTR("%sinterface = %@, current = %p, requested = %p\n%@\n"),
								(i == 0) ? "" : "\n",
								SCNetworkInterfaceGetBSDName(interface),
								(void *)(uintptr_t)cap_current,
								(void *)(uintptr_t)cap_requested,
								capabilities);
							CFRelease(capabilities);
						}

						CFSetAddValue(seen, interface);
					}
				}

				CFRelease(seen);
				CFRelease(services);
			}

			CFRelease(set);
		}

		CFRelease(prefs);
	}
	ioctl_socket_close();

	load_LinkConfiguration(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif /* TEST_LINKCONFIG */
