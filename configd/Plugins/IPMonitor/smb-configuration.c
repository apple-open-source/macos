/*
 * Copyright (c) 2006-2023 Apple Inc. All rights reserved.
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
 * June 26, 2006	Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb_async.h>
#include <notify.h>
#include <smb_server_prefs.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFStringDefaultEncoding.h>	// for __CFStringGetInstallationEncodingAndRegion()
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include "smb-configuration.h"

#ifdef	TEST_SMB_CONFIGURATION
#define	my_log(__level, __format, ...)	SCPrint(TRUE, stdout, CFSTR(__format "\n"), ## __VA_ARGS__)
#else	/* TEST_SMB_CONFIGURATION */
#include "ip_plugin.h"
#endif	/* TEST_SMB_CONFIGURATION */

#define	NETBIOS_NAME_LEN		16

#define	SMB_STARTUP_DELAY		60.0
#define	SMB_DEBOUNCE_DELAY		5.0

static SCDynamicStoreRef	S_store;
static dispatch_queue_t		S_queue;

static struct timeval		ptrQueryStart;
static SCNetworkReachabilityRef	ptrTarget;

static dispatch_source_t	S_debounce_timer;


static CFAbsoluteTime
boottime(void)
{
	static CFAbsoluteTime	bt	= 0;
	static dispatch_once_t	once;

	dispatch_once(&once, ^{
		int		mib[2]	= { CTL_KERN, KERN_BOOTTIME };
		struct timeval	tv;
		size_t		tv_len	= sizeof(tv);

		if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), &tv, &tv_len, NULL, 0) == -1) {
			my_log(LOG_ERR, "sysctl() CTL_KERN/KERN_BOOTTIME failed: %s", strerror(errno));
			bt = kCFAbsoluteTimeIntervalSince1970;
			return;
		}

		// Note: we need to convert from Unix time to CF time.
		bt = (CFTimeInterval)tv.tv_sec - kCFAbsoluteTimeIntervalSince1970;
		bt += (1.0E-6 * (CFTimeInterval)tv.tv_usec);
	});

	return bt;
}


static CFStringRef
copy_default_name(void)
{
	CFStringRef		model;
	CFIndex			n;
	CFMutableStringRef	str;

	// get HW model name
	model = _SC_hw_model(TRUE);
	if (model == NULL) {
		return NULL;
	}

	// start off with the [trunated] HW model
	str = CFStringCreateMutable(NULL, 0);
	CFStringAppend(str, model);

	// truncate as needed
	n = CFStringGetLength(str);
	if (n > (NETBIOS_NAME_LEN - 1)) {
		CFStringReplace(str,
				CFRangeMake(NETBIOS_NAME_LEN - 1, n - (NETBIOS_NAME_LEN - 1)),
				CFSTR(""));
		n = NETBIOS_NAME_LEN - 1;
	}

	//
	// if there is room for at least one byte (two hex characters)
	// of the MAC address than append that to the NetBIOS name.
	//
	//    NETBIOS_NAME_LEN	max length
	//      -1		the last byte is reserved
	//      -3		"-XX"
	//
	if (n < (NETBIOS_NAME_LEN - 1 - 3)) {
		SCNetworkInterfaceRef	interface;

		interface = _SCNetworkInterfaceCreateWithBSDName(NULL, CFSTR("en0"),
								 kIncludeNoVirtualInterfaces);
		if (interface != NULL) {
			CFMutableStringRef	en0_MAC;

			en0_MAC = (CFMutableStringRef)SCNetworkInterfaceGetHardwareAddressString(interface);
			if (en0_MAC != NULL) {
				CFIndex	en0_MAC_len;

				// remove ":" characters from MAC address string
				en0_MAC = CFStringCreateMutableCopy(NULL, 0, en0_MAC);
				CFStringFindAndReplace(en0_MAC,
						       CFSTR(":"),
						       CFSTR(""),
						       CFRangeMake(0, CFStringGetLength(en0_MAC)),
						       0);

				//
				// compute how may bytes (characters) to append
				//    ... and limit that number to 6
				//
				//    NETBIOS_NAME_LEN	max length
				//      -1		the last byte is reserved
				//	-n		"iMac"
				//      -1		"-"
				//
				n = ((NETBIOS_NAME_LEN - 1 - n - 1) / 2) * 2;
				if (n > 6) {
					n = 6;
				}

				// remove what we don't want
				en0_MAC_len = CFStringGetLength(en0_MAC);
				if (en0_MAC_len > n) {
					CFStringDelete(en0_MAC, CFRangeMake(0, en0_MAC_len - n));
				}

				// append
				CFStringAppendFormat(str, NULL, CFSTR("-%@"), en0_MAC);
				CFRelease(en0_MAC);
			}

			CFRelease(interface);
		}
	}

	CFStringUppercase(str, NULL);
	return str;
}


static CFDictionaryRef
smb_copy_global_configuration(SCDynamicStoreRef store)
{
	CFDictionaryRef	dict;
	CFStringRef	key;

	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetSMB);
	dict = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);

	if (dict != NULL) {
		if (isA_CFDictionary(dict)) {
			return dict;
		}

		CFRelease(dict);
	}

	dict = CFDictionaryCreate(NULL,			// allocator
				  NULL,			// keys
				  NULL,			// values
				  0,			// numValues
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
	return dict;
}


static void
update_pref(SCPreferencesRef prefs, CFStringRef key, CFTypeRef newVal, Boolean *changed)
{
	CFTypeRef	curVal;

	curVal = SCPreferencesGetValue(prefs, key);
	if (!_SC_CFEqual(curVal, newVal)) {
		if (newVal != NULL) {
			SCPreferencesSetValue(prefs, key, newVal);
		} else {
			SCPreferencesRemoveValue(prefs, key);
		}

		*changed = TRUE;
	}

	return;
}


static void
smb_set_configuration(SCDynamicStoreRef store, CFDictionaryRef dict)
{
	CFArrayRef		array;
	Boolean			changed		= FALSE;
	UInt32			dosCodepage	= 0;
	CFStringEncoding	dosEncoding	= 0;
	CFStringEncoding	macEncoding	= kCFStringEncodingMacRoman;
	uint32_t		macRegion	= 0;
	Boolean			ok;
	SCPreferencesRef	prefs;
	CFStringRef		str;

	prefs = SCPreferencesCreate(NULL, CFSTR("smb-configuration"), CFSTR(kSMBPreferencesAppID));
	if (prefs == NULL) {
		my_log(LOG_ERR,
		       "smb_set_configuration: SCPreferencesCreate() failed: %s",
		       SCErrorString(SCError()));
		return;
	}

	ok = SCPreferencesLock(prefs, TRUE);
	if (!ok) {
		my_log(LOG_ERR,
		       "smb_set_configuration: SCPreferencesLock() failed: %s",
		       SCErrorString(SCError()));
		goto done;
	}

	// Server description
	str = SCDynamicStoreCopyComputerName(store, &macEncoding);
	update_pref(prefs, CFSTR(kSMBPrefServerDescription), str, &changed);

	// DOS code page
	if (str != NULL) {
		if (macEncoding == kCFStringEncodingMacRoman) {
			CFStringRef	key;
			CFDictionaryRef	dict;

			// get region
			key = SCDynamicStoreKeyCreateComputerName(NULL);
			dict = SCDynamicStoreCopyValue(store, key);
			CFRelease(key);
			if (dict != NULL) {
				if (isA_CFDictionary(dict)) {
					CFNumberRef	num;
					SInt32		val;

					num = CFDictionaryGetValue(dict, kSCPropSystemComputerNameRegion);
					if (isA_CFNumber(num) &&
					    CFNumberGetValue(num, kCFNumberSInt32Type, &val)) {
						macRegion = (uint32_t)val;
					}
				}

				CFRelease(dict);
			}
		}

		CFRelease(str);
	} else {
		// Important: must have root acccess (eUID==0) to access the config file!
		__CFStringGetInstallationEncodingAndRegion((uint32_t *)&macEncoding, &macRegion);
	}
	_SC_dos_encoding_and_codepage(macEncoding, macRegion, &dosEncoding, &dosCodepage);
	str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%u"), dosCodepage);
	assert(str != NULL);
	update_pref(prefs, CFSTR(kSMBPrefDOSCodePage), str, &changed);
	CFRelease(str);

	// NetBIOS name
	str = CFDictionaryGetValue(dict, kSCPropNetSMBNetBIOSName);
	str = isA_CFString(str);
	update_pref(prefs, CFSTR(kSMBPrefNetBIOSName), str, &changed);

	// NetBIOS node type
	str = CFDictionaryGetValue(dict, kSCPropNetSMBNetBIOSNodeType);
	str = isA_CFString(str);
	if (str != NULL) {
		if (CFEqual(str, kSCValNetSMBNetBIOSNodeTypeBroadcast)) {
			// B-node
			str = CFSTR(kSMBPrefNetBIOSNodeBroadcast);
		} else if (CFEqual(str, kSCValNetSMBNetBIOSNodeTypePeer)) {
			// P-node
			str = CFSTR(kSMBPrefNetBIOSNodePeer);
		} else if (CFEqual(str, kSCValNetSMBNetBIOSNodeTypeMixed)) {
			// M-node
			str = CFSTR(kSMBPrefNetBIOSNodeMixed);
		} else if (CFEqual(str, kSCValNetSMBNetBIOSNodeTypeHybrid)) {
			// H-node
			str = CFSTR(kSMBPrefNetBIOSNodeHybrid);
		} else {
			str = NULL;
		}
	}
	update_pref(prefs, CFSTR(kSMBPrefNetBIOSNodeType), str, &changed);

#ifdef	ADD_NETBIOS_SCOPE
	// NetBIOS scope
	str = CFDictionaryGetValue(dict, kSCPropNetSMBNetBIOSScope);
	str = isA_CFString(str);
	update_pref(prefs, CFSTR(kSMBPrefNetBIOSScope), str, &changed);
#endif	// ADD_NETBIOS_SCOPE

	// WINS addresses
	array = CFDictionaryGetValue(dict, kSCPropNetSMBWINSAddresses);
	array = isA_CFArray(array);
	update_pref(prefs, CFSTR(kSMBPrefWINSServerAddressList), array, &changed);

	// Workgroup (or domain)
	str = CFDictionaryGetValue(dict, kSCPropNetSMBWorkgroup);
	str = isA_CFString(str);
	update_pref(prefs, CFSTR(kSMBPrefWorkgroup), str, &changed);

	if (changed) {
		ok = SCPreferencesCommitChanges(prefs);
		if (!ok) {
			if ((SCError() != EROFS)) {
				my_log(LOG_ERR,
				       "smb_set_configuration: SCPreferencesCommitChanges() failed: %s",
				       SCErrorString(SCError()));
			}
			goto done;
		}

		ok = SCPreferencesApplyChanges(prefs);
		if (!ok) {
			my_log(LOG_ERR,
			       "smb_set_configuration: SCPreferencesApplyChanges() failed: %s",
			       SCErrorString(SCError()));
			goto done;
		}
	}

    done :

	(void) SCPreferencesUnlock(prefs);
	CFRelease(prefs);
	return;
}


static CFStringRef
copy_primary_service(SCDynamicStoreRef store)
{
	CFDictionaryRef	dict;
	CFStringRef	key;
	CFStringRef	serviceID	= NULL;

	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetIPv4);
	dict = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);

	if (dict != NULL) {
		if (isA_CFDictionary(dict)) {
			serviceID = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryService);
			if (isA_CFString(serviceID)) {
				CFRetain(serviceID);
			} else {
				serviceID = NULL;
			}
		}
		CFRelease(dict);
	}

	return serviceID;
}


static CFStringRef
copy_primary_ip(SCDynamicStoreRef store, CFStringRef serviceID)
{
	CFStringRef	address	= NULL;
	CFDictionaryRef	dict;
	CFStringRef	key;

	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  serviceID,
							  kSCEntNetIPv4);
	dict = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);

	if (dict != NULL) {
		if (isA_CFDictionary(dict)) {
			CFArrayRef	addresses;

			addresses = CFDictionaryGetValue(dict, kSCPropNetIPv4Addresses);
			if (isA_CFArray(addresses) && (CFArrayGetCount(addresses) > 0)) {
				address = CFArrayGetValueAtIndex(addresses, 0);
				if (isA_CFString(address)) {
					CFRetain(address);
				} else {
					address = NULL;
				}
			}
		}
		CFRelease(dict);
	}

	return address;
}


static void
ptr_query_stop(void)
{
	if (ptrTarget == NULL) {
		return;
	}

	my_log(LOG_INFO, "NetBIOS name: ptr query stop");

	SCNetworkReachabilitySetCallback(ptrTarget, NULL, NULL);
	SCNetworkReachabilitySetDispatchQueue(ptrTarget, NULL);

	CFRelease(ptrTarget);
	ptrTarget = NULL;

	return;
}


static void
ptr_query_callback(SCNetworkReachabilityRef target, SCNetworkReachabilityFlags flags, void *info)
{
#pragma unused(info)
	CFDictionaryRef		dict;
	CFStringRef		name;
	CFMutableDictionaryRef	newDict;
	struct timeval		ptrQueryComplete;
	struct timeval		ptrQueryElapsed;

	(void) gettimeofday(&ptrQueryComplete, NULL);
	timersub(&ptrQueryComplete, &ptrQueryStart, &ptrQueryElapsed);
	my_log(LOG_INFO, "NetBIOS name: ptr query complete%s (query time = %ld.%3.3d)",
	       (flags & kSCNetworkReachabilityFlagsReachable) ? "" : ", host not found",
	       ptrQueryElapsed.tv_sec,
	       ptrQueryElapsed.tv_usec / 1000);

	// get network configuration
	dict = smb_copy_global_configuration(S_store);

	// use NetBIOS name from network configuration (if available)
	name = CFDictionaryGetValue(dict, kSCPropNetSMBNetBIOSName);
	if ((name != NULL) && _SC_CFStringIsValidNetBIOSName(name)) {
		my_log(LOG_INFO, "NetBIOS name (network configuration) = %@", name);
		goto setDict;
	}

	// use reverse DNS name, if available

	name = NULL;
	if (flags & kSCNetworkReachabilityFlagsReachable) {
		int		error_num;
		CFArrayRef	hosts;

		/*
		 * if [reverse] DNS query was successful
		 */
		hosts = SCNetworkReachabilityCopyResolvedAddress(target, &error_num);
		if (hosts != NULL) {
			if (CFArrayGetCount(hosts) > 0) {
				CFIndex			ptrLen;
				CFMutableStringRef	ptrName;
				CFRange			range;

				name = CFArrayGetValueAtIndex(hosts, 0);
				ptrName = CFStringCreateMutableCopy(NULL, 0, name);
				ptrLen = CFStringGetLength(ptrName);
				if (CFStringFindWithOptions(ptrName,
							    CFSTR("."),
							    CFRangeMake(0, ptrLen),
							    0,
							    &range)) {
					CFStringDelete(ptrName,
						       CFRangeMake(range.location, ptrLen - range.location));
				}
				name = ptrName;
			}
			CFRelease(hosts);
		}
	}
	if (name != NULL) {
		if (_SC_CFStringIsValidNetBIOSName(name)) {
			my_log(LOG_INFO, "NetBIOS name (reverse DNS query) = %@", name);
			goto setName;
		}
		CFRelease(name);
	}

	// try local (multicast DNS) name, if available
	name = SCDynamicStoreCopyLocalHostName(S_store);
	if (name != NULL) {
		if (_SC_CFStringIsValidNetBIOSName(name)) {
			my_log(LOG_INFO, "NetBIOS name (multicast DNS) = %@", name);
			goto setName;
		}
		CFRelease(name);
	}

	// use "default" name
	name = copy_default_name();
	if (name != NULL) {
		my_log(LOG_INFO, "NetBIOS name (default) = %@", name);
		goto setName;
	}

	goto setDict;

    setName :

	newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	CFDictionarySetValue(newDict, kSCPropNetSMBNetBIOSName, name);
	CFRelease(dict);
	dict = newDict;
	CFRelease(name);

    setDict :

	// update SMB configuration
	smb_set_configuration(S_store, dict);
	CFRelease(dict);

	ptr_query_stop();

#ifdef	TEST_SMB_CONFIGURATION
	printf("Exiting\n");
	exit(0);
#endif	/* TEST_SMB_CONFIGURATION */

	return;
}


static Boolean
ptr_query_start(CFStringRef address)
{
	union {
		struct sockaddr         sa;
		struct sockaddr_in      sin;
		struct sockaddr_in6     sin6;
	} addr;
	char				buf[64];
	CFDataRef			data;
	CFMutableDictionaryRef		options;

	ptr_query_stop();
	if (_SC_cfstring_to_cstring(address, buf, sizeof(buf), kCFStringEncodingASCII) == NULL) {
		my_log(LOG_ERR, "could not convert [primary] address string");
		return FALSE;
	}

	if (_SC_string_to_sockaddr(buf, AF_UNSPEC, (void *)&addr, sizeof(addr)) == NULL) {
		my_log(LOG_ERR, "could not convert [primary] address");
		return FALSE;
	}

	options = CFDictionaryCreateMutable(NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	data = CFDataCreate(NULL, (const UInt8 *)&addr.sa, addr.sa.sa_len);
	CFDictionarySetValue(options, kSCNetworkReachabilityOptionPTRAddress, data);
	CFRelease(data);
	ptrTarget = SCNetworkReachabilityCreateWithOptions(NULL, options);
	CFRelease(options);
	if (ptrTarget == NULL) {
		my_log(LOG_ERR, "could not resolve [primary] address");
		return FALSE;
	}

	my_log(LOG_INFO, "NetBIOS name: ptr query start");

	(void) gettimeofday(&ptrQueryStart, NULL);
	(void) SCNetworkReachabilitySetCallback(ptrTarget, ptr_query_callback, NULL);
	(void) SCNetworkReachabilitySetDispatchQueue(ptrTarget, S_queue);

	return TRUE;
}

static void
cancel_debounce_timer(void);

static void
update_configuration(void)
{
	CFStringRef		address		= NULL;
	CFDictionaryRef		dict;
	CFStringRef		name;
	CFStringRef		serviceID	= NULL;

	// get network configuration
	dict = smb_copy_global_configuration(S_store);

	// use NetBIOS name from network configuration (if available)
	name = CFDictionaryGetValue(dict, kSCPropNetSMBNetBIOSName);
	if ((name != NULL) && _SC_CFStringIsValidNetBIOSName(name)) {
		my_log(LOG_INFO, "NetBIOS name (network configuration) = %@", name);
		goto set;
	}

	// get primary service ID
	serviceID = copy_primary_service(S_store);
	if (serviceID == NULL) {
		// if no primary service
		goto mDNS;
	}

	// get DNS name associated with primary IP, if available
	address = copy_primary_ip(S_store, serviceID);
	if (address != NULL) {
		Boolean	ok;

		// start reverse DNS query using primary IP address
		ok = ptr_query_start(address);
		if (ok) {
			// if query started
			goto done;
		}
	}

    mDNS :

	// get local (multicast DNS) name, if available

	name = SCDynamicStoreCopyLocalHostName(S_store);
	if (name != NULL) {
		if (_SC_CFStringIsValidNetBIOSName(name)) {
			CFMutableDictionaryRef	newDict;

			my_log(LOG_INFO, "NetBIOS name (multicast DNS) = %@", name);
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
			CFDictionarySetValue(newDict, kSCPropNetSMBNetBIOSName, name);
			CFRelease(dict);
			dict = newDict;
			CFRelease(name);
			goto set;
		}
		CFRelease(name);
	}

	// get "default" name
	name = copy_default_name();
	if (name != NULL) {
		CFMutableDictionaryRef	newDict;

		my_log(LOG_INFO, "NetBIOS name (default) = %@", name);
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		CFDictionarySetValue(newDict, kSCPropNetSMBNetBIOSName, name);
		CFRelease(dict);
		dict = newDict;
		CFRelease(name);
	}

    set :

	// update SMB configuration
	smb_set_configuration(S_store, dict);

    done :

	if (address != NULL)	CFRelease(address);
	if (dict != NULL)	CFRelease(dict);
	if (serviceID != NULL)	CFRelease(serviceID);

	cancel_debounce_timer();
	return;
}

static void
cancel_debounce_timer(void)
{
	if (S_debounce_timer != NULL) {
		dispatch_source_cancel(S_debounce_timer);
		dispatch_release(S_debounce_timer);
		S_debounce_timer = NULL;
	}
}

static void
start_debounce_timer(uint64_t delay_nsecs)
{
	dispatch_block_t	handler;
	dispatch_time_t		when;

	cancel_debounce_timer();
	S_debounce_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
						  0,
						  0,
						  S_queue);
	handler = ^{
		update_configuration();
	};
	dispatch_source_set_event_handler(S_debounce_timer, handler);
	when = dispatch_time(DISPATCH_TIME_NOW,	delay_nsecs);
	dispatch_source_set_timer(S_debounce_timer,
				  when,
				  DISPATCH_TIME_FOREVER,
				  0);
	dispatch_resume(S_debounce_timer);
}

static void
configuration_changed(SCDynamicStoreRef store,
		      CFArrayRef changedKeys, void *info)
{
#pragma unused(store)
#pragma unused(changedKeys)
#pragma unused(info)
	CFAbsoluteTime		boot_time;
	CFAbsoluteTime		delay_secs;
	CFAbsoluteTime		now;

	/* cancel any in-progress attempt to resolve the primary IP address */
	ptr_query_stop();

	/*
	 * Process the change after the debounce delay time
	 * or no sooner than 60 seconds post boot.
	 */
	boot_time = boottime();
	now = CFAbsoluteTimeGetCurrent();
#ifdef TEST_SMB_CONFIGURATION
	printf("Boot time %g now %g delta %g seconds\n",
	       boot_time, now, now - boot_time);
#endif
	if (now < (boot_time + SMB_STARTUP_DELAY)) {
		delay_secs = boot_time + SMB_STARTUP_DELAY - now;
	}
	else {
		delay_secs = SMB_DEBOUNCE_DELAY;
	}
	my_log(LOG_DEBUG, "smb-configuration: waiting %g seconds",
	       delay_secs);
	start_debounce_timer(delay_secs * NSEC_PER_SEC);
	return;
}

__private_extern__
void
load_smb_configuration(dispatch_queue_t queue)
{
	CFStringRef		key;
	CFMutableArrayRef	keys		= NULL;
	notify_handler_t	notify_handler;
	int			notify_token;
	Boolean			ok;
	CFMutableArrayRef	patterns	= NULL;
	uint32_t		status;

	/* remember this for scheduling reachability callbacks */
	S_queue = queue;

	/* register for notifications */
	S_store = SCDynamicStoreCreate(NULL, CFSTR("smb-configuration"),
				       configuration_changed, NULL);
	if (S_store == NULL) {
		my_log(LOG_ERR,
		       "SCDynamicStoreCreate() failed: %s",
		       SCErrorString(SCError()));
		goto error;
	}
	keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	/* SMB configuration changes */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetSMB);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/* ComputerName changes */
	key = SCDynamicStoreKeyCreateComputerName(NULL);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/* multicast DNS hostname */
	key = SCDynamicStoreKeyCreateHostNames(NULL);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	ok = SCDynamicStoreSetNotificationKeys(S_store, keys, patterns);
	CFRelease(keys);
	CFRelease(patterns);
	if (!ok) {
		my_log(LOG_ERR,
		       "SCDynamicStoreSetNotificationKeys() failed: %s",
		       SCErrorString(SCError()));
		goto error;
	}
	ok = SCDynamicStoreSetDispatchQueue(S_store, queue);
	if (!ok) {
		my_log(LOG_ERR,
		       "SCDynamicStoreSetDispatchQueue() failed: %s",
		       SCErrorString(SCError()));
		goto error;
	}
	/* watch for primary service/interface and DNS configuration changes */
	notify_handler = ^(int token){
#pragma unused(token)
		configuration_changed(NULL, NULL, NULL);
	};
	status = notify_register_dispatch(_SC_NOTIFY_NETWORK_CHANGE,
					  &notify_token,
					  queue,
					  notify_handler);
	if (status != NOTIFY_STATUS_OK) {
		my_log(LOG_ERR, "notify_register_dispatch() failed: %u", status);
		goto error;
	}
	return;

    error :

	if (S_store != NULL) {
		SCDynamicStoreSetDispatchQueue(S_store, NULL);
		CFRelease(S_store);
		S_store = NULL;
	}
	return;
}


#ifdef	TEST_SMB_CONFIGURATION
int
main(int argc, char * const argv[])
{

#ifdef	DEBUG
	CFStringRef		address;
	CFStringRef		name;
	CFStringRef		serviceID;
	SCDynamicStoreRef	store;

	_sc_log = kSCLogDestinationFile;
	if ((argc > 1) && (strcmp(argv[1], "-d") == 0)) {
		_sc_verbose = TRUE;
		argv++;
		argc--;
	}

	store = SCDynamicStoreCreate(NULL, CFSTR("smb-configuration"), NULL, NULL);
	if (store == NULL) {
		SCPrint(TRUE, stdout,
			CFSTR("SCDynamicStoreCreate() failed: %s\n"),
			SCErrorString(SCError()));
		exit(1);
	}

	// get "default" name
	name = copy_default_name();
	if (name != NULL) {
		SCPrint(TRUE, stdout, CFSTR("default name = %@\n"), name);
		CFRelease(name);
	}

	// get primary service
	serviceID = copy_primary_service(store);
	if (serviceID != NULL) {
		SCPrint(TRUE, stdout, CFSTR("primary service ID = %@\n"), serviceID);
	} else {
		SCPrint(TRUE, stdout, CFSTR("No primary service\n"));
		goto done;
	}

	if ((argc == (2+1)) && (argv[1][0] == 's')) {
		if (serviceID != NULL)	CFRelease(serviceID);
		serviceID = CFStringCreateWithCString(NULL, argv[2], kCFStringEncodingUTF8);
		SCPrint(TRUE, stdout, CFSTR("alternate service ID = %@\n"), serviceID);
	}

	// get primary IP address
	address = copy_primary_ip(store, serviceID);
	CFRelease(serviceID);
	if (address != NULL) {
		SCPrint(TRUE, stdout, CFSTR("primary address = %@\n"), address);

		if ((argc == (2+1)) && (argv[1][0] == 'a')) {
			if (address != NULL)	CFRelease(address);
			address = CFStringCreateWithCString(NULL, argv[2], kCFStringEncodingUTF8);
			SCPrint(TRUE, stdout, CFSTR("alternate primary address = %@\n"), address);
		}

		// start reverse DNS query using primary IP address
		(void) ptr_query_start(address);
		CFRelease(address);
	}

    done :

	configuration_changed(NULL, NULL, NULL);

	CFRelease(store);

#else	/* DEBUG */

	_sc_log     = kSCLogDestinationFile;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	{
		dispatch_queue_t	queue;

		queue = dispatch_queue_create("test-smb-configuration", NULL);
		load_smb_configuration(queue);
	}

#endif	/* DEBUG */

	dispatch_main();

	exit(0);
	return 0;
}
#endif	/* TEST_SMB_CONFIGURATION */
