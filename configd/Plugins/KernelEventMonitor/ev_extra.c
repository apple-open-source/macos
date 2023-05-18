/*
 * Copyright (c) 2013-2022 Apple Inc. All rights reserved.
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
 * October 7, 2013	Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include "eventmon.h"
#include "ev_extra.h"

#include <os/feature_private.h>
#include "SCNetworkConfigurationInternal.h"
#include "WiFiUtil.h"

static SCNetworkInterfaceCost
getOverrideCost(CFStringRef type)
{
	return __SCDynamicStoreGetNetworkOverrideInterfaceTypeCost(NULL,
								   type,
								   NULL);
}

static CFBooleanRef
is_expensive(SCNetworkInterfaceRef _Nonnull interface)
{
	CFBooleanRef	expensive;
	CFStringRef	interfaceType;

	while (interface != NULL) {
		SCNetworkInterfaceRef	child;

		child = SCNetworkInterfaceGetInterface(interface);
		if (child == NULL) {
			break;
		}

		interface = child;
	}
	// by default, don't set/clear expensive
	expensive = NULL;
	if (_SCNetworkInterfaceIsTethered(interface)) {
		// if tethered (to iOS) interface
		expensive = kCFBooleanTrue;
		goto done;
	}
	if (_SCNetworkInterfaceIsBluetoothPAN(interface)) {
		// if BT-PAN interface
		expensive = kCFBooleanTrue;
		goto done;
	}
	if (_SCNetworkInterfaceIsWiFiInfra(interface)) {
		SCNetworkInterfaceCost	cost;

		// assume WiFi is not expensive
		expensive = kCFBooleanFalse;
		cost = getOverrideCost(kSCNetworkInterfaceTypeIEEE80211);
		if (cost != kSCNetworkInterfaceCostUnspecified) {
			SC_log(LOG_NOTICE,
			       "%@: Wi-Fi using %sexpensive override",
			       SCNetworkInterfaceGetBSDName(interface),
			       (cost == kSCNetworkInterfaceCostExpensive)
			       ? "" : "in");
			/* interface has been forced expensive/inexpensive */
			if (cost == kSCNetworkInterfaceCostExpensive) {
				expensive = kCFBooleanTrue;
			}
		}
		else if (WiFiIsExpensive()) {
			SC_log(LOG_NOTICE, "%@: Wi-Fi is expensive",
			       SCNetworkInterfaceGetBSDName(interface));
			expensive = kCFBooleanTrue;
		}
		goto done;
	}
	interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
	if (CFEqual(interfaceType, kSCNetworkInterfaceTypeWWAN)) {
		// if WWAN [Ethernet] interface
		expensive = kCFBooleanTrue;
	}

 done:
	return expensive;
}


static int
ifexpensive_set(int s, const char * name, uint32_t expensive)
{
#if	defined(SIOCGIFEXPENSIVE) && defined(SIOCSIFEXPENSIVE) && !defined(MAIN)
	struct ifreq	ifr;
	int		ret;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ret = ioctl(s, SIOCGIFEXPENSIVE, &ifr);
	if ((ret == -1) && (errno != EPERM)) {
		SC_log(LOG_ERR, "%s: ioctl(SIOCGIFEXPENSIVE) failed: %s", name, strerror(errno));
		return ret;
	}

	if (ifr.ifr_expensive == expensive) {
		// if no change
		return ret;
	}

	ifr.ifr_expensive = expensive;
	ret = ioctl(s, SIOCSIFEXPENSIVE, &ifr);
	if ((ret == -1) && (errno != EPERM)) {
		SC_log(LOG_ERR, "%s: ioctl(SIOCSIFEXPENSIVE) failed: %s", name, strerror(errno));
	}

	return ret;
#else	// defined(SIOCSIFEXPENSIVE) && !defined(MAIN)
	return 0;
#endif	// defined(SIOCSIFEXPENSIVE) && !defined(MAIN)
}

static SCNetworkInterfaceRef
copy_configured_interface(SCPreferencesRef prefs, CFStringRef if_name)
{
	SCNetworkSetRef		current_set = NULL;
	CFIndex			count;
	CFIndex			i;
	SCNetworkInterfaceRef	ret_if = NULL;
	CFArrayRef		services = NULL;

	if (prefs == NULL) {
		goto done;
	}
	current_set = SCNetworkSetCopyCurrent(prefs);
	if (current_set == NULL) {
		goto done;
	}
	services = SCNetworkSetCopyServices(current_set);
	if (services == NULL) {
		goto done;
	}

	count = CFArrayGetCount(services);
	for (i = 0; i < count; i++) {
		CFStringRef		this_if_name;
		SCNetworkInterfaceRef	this_if;
		SCNetworkServiceRef	s;

		s = (SCNetworkServiceRef)CFArrayGetValueAtIndex(services, i);
		if (!SCNetworkServiceGetEnabled(s)) {
			/* skip disabled services */
			continue;
		}
		this_if = SCNetworkServiceGetInterface(s);
		if (this_if == NULL) {
			continue;
		}
		this_if_name = SCNetworkInterfaceGetBSDName(this_if);
		if (this_if_name == NULL) {
			continue;
		}
		if (CFEqual(this_if_name, if_name)) {
			CFRetain(this_if);
			ret_if = this_if;
			break;
		}
	}

 done:
	if (current_set != NULL) {
		CFRelease(current_set);
	}
	if (services != NULL) {
		CFRelease(services);
	}
	return (ret_if);
}

static void
setLowDataMode(CFStringRef interface_name)
{
	CFTypeRef		enabled;
	SCNetworkInterfaceRef	netif = NULL;
	SCPreferencesRef	prefs = NULL;

	/* we need to get an SCNetworkInterfaceRef that is bound to the on-disk prefs */
	prefs = SCPreferencesCreate(NULL, CFSTR("KernelEventMonitor"), NULL);
	if (prefs == NULL) {
		SC_log(LOG_NOTICE, "SCPreferencesCreate() failed, %s",
		       SCErrorString(SCError()));
		goto done;
	}
	netif = copy_configured_interface(prefs, interface_name);
	if (netif == NULL) {
		SC_log(LOG_NOTICE, "Can't find interface for %@",
		       interface_name);
		goto done;
	}
	enabled = __SCNetworkInterfaceGetEnableLowDataModeValue(netif);
	if (enabled != NULL) {
		int	val = 0;

		CFNumberGetValue(enabled, kCFNumberIntType, &val);
		SC_log(LOG_NOTICE, "EnableLowDataModeValue(%@) is %s, skipping",
		       interface_name, (val != 0) ? "true" : "false");
		goto done;
	}
	if (!SCNetworkInterfaceSetEnableLowDataMode(netif, TRUE)) {
		SC_log(LOG_NOTICE, "SCNetworkInterfaceSetEnableLowDataModeValue(%@) failed, %s",
		       interface_name, SCErrorString(SCError()));
		goto done;
	}
	if (!SCPreferencesCommitChanges(prefs)) {
		SC_log(LOG_NOTICE, "SCPreferencesCommitChanges failed, %s",
		       SCErrorString(SCError()));
		goto done;
	}
	if (!SCPreferencesApplyChanges(prefs)) {
		SC_log(LOG_NOTICE, "SCPreferencesApplyChanges failed, %s",
		       SCErrorString(SCError()));
		goto done;
	}
	SC_log(LOG_NOTICE, "SCNetworkInterfaceSetEnableLowDataModeValue(%@) success",
	       interface_name);

 done:
	if (prefs != NULL) {
		CFRelease(prefs);
	}
	if (netif != NULL) {
		CFRelease(netif);
	}
	return;
}

__private_extern__
CFBooleanRef
interface_update_expensive(const char *if_name)
{
	CFBooleanRef		expensive = NULL;
	SCNetworkInterfaceRef	interface;
	CFStringRef		interface_name = NULL;
	int			s;
	Boolean			supports_low_data_mode = FALSE;

	interface_name = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingUTF8);
	interface = _SCNetworkInterfaceCreateWithBSDName(NULL, interface_name, kIncludeNoVirtualInterfaces);
	if (interface == NULL) {
		goto done;
	}
	expensive = is_expensive(interface);
	if (os_feature_enabled(Network, low_data_mode)) {
		supports_low_data_mode = SCNetworkInterfaceSupportsLowDataMode(interface);
	}
	CFRelease(interface);
	if (expensive == NULL) {
		goto done;
	}

	// mark ... or clear ... the [if_name] interface as "expensive"
	s = dgram_socket(AF_INET);
	if (s != -1) {
		int		ret;
		uint32_t	val = CFBooleanGetValue(expensive) ? 1 : 0;

		ret = ifexpensive_set(s, if_name, val);
		close(s);
		if (supports_low_data_mode && val != 0 && ret == 0) {
			// if we enabled expensive, check whether we need to enable LowDataMode
			setLowDataMode(interface_name);
		}
	}

 done:
	CFRelease(interface_name);
	return expensive;
}


#ifdef	MAIN

int
dgram_socket(int domain)
{
	int	s;

	s = socket(domain, SOCK_DGRAM, 0);
	if (s == -1) {
		SC_log(LOG_ERR, "socket() failed: %s", strerror(errno));
	}

	return s;
}

int
main(int argc, char * const argv[])
{
	CFBooleanRef	expensive;

	if (argc < 1 + 1) {
		SCPrint(TRUE, stderr, CFSTR("usage: %s <interface>\n"), argv[0]);
		exit(1);
	}

	expensive = interface_update_expensive(argv[1]);
	if (expensive != NULL) {
		SCPrint(TRUE, stdout, CFSTR("%s: set expensive to %@\n"), argv[1], expensive);
	} else {
		SCPrint(TRUE, stdout, CFSTR("%s: not changing expensive\n"), argv[1]);
	}

	exit(0);
}
#endif	// MAIN
