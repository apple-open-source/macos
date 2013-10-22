/*
 * Copyright (c) 2010-2011 Apple Computer, Inc. All rights reserved.
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
#include <Kernel/IOKit/apple80211/apple80211_var.h>
#include <Kernel/IOKit/apple80211/apple80211_ioctl.h>
#include <sys/sysctl.h>
#include <ifaddrs.h>
#include <SystemConfiguration/SCNetworkSignature.h>
#if !TARGET_OS_EMBEDDED
#include <Apple80211/Apple80211API.h>
#endif /* TARGET_OS_EMBEDDED */
#include <net/if_media.h>

#include "scnc_mach_server.h"
#include "scnc_main.h"
#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "scnc_client.h"
#include "ipsec_manager.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "ppp_socket_server.h"
#include "scnc_utils.h"
#include "if_ppplink.h"
#include "PPPControllerPriv.h"
#include "pppd.h"
#include "ipsec_utils.h"
#include "../Drivers/PPTP/PPTP-plugin/pptp.h"
#include "../Drivers/L2TP/L2TP-plugin/l2tp.h"

#if !TARGET_OS_EMBEDDED

#define FAR_FUTURE          (60.0 * 60.0 * 24.0 * 365.0 * 1000.0)
#define WAIT_SSID_TIMEOUT    20

// NID -> Network ID or Signature
// NAP -> Network Attachment Point (basically SSID)
#define KEY_VPNNETWORKLOCATION_SSID              CFSTR("SSID_STR")
#define KEY_VPNNETWORKLOCATION_INTERFINFO        CFSTR("CachedScanRecord")
#define VPN_INTERFACE_NID_PREFIX                 CFSTR("VPN.RemoteAddress=")

static CFStringRef
copyVPNInterfaceNAP (char *interface_buf)
{
	CFStringRef       interf_key;
	CFMutableArrayRef interf_keys;
	CFDictionaryRef   dict = NULL;
	CFIndex           i;
	const void *      keys_q[128];
	void **           keys = (__typeof__(keys))keys_q;
	const void *      values_q[128];
	void **           values = (__typeof__(values))values_q;
	CFIndex           n;
	CFStringRef       vpn_if =  NULL;
	CFStringRef       result = NULL;

	if (!interface_buf) {
		return NULL;
	}

	if (gDynamicStore) {
		vpn_if = CFStringCreateWithCStringNoCopy(NULL,
												 interface_buf,
												 kCFStringEncodingASCII,
												 kCFAllocatorNull);
		if (!vpn_if) {
			// if we could not initialize interface CFString
			goto done;
		}
		
		interf_keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		// get State:/Network/Interface/<vpn_if>/Airport
		interf_key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
																   kSCDynamicStoreDomainState,
																   vpn_if,
																   kSCEntNetAirPort);
		CFArrayAppendValue(interf_keys, interf_key);
		CFRelease(interf_key);
		dict = SCDynamicStoreCopyMultiple(gDynamicStore, interf_keys, NULL);
		CFRelease(interf_keys);
		
		if (!dict) {
			// if we could not access the SCDynamicStore
			goto done;
		}
		// look for the service which matches the provided prefixes
		n = CFDictionaryGetCount(dict);
		if (n <= 0) {
			goto done;
		}
		if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys   = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
			values = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
		}
		CFDictionaryGetKeysAndValues(dict, (const void **)keys, (const void **)values);
		for (i=0; i < n; i++) {
			CFStringRef     s_key  = (CFStringRef)keys[i];
			CFDictionaryRef s_dict = (CFDictionaryRef)values[i];
			CFDictionaryRef i_dict = NULL;
			CFStringRef     nap;
			
			if (!isA_CFString(s_key) || !isA_CFDictionary(s_dict)) {
				continue;
			}

			i_dict = CFDictionaryGetValue(s_dict, KEY_VPNNETWORKLOCATION_INTERFINFO);
			if (!isA_CFDictionary(i_dict)) {
				continue;
			}

			nap = CFDictionaryGetValue(i_dict, KEY_VPNNETWORKLOCATION_SSID);
			if (nap) {
				result = CFStringCreateCopy(NULL, nap);
				SCLog(TRUE, LOG_INFO, CFSTR("%s: found nap %@, interf %s"), __FUNCTION__, result, interface_buf);
				goto done;
			}
		}
		done :
		if (vpn_if) CFRelease(vpn_if);
		if (keys != (__typeof__(keys))keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, values);
		}
		if (dict) CFRelease(dict);
	}
	return result;
}

int
checkVPNInterfaceOrServiceBlocked (const char        *location,
								   char              *interface_buf)
{
	// check to see if interface is captive: if so, bail if the interface is not ready.
	if (check_interface_captive_and_not_ready(gDynamicStore, interface_buf)) {
		// TODO: perhaps we should wait for a few seconds?
		return true;
	}

	// return 1, if this is a delete event, and;
	// TODO: add support for IPv6 <rdar://problem/5920237>
	// walk Setup:/Network/Service/* and check if there are service entries referencing this interface. e.g. Setup:/Network/Service/44DB8790-0177-4F17-8D4E-37F9413D1D87/Interface:DeviceName == interface, other_serv_found = 1
	// Setup:/Network/Interface/"interface"/AirPort:'PowerEnable' == 0 || Setup:/Network/Interface/"interface"/IPv4 is missing, interf_down = 1	
	if (gDynamicStore) {
		CFStringRef       interf_key;
		CFMutableArrayRef interf_keys;
		CFStringRef       pattern;
		CFMutableArrayRef patterns;
		CFDictionaryRef   dict = NULL;
		CFIndex           i;
		const void *      keys_q[128];
		const void **     keys = keys_q;
		const void *      values_q[128];
		const void **     values = values_q;
		CFIndex           n;
		CFStringRef       vpn_if;
		int               other_serv_found = 0, interf_down = 0;
		
		vpn_if = CFStringCreateWithCStringNoCopy(NULL,
												 interface_buf,
												 kCFStringEncodingASCII,
												 kCFAllocatorNull);
		if (!vpn_if) {
			// if we could not initialize interface CFString
			syslog(LOG_NOTICE, "%s: failed to initialize interface CFString", location);
			goto done;
		}
		
		interf_keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		// get Setup:/Network/Interface/<vpn_if>/Airport
		interf_key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
																   kSCDynamicStoreDomainSetup,
																   vpn_if,
																   kSCEntNetAirPort);
		CFArrayAppendValue(interf_keys, interf_key);
		CFRelease(interf_key);
		// get State:/Network/Interface/<vpn_if>/Airport
		interf_key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
																   kSCDynamicStoreDomainState,
																   vpn_if,
																   kSCEntNetAirPort);
		CFArrayAppendValue(interf_keys, interf_key);
		CFRelease(interf_key);
		// get Setup:/Network/Service/*/Interface
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
															  kSCDynamicStoreDomainSetup,
															  kSCCompAnyRegex,
															  kSCEntNetInterface);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		// get Setup:/Network/Service/*/IPv4
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
															  kSCDynamicStoreDomainSetup,
															  kSCCompAnyRegex,
															  kSCEntNetIPv4);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		dict = SCDynamicStoreCopyMultiple(gDynamicStore, interf_keys, patterns);
		CFRelease(interf_keys);
		CFRelease(patterns);
		
		if (!dict) {
			// if we could not access the SCDynamicStore
			syslog(LOG_NOTICE, "%s: failed to initialize SCDynamicStore dictionary", location);
			CFRelease(vpn_if);
			goto done;
		}
		// look for the service which matches the provided prefixes
		n = CFDictionaryGetCount(dict);
		if (n <= 0) {
			syslog(LOG_NOTICE, "%s: empty SCDynamicStore dictionary", location);
			CFRelease(vpn_if);
			goto done;
		}
		if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys   = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
			values = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
		}
		CFDictionaryGetKeysAndValues(dict, keys, values);
		for (i=0; i < n; i++) {
			CFStringRef     s_key  = (CFStringRef)keys[i];
			CFDictionaryRef s_dict = (CFDictionaryRef)values[i];
			CFStringRef     s_if;
			
			if (!isA_CFString(s_key) || !isA_CFDictionary(s_dict)) {
				continue;
			}
			
			if (CFStringHasSuffix(s_key, kSCEntNetInterface)) {
				// is a Service Interface entity
				s_if = CFDictionaryGetValue(s_dict, kSCPropNetInterfaceDeviceName);
				if (isA_CFString(s_if) && CFEqual(vpn_if, s_if)) {
					CFArrayRef        components;
					CFStringRef       serviceIDRef = NULL, serviceKey = NULL;
					
					other_serv_found = 1;
					// extract service ID
					components = CFStringCreateArrayBySeparatingStrings(NULL, s_key, CFSTR("/"));
					if (CFArrayGetCount(components) > 3) {
						serviceIDRef = CFArrayGetValueAtIndex(components, 3);
						//if (new key) Setup:/Network/Service/service_id/IPv4 is missing, then interf_down = 1
						serviceKey = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainSetup, serviceIDRef, kSCEntNetIPv4);
						if (!serviceKey ||
						    !CFDictionaryGetValue(dict, serviceKey)) {
							syslog(LOG_NOTICE, "%s: detected disabled IPv4 Config", location);
							interf_down = 1;
						}
						if (serviceKey) CFRelease(serviceKey);
					}
					if (components) CFRelease(components);
					if (interf_down) break;
				}
				continue;
			} else if (CFStringHasSuffix(s_key, kSCEntNetAirPort)) {
				// Interface/<vpn_if>/Airport entity
				if (CFStringHasPrefix(s_key, kSCDynamicStoreDomainSetup)) {
					CFBooleanRef powerEnable = CFDictionaryGetValue(s_dict, SC_AIRPORT_POWERENABLED_KEY);
					if (isA_CFBoolean(powerEnable) &&
					    CFEqual(powerEnable, kCFBooleanFalse)) {
						syslog(LOG_NOTICE, "%s: detected AirPort, PowerEnable == FALSE", location);
						interf_down = 1;
						break;
					}
				} else if (CFStringHasPrefix(s_key, kSCDynamicStoreDomainState)) {
					UInt16      temp;
					CFNumberRef airStatus = CFDictionaryGetValue(s_dict, SC_AIRPORT_POWERSTATUS_KEY);
					if (isA_CFNumber(airStatus) &&
					    CFNumberGetValue(airStatus, kCFNumberShortType, &temp)) {
						if (temp ==0) {
							syslog(LOG_NOTICE, "%s: detected AirPort, PowerStatus == 0", location);
						}
					}
				}
				continue;
			}
		}
		if (vpn_if) CFRelease(vpn_if);
		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, values);
		}
		done :
		if (dict) CFRelease(dict);
		
		return (other_serv_found == 0 || interf_down == 1);             
	}
	return 0;
}

static CFStringRef
copyCurrentNIDFromStore (struct service *serv)
{
	CFStringRef            result = NULL;
	struct sockaddr_in     addr;

	SCLog(TRUE, LOG_INFO, CFSTR("%s:"), __FUNCTION__);

	// only one NID for now
	bzero(&addr, sizeof(addr));
	((struct sockaddr *)&addr)->sa_len = sizeof(addr);
	((struct sockaddr *)&addr)->sa_family = AF_INET;
	result = SCNetworkSignatureCopyActiveIdentifierForAddress(NULL, (struct sockaddr *)&addr);
	if (!result) {
		return NULL;
	} else if (CFStringHasPrefix(result, VPN_INTERFACE_NID_PREFIX)) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("%s: ignoring nid %@"),
		      __FUNCTION__, result);
		CFRelease(result);
		return NULL;
	}
	SCLog(TRUE, LOG_INFO, CFSTR("%s: nid %@"), __FUNCTION__, result);
	return result;
}

static CFStringRef
pollCurrentNID (struct service *serv)
{
	return copyCurrentNIDFromStore(serv);
}

static char *
getVPNInterfaceBuf (struct service *serv)
{
	char *interface_buf;

	SCLog(TRUE, LOG_INFO, CFSTR("%s:"), __FUNCTION__);

	switch (serv->type) {
	case TYPE_IPSEC:
		interface_buf = serv->u.ipsec.lower_interface;
		break;
	case TYPE_PPP:
		if (!serv->u.ppp.lower_interface[0]) {
			CFStringRef     string, key;
			CFDictionaryRef dict;

			SCLog(TRUE, LOG_INFO, CFSTR("%s: lower_interface is null"), __FUNCTION__);

			/* TODO: this part should be change to grab the real lower interface instead of the primary */
			key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
			if (key) {
				dict = SCDynamicStoreCopyValue(gDynamicStore, key);
				CFRelease(key);
				if (dict) {
					if ((string = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryInterface))) {
						CFStringGetCString(string, serv->u.ppp.lower_interface, sizeof(serv->u.ppp.lower_interface), kCFStringEncodingASCII);
						SCLog(TRUE, LOG_INFO, CFSTR("%s: lower_interface is gotten %@"), __FUNCTION__, string);
					}
					CFRelease(dict);
				}
			}
		}
		interface_buf = serv->u.ppp.lower_interface;
		SCLog(TRUE, LOG_INFO, CFSTR("%s: lower_interface is %s"), __FUNCTION__, interface_buf[0]? interface_buf : "null");
		break;
	default:
		interface_buf = NULL;
		break;
	}

	return interface_buf;
}

static Boolean
isVPNInterfaceWifi (struct service *serv)
{
	u_int32_t  media;
	char      *interface_buf = getVPNInterfaceBuf(serv);

	if (interface_buf && interface_buf[0]) {
		media = get_if_media(interface_buf);
	} else {
		media = 0;
	}
	if (IFM_TYPE(media) == IFM_IEEE80211) {
		return true;
	}
	return false;
}

static CFStringRef
copyVPNInterfaceName (struct service *serv)
{
	switch (serv->type) {
	case TYPE_IPSEC:
		return(CFStringCreateWithCString(NULL, serv->u.ipsec.lower_interface, kCFStringEncodingASCII));
	case TYPE_PPP:
		return(CFStringCreateWithCString(NULL, serv->u.ppp.lower_interface, kCFStringEncodingASCII));
	default:
		return (CFStringRef)NULL;
	}
}

static void
monitorApple80211Callback (Apple80211Err  err,
						   Apple80211Ref  ref,
						   UInt32         event,
						   void          *eventData,
						   UInt32         eventDataLen,
						   void          *context)
{
	struct service *serv = (__typeof__(serv))context;

	if (!serv) {
		return;
	}

	if (event ==  APPLE80211_M_SSID_CHANGED) {
		SCLog(TRUE, LOG_INFO, CFSTR("%s: VPN got wifi event %d"), __FUNCTION__, event);
		disconnectIfVPNLocationChanged(serv);
	}
}

static CFStringRef
copyCurrentNAPFromStore (struct service *serv)
{
	CFStringRef      nap;
	char            *interface_buf;
	CFStringRef      interfName = NULL;
	Apple80211Err    err = kA11NoErr;

	SCLog(TRUE, LOG_INFO, CFSTR("%s:"), __FUNCTION__);

	// get interface info
	interface_buf = getVPNInterfaceBuf(serv);
	nap = copyVPNInterfaceNAP(interface_buf);
	if (!nap) {
		return NULL;
	}

	// setup update notification (if not already)
	if (!serv->connection_nap_monitor) {
		interfName = copyVPNInterfaceName(serv);
		if (!interfName) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: failed to get CF interface name for %s"), __FUNCTION__, interface_buf? interface_buf : "null");
			goto init_failed;
		}

		err = Apple80211Open((Apple80211Ref *)&serv->connection_nap_monitor);
		if (err != kA11NoErr) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211Open failed"), __FUNCTION__);
			goto init_failed;
		}

		err = Apple80211BindToInterface((Apple80211Ref)serv->connection_nap_monitor, interfName);
		if (err != kA11NoErr) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211BindToInterface(%@) failed"),
				  __FUNCTION__, interfName);
			goto init_failed;
		}

		err = Apple80211EventMonitoringInit((Apple80211Ref)serv->connection_nap_monitor, monitorApple80211Callback, (void*)serv, CFRunLoopGetCurrent());
		if (err != kA11NoErr) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211EventMonitoringInit(%@) failed, %d"),
				  __FUNCTION__, interfName, err);
			goto init_failed;
		}
		err = Apple80211StartMonitoringEvent((Apple80211Ref)serv->connection_nap_monitor, APPLE80211_M_SSID_CHANGED);
		if (err != kA11NoErr) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211StartMonitoringEvent(%@) failed, %d"),
				  __FUNCTION__, interfName, err);
			goto init_failed;
		}
	}

	if (interfName) {
		CFRelease(interfName);
	}

	return nap;

init_failed:
	if (serv->connection_nap_monitor) {
		Apple80211Close((Apple80211Ref)serv->connection_nap_monitor);
		serv->connection_nap_monitor = NULL;
	}
	if (interfName) {
		CFRelease(interfName);
	}
	return nap;
}


static CFStringRef
pollCurrentNAP (struct service *serv, Boolean *wifiErr)
{
	CFDataRef        napDataRef = NULL;
	CFStringRef      nap = NULL;
	char            *interface_buf;
	CFStringRef      interfName = NULL;
	Apple80211Err    err = kA11NoErr;
	int              state;
	
	SCLog(TRUE, LOG_INFO, CFSTR("%s:"), __FUNCTION__);

	// get interface info
	interface_buf = getVPNInterfaceBuf(serv);
	
	// setup update notification (if not already)
	if (!serv->connection_nap_monitor) {
		interfName = copyVPNInterfaceName(serv);
		if (!interfName) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: failed to get CF interface name for %s"), __FUNCTION__,  interface_buf? interface_buf : "null");
			goto init_failed;
		}

		err = Apple80211Open((Apple80211Ref *)&serv->connection_nap_monitor);
		if (err != kA11NoErr) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211Open failed"), __FUNCTION__);
			*wifiErr = true;
			goto init_failed;
		}
		
		err = Apple80211BindToInterface((Apple80211Ref)serv->connection_nap_monitor, interfName);
		if (err != kA11NoErr) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211BindToInterface(%@) failed"),
				  __FUNCTION__, interfName);
			*wifiErr = true;
			goto init_failed;
		}
		
		err = Apple80211EventMonitoringInit((Apple80211Ref)serv->connection_nap_monitor, monitorApple80211Callback, (void*)serv, CFRunLoopGetCurrent());
		if (err != kA11NoErr) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211EventMonitoringInit(%@) failed, %d"),
				  __FUNCTION__, interfName, err);
			goto init_failed;
		}
		err = Apple80211StartMonitoringEvent((Apple80211Ref)serv->connection_nap_monitor, APPLE80211_M_SSID_CHANGED);
		if (err != kA11NoErr) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211StartMonitoringEvent(%@) failed, %d"),
				  __FUNCTION__, interfName, err);
			goto init_failed;
		}
	}
	
	// get current state
	err = Apple80211Get((Apple80211Ref)serv->connection_nap_monitor, APPLE80211_IOC_STATE, 0, &state, sizeof(state));
	if (err != kA11NoErr) {
		SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211GetState(%@) failed, err = %d"),
			  __FUNCTION__, interfName, err);
		*wifiErr = true;
		goto done;
	}
	if (state != APPLE80211_S_ASSOC && state != APPLE80211_S_RUN) {
		if (state > APPLE80211_S_INIT) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211GetState(%@) got invalid state = %d."),
				  __FUNCTION__, interfName, state, WAIT_SSID_TIMEOUT);
		}

		goto done;
	} else {
		SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211GetState(%@) got valid state = %d"),
			  __FUNCTION__, interfName, state);
	}

	err = Apple80211CopyValue((Apple80211Ref)serv->connection_nap_monitor, APPLE80211_IOC_SSID, NULL, &napDataRef);
	if (err != kA11NoErr) {
		SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211CopyValue(%@, SSID) failed, err = %d"),
			  __FUNCTION__, interfName, err);
		*wifiErr = true;
		goto done;
	}

	nap = CFStringCreateWithBytes(kCFAllocatorDefault, CFDataGetBytePtr(napDataRef), CFDataGetLength(napDataRef), kCFStringEncodingUTF8, false);

	SCLog(TRUE, LOG_INFO, CFSTR("%s: Apple80211CopyValue(%@, SSID) got %@"),
		  __FUNCTION__, interfName, nap);

	if (napDataRef) {
		CFRelease(napDataRef);
	}

	if (interfName) {
		CFRelease(interfName);
	}

	return nap;

init_failed:
	if (serv->connection_nap_monitor) {
		Apple80211Close((Apple80211Ref)serv->connection_nap_monitor);
		serv->connection_nap_monitor = NULL;
	}
done:
	if (nap) {
		CFRelease(nap);
	}
	if (napDataRef) {
		CFRelease(napDataRef);
	}
	if (interfName) {
		CFRelease(interfName);
	}
	return NULL;
}

static int
copyCurrentVPNLocation (struct service *serv,
						CFStringRef    *nidRef,
						CFStringRef    *napRef)
{
	int rc = 0;

	// get NID from store
	*nidRef = copyCurrentNIDFromStore(serv);

	if (isVPNInterfaceWifi(serv)) {
		// get NAP from store
		*napRef = copyCurrentNAPFromStore(serv);
	} else {
		SCLog(TRUE, LOG_INFO, CFSTR("%s: underlying interface isn't wifi. Skip NAP"), __FUNCTION__);
		*napRef = NULL;
	}
	return rc;
}

static int
pollCurrentVPNLocation (struct service *serv,
						CFStringRef    *nidRef,
						CFStringRef    *napRef)
{
	int rc = 0;

	// poll current NID
	*nidRef = pollCurrentNID(serv);

	if (isVPNInterfaceWifi(serv)) {
		Boolean wifiErr = false;

		// poll current NAP
		*napRef = pollCurrentNAP(serv, &wifiErr);
		if (wifiErr) {
			// special case disconnection: a bad/driver error while trying to detect nhood, disconnect b/c it worked before
			SCLog(TRUE, LOG_INFO, CFSTR("%s: failed to detect nap %@ because of a scan/association error"), __FUNCTION__,
				  serv->connection_nap);
			my_CFRelease(nidRef);
			rc = -1;
		}
	} else {
		SCLog(TRUE, LOG_INFO, CFSTR("%s: underlying interface isn't wifi. Skip NAP"), __FUNCTION__);
		*napRef = NULL;
	}
	return rc;
}

void
initVPNConnectionLocation (struct service *serv)
{
	// ignore connection if it isn't ipsec, l2tp or pptp
	if (!(serv->type == TYPE_IPSEC || (serv->type == TYPE_PPP && (serv->subtype == PPP_TYPE_L2TP || serv->subtype == PPP_TYPE_PPTP)))) {
		return;
	}

	SCLog(TRUE, LOG_INFO, CFSTR("%s:"), __FUNCTION__);

	// get current Location
	clearVPNLocation(serv);
	if (copyCurrentVPNLocation(serv, &serv->connection_nid, &serv->connection_nap)) {
		SCLog(TRUE, LOG_INFO, CFSTR("%s: failed to get current location"), __FUNCTION__);
	}
}

static int
verifyCurrentVPNLocation (struct service *serv,
						  Boolean        *changed)
{
	int         rc = 0;
	CFStringRef nid = NULL, nap = NULL;

	// get current Location
	(void)pollCurrentVPNLocation(serv, &nid, &nap);

	// try nap match 1st (ethernet will skip this part)
	if (nap && serv->connection_nap) {
		if (CFStringCompare(nap, serv->connection_nap, 0) == kCFCompareEqualTo) {
			SCLog(TRUE, LOG_INFO, CFSTR("%s: nap %@ matched"),
				  __FUNCTION__, nap);
			// a matching nap is good enough if nid is currently unavailable
			if (!nid || !serv->connection_nid) {
				*changed = false;
				goto done;
			}
			// otherwise nid needs to be verified (to detect change in upstream router).
		} else {
			SCLog(TRUE, LOG_NOTICE, CFSTR("%s: nap %@ changed, expected %@"),
				  __FUNCTION__, nap, serv->connection_nap);
			// we still need to verify nid, since some nap changes are ok (e.g. same basestation with two ssids).
		}
	}

	// try nid match next
	if (nid && serv->connection_nid) {
		if (CFStringCompare(nid, serv->connection_nid, 0) == kCFCompareEqualTo) {
			// upstream router matched
			SCLog(TRUE, LOG_INFO, CFSTR("%s: nid %@ matched"),
				  __FUNCTION__, nid);
			*changed = false;
			goto done;
		} else {
			// upstream router changed
			SCLog(TRUE, LOG_NOTICE, CFSTR("%s: nid %@ changed, expected %@"),
				  __FUNCTION__, nid, serv->connection_nid);
		}
	} else {
		// could only get here if !nap (e.g. ethernet or unassociated wifi).
		// could not verify location because both nap and nid were unavailable
		SCLog(TRUE, LOG_INFO, CFSTR("%s: failed to verify location"), __FUNCTION__);
		*changed = false;
		rc = -1;
		goto done;
	}

	// update changed
	*changed = true;	

done:
	if (nap) {
		CFRelease(nap);
	}
	if (nid) {
		CFRelease(nid);
	}
	return rc;
}

Boolean
didVPNLocationChange (struct service *serv)
{
	char    *interface_buf = NULL;
	Boolean  changed = false;
	int      err;

	if (!(interface_buf = getVPNInterfaceBuf(serv))) {
		SCLog(TRUE, LOG_INFO, CFSTR("%s: unsupported service type"), __FUNCTION__);
		return false;
	}

	// exit quickly if networking is disabled/blocked.
	if (checkVPNInterfaceOrServiceBlocked(__FUNCTION__,
										  interface_buf)) {
		return false;
	}

	if ((err = verifyCurrentVPNLocation(serv, &changed)) < 0) {
		return false;
	}

	return changed;
}

Boolean
disconnectIfVPNLocationChanged(struct service *serv)
{
	// ignore connection if it isn't ipsec, l2tp or pptp
	if (!(serv->type == TYPE_IPSEC || (serv->type == TYPE_PPP && (serv->subtype == PPP_TYPE_L2TP || serv->subtype == PPP_TYPE_PPTP)))) {
		return false;
	}

	if (didVPNLocationChange(serv)) {
		/* location changed */
		switch (serv->type) {
		case TYPE_PPP:
			serv->u.ppp.laststatus = EXIT_HANGUP;
			if (serv->subtype == PPP_TYPE_L2TP) {
				serv->u.ppp.lastdevstatus = EXIT_L2TP_NETWORKCHANGED;
			} else {
				serv->u.ppp.lastdevstatus = EXIT_PPTP_NETWORKCHANGED;
			}
			SCLog(TRUE, LOG_ERR, CFSTR("Controller: PPP disconnecting because the location changed"));
			ppp_stop(serv, SIGTERM);
			return true;
		case TYPE_IPSEC:
			serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;
			SCLog(TRUE, LOG_ERR, CFSTR("Controller: IPSec disconnecting because the location changed"));
			ipsec_stop(serv, SIGTERM);
			return true;
		}
	}
	return false;
}

void
clearVPNLocation (struct service *serv)
{
	if (serv->connection_nap_monitor) {
		Apple80211Close((Apple80211Ref)serv->connection_nap_monitor);
		serv->connection_nap_monitor = NULL;
	}
	if (serv->type == TYPE_PPP) {
		serv->u.ppp.lower_interface[0] = 0;
	}
	my_CFRelease(&serv->connection_nid);
	my_CFRelease(&serv->connection_nap);
}
#endif
