/*
 * Copyright (c) 2006-2011 Apple Inc. All rights reserved.
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
#include <SystemConfiguration/SCPrivate.h>		// for SCLog(), SCPrint()

#define	HW_MODEL_LEN		64			// Note: must be >= NETBIOS_NAME_LEN (below)

#define	NETBIOS_NAME_LEN	16

#define	SMB_STARTUP_DELAY	60.0
#define	SMB_DEBOUNCE_DELAY	5.0

static SCDynamicStoreRef	store		= NULL;
static CFRunLoopSourceRef	rls		= NULL;

static Boolean			dnsActive	= FALSE;
static CFMachPortRef		dnsPort		= NULL;
static struct timeval		dnsQueryStart;

static CFRunLoopTimerRef	timer		= NULL;

static Boolean			_verbose	= FALSE;


static Boolean
isMacOSXServer()
{
	static enum { Unknown, Client, Server }	isServer	= Unknown;

	if (isServer == Unknown) {
		int		ret;
		struct stat	statbuf;

		ret = stat("/System/Library/CoreServices/ServerVersion.plist", &statbuf);
		isServer = (ret == 0) ? Server : Client;
	}

	return (isServer == Server) ? TRUE : FALSE;
}


static CFAbsoluteTime
boottime(void)
{
	static CFAbsoluteTime	bt	= 0;

	if (bt == 0) {
		int		mib[2]	= { CTL_KERN, KERN_BOOTTIME };
		struct timeval	tv;
		size_t		tv_len	= sizeof(tv);

		if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), &tv, &tv_len, NULL, 0) == -1) {
			SCLog(TRUE, LOG_ERR, CFSTR("sysctl() CTL_KERN/KERN_BOOTTIME failed: %s"), strerror(errno));
			return kCFAbsoluteTimeIntervalSince1970;
		}

		// Note: we need to convert from Unix time to CF time.
		bt = (CFTimeInterval)tv.tv_sec - kCFAbsoluteTimeIntervalSince1970;
		bt += (1.0E-6 * (CFTimeInterval)tv.tv_usec);
	}

	return bt;
}


static CFStringRef
copy_default_name(void)
{
	char			*cp;
	char			hwModel[HW_MODEL_LEN];
	int			mib[]		= { CTL_HW, HW_MODEL };
	size_t			n		= sizeof(hwModel);
	int			ret;
	CFMutableStringRef	str;

	// get HW model name
	bzero(&hwModel, sizeof(hwModel));
	ret = sysctl(mib, sizeof(mib) / sizeof(mib[0]), &hwModel, &n, NULL, 0);
	if (ret != 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("sysctl() CTL_HW/HW_MODEL failed: %s"), strerror(errno));
		return NULL;
	}

	// truncate name
	hwModel[NETBIOS_NAME_LEN - 1] = '\0';

	// trim everything after (and including) a comma
	cp = index(hwModel, ',');
	if (cp != NULL) {
		*cp = '\0';
	}

	// trim any trailing digits
	n = strlen(hwModel);
	while (n > 0) {
		if (!isdigit(hwModel[n - 1])) {
			break;
		}
		hwModel[--n] = '\0';
	}

	// start off with the [trunated] HW model
	str = CFStringCreateMutable(NULL, 0);
	CFStringAppendFormat(str, NULL, CFSTR("%s"), hwModel);

	//
	// if there is room for at least one byte (two hex characters)
	// of the MAC address than append that to the NetBIOS name.
	//
	//    NETBIOS_NAME_LEN	max length
	//      -1		the last byte is reserved
	//	-3		"-XX"
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
		SCLog(TRUE, LOG_ERR,
		      CFSTR("smb_set_configuration: SCPreferencesCreate() failed: %s"),
		      SCErrorString(SCError()));
		return;
	}

	ok = SCPreferencesLock(prefs, TRUE);
	if (!ok) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("smb_set_configuration: SCPreferencesLock() failed: %s"),
		      SCErrorString(SCError()));
		goto done;
	}

	if (!isMacOSXServer()) {
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
		str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), dosCodepage);
		update_pref(prefs, CFSTR(kSMBPrefDOSCodePage), str, &changed);
		CFRelease(str);
	}

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
			SCLog((SCError() != EROFS), LOG_ERR,
			      CFSTR("smb_set_configuration: SCPreferencesCommitChanges() failed: %s"),
			      SCErrorString(SCError()));
			goto done;
		}

		ok = SCPreferencesApplyChanges(prefs);
		if (!ok) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("smb_set_configuration: SCPreferencesApplyChanges() failed: %s"),
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
reverseDNSComplete(int32_t status, char *host, char *serv, void *context)
{
	CFDictionaryRef		dict;
	struct timeval		dnsQueryComplete;
	struct timeval		dnsQueryElapsed;
	CFStringRef		name;
	SCDynamicStoreRef	store	= (SCDynamicStoreRef)context;

	(void) gettimeofday(&dnsQueryComplete, NULL);
	timersub(&dnsQueryComplete, &dnsQueryStart, &dnsQueryElapsed);
	SCLog(_verbose, LOG_INFO,
	      CFSTR("async DNS complete%s (query time = %d.%3.3d)"),
	      ((status == 0) && (host != NULL)) ? "" : ", host not found",
	      dnsQueryElapsed.tv_sec,
	      dnsQueryElapsed.tv_usec / 1000);

	// get network configuration
	dict = smb_copy_global_configuration(store);

	// use NetBIOS name from network configuration (if available)
	name = CFDictionaryGetValue(dict, kSCPropNetSMBNetBIOSName);
	if ((name != NULL) && _SC_CFStringIsValidNetBIOSName(name)) {
		SCLog(TRUE, LOG_INFO, CFSTR("NetBIOS name (network configuration) = %@"), name);
		goto set;
	}

	// use reverse DNS name, if available
	switch (status) {
		case 0 :
			/*
			 * if [reverse] DNS query was successful
			 */
			if (host != NULL) {
				char	*dot;

				dot = strchr(host, '.');
				name = CFStringCreateWithBytes(NULL,
							       (UInt8 *)host,
							       (dot != NULL) ? dot - host : strlen(host),
							       kCFStringEncodingUTF8,
							       FALSE);
				if (name != NULL) {
					if (_SC_CFStringIsValidNetBIOSName(name)) {
						CFMutableDictionaryRef	newDict;

						SCLog(TRUE, LOG_INFO, CFSTR("NetBIOS name (reverse DNS query) = %@"), name);
						newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
						CFDictionarySetValue(newDict, kSCPropNetSMBNetBIOSName, name);
						CFRelease(dict);
						dict = newDict;
						CFRelease(name);
						goto set;
					}

					CFRelease(name);
				}
			}
			break;

		case EAI_NONAME :
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
		case EAI_NODATA:
#endif
			/*
			 * if no name available
			 */
			break;

		default :
			/*
			 * Hmmmm...
			 */
			SCLog(TRUE, LOG_ERR, CFSTR("getnameinfo() failed: %s"), gai_strerror(status));
	}

	// try local (multicast DNS) name, if available
	name = SCDynamicStoreCopyLocalHostName(store);
	if (name != NULL) {
		if (_SC_CFStringIsValidNetBIOSName(name)) {
			CFMutableDictionaryRef	newDict;

			SCLog(TRUE, LOG_INFO, CFSTR("NetBIOS name (multicast DNS) = %@"), name);
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
			CFDictionarySetValue(newDict, kSCPropNetSMBNetBIOSName, name);
			CFRelease(dict);
			dict = newDict;
			CFRelease(name);
			goto set;
		}
		CFRelease(name);
	}

	// use "default" name
	name = copy_default_name();
	if (name != NULL) {
		CFMutableDictionaryRef	newDict;

		SCLog(TRUE, LOG_INFO, CFSTR("NetBIOS name (default) = %@"), name);
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		CFDictionarySetValue(newDict, kSCPropNetSMBNetBIOSName, name);
		CFRelease(dict);
		dict = newDict;
		CFRelease(name);
	}

    set :

	// update SMB configuration
	smb_set_configuration(store, dict);

	if (host != NULL)	free(host);
	if (dict != NULL)	CFRelease(dict);
	if (serv != NULL)	free(serv);
	dnsActive = FALSE;
	return;
}


static CFStringRef
replyMPCopyDescription(const void *info)
{
	SCDynamicStoreRef	store	= (SCDynamicStoreRef)info;

	return CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("<getnameinfo_async_start reply MP> {store = %p}"),
					store);
}


static void
getnameinfo_async_handleCFReply(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	mach_port_t	mp	= MACH_PORT_NULL;
	int32_t		status;

	if (port != dnsPort) {
		// we've received a callback on the async DNS port but since the
		// associated CFMachPort doesn't match than the request must have
		// already been cancelled.
		SCLog(TRUE, LOG_ERR, CFSTR("getnameinfo_async_handleCFReply(): port != dnsPort"));
		return;
	}

	mp = CFMachPortGetPort(port);
	CFMachPortInvalidate(dnsPort);
	CFRelease(dnsPort);
	dnsPort = NULL;

	status = getnameinfo_async_handle_reply(msg);
	if ((status == 0) && dnsActive && (mp != MACH_PORT_NULL)) {
		CFMachPortContext	context	= { 0
						  , (void *)store
						  , CFRetain
						  , CFRelease
						  , replyMPCopyDescription
						  };
		CFRunLoopSourceRef	rls;

		// if request has been re-queued
		dnsPort = _SC_CFMachPortCreateWithPort("IPMonitor/smb-configuration/re-queue",
						       mp,
						       getnameinfo_async_handleCFReply,
						       &context);
		rls = CFMachPortCreateRunLoopSource(NULL, dnsPort, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
		CFRelease(rls);
	}

	return;
}


static Boolean
start_dns_query(SCDynamicStoreRef store, CFStringRef address)
{
	union {
		struct sockaddr		sa;
		struct sockaddr_in	sin;
		struct sockaddr_in6	sin6;
	} addr;
	char				buf[64];
	SCNetworkReachabilityFlags	flags;
	Boolean				haveDNS;
	Boolean				ok	= FALSE;

	if (_SC_cfstring_to_cstring(address, buf, sizeof(buf), kCFStringEncodingASCII) == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("could not convert [primary] address"));
		return FALSE;
	}

	if (_SC_string_to_sockaddr(buf, AF_UNSPEC, (void *)&addr, sizeof(addr)) == NULL) {
		/* if not an IP[v6] address */
		SCLog(TRUE, LOG_ERR, CFSTR("could not parse [primary] address"));
		return FALSE;
	}

	ok = _SC_checkResolverReachabilityByAddress(&store, &flags, &haveDNS, &addr.sa);
	if (ok) {
		if (!(flags & kSCNetworkReachabilityFlagsReachable) ||
		    (flags & kSCNetworkReachabilityFlagsConnectionRequired)) {
			// if not reachable *OR* connection required
			ok = FALSE;
		}
	}

	if (ok) {
		CFMachPortContext	context	= { 0
						  , (void *)store
						  , CFRetain
						  , CFRelease
						  , replyMPCopyDescription
						  };
		int32_t			error;
		mach_port_t		mp;
		CFRunLoopSourceRef	rls;

		(void) gettimeofday(&dnsQueryStart, NULL);

		error = getnameinfo_async_start(&mp,
						&addr.sa,
						addr.sa.sa_len,
						NI_NAMEREQD,	// flags
						reverseDNSComplete,
						(void *)store);
		if (error != 0) {
			ok = FALSE;
			goto done;
		}

		dnsActive = TRUE;
		dnsPort = _SC_CFMachPortCreateWithPort("IPMonitor/smb-configuration",
						       mp,
						       getnameinfo_async_handleCFReply,
						       &context);
		rls = CFMachPortCreateRunLoopSource(NULL, dnsPort, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
		CFRelease(rls);
	}

    done :

	return ok;
}


static void
smb_update_configuration(__unused CFRunLoopTimerRef _timer, void *info)
{
	CFStringRef		address		= NULL;
	CFDictionaryRef		dict;
	CFStringRef		name;
	CFStringRef		serviceID	= NULL;
	SCDynamicStoreRef	store		= (SCDynamicStoreRef)info;

	// get network configuration
	dict = smb_copy_global_configuration(store);

	// use NetBIOS name from network configuration (if available)
	name = CFDictionaryGetValue(dict, kSCPropNetSMBNetBIOSName);
	if ((name != NULL) && _SC_CFStringIsValidNetBIOSName(name)) {
		SCLog(TRUE, LOG_INFO, CFSTR("NetBIOS name (network configuration) = %@"), name);
		goto set;
	}

	// get primary service ID
	serviceID = copy_primary_service(store);
	if (serviceID == NULL) {
		// if no primary service
		goto mDNS;
	}

	// get DNS name associated with primary IP, if available
	address = copy_primary_ip(store, serviceID);
	if (address != NULL) {
		Boolean	ok;

		// start reverse DNS query using primary IP address
		ok = start_dns_query(store, address);
		if (ok) {
			// if query started
			goto done;
		}
	}

    mDNS :

	// get local (multicast DNS) name, if available

	name = SCDynamicStoreCopyLocalHostName(store);
	if (name != NULL) {
		if (_SC_CFStringIsValidNetBIOSName(name)) {
			CFMutableDictionaryRef	newDict;

			SCLog(TRUE, LOG_INFO, CFSTR("NetBIOS name (multicast DNS) = %@"), name);
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

		SCLog(TRUE, LOG_INFO, CFSTR("NetBIOS name (default) = %@"), name);
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		CFDictionarySetValue(newDict, kSCPropNetSMBNetBIOSName, name);
		CFRelease(dict);
		dict = newDict;
		CFRelease(name);
	}

    set :

	// update SMB configuration
	smb_set_configuration(store, dict);

    done :

	if (address != NULL)	CFRelease(address);
	if (dict != NULL)	CFRelease(dict);
	if (serviceID != NULL)	CFRelease(serviceID);

	if (timer != NULL) {
		CFRunLoopTimerInvalidate(timer);
		CFRelease(timer);
		timer = NULL;
	}

	return;
}


static void
configuration_changed(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
	CFRunLoopTimerContext	context	= { 0, (void *)store, CFRetain, CFRelease, NULL };
	CFAbsoluteTime		time_boot;
	CFAbsoluteTime		time_now ;

	// if active, cancel any in-progress attempt to resolve the primary IP address
	if (dnsPort != NULL) {
		mach_port_t	mp	= CFMachPortGetPort(dnsPort);

		/* cancel the outstanding DNS query */
		CFMachPortInvalidate(dnsPort);
		CFRelease(dnsPort);
		dnsPort = NULL;

		getnameinfo_async_cancel(mp);
	}

	// if active, cancel any queued configuration change
	if (timer != NULL) {
		CFRunLoopTimerInvalidate(timer);
		CFRelease(timer);
		timer = NULL;
	}

	// queue configuration change
	time_boot = boottime() + SMB_STARTUP_DELAY;
	time_now  = CFAbsoluteTimeGetCurrent() + SMB_DEBOUNCE_DELAY;

	timer = CFRunLoopTimerCreate(NULL,
				     time_now > time_boot ? time_now : time_boot,
				     0,
				     0,
				     0,
				     smb_update_configuration,
				     &context);
	CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);

	return;
}


__private_extern__
void
load_smb_configuration(Boolean verbose)
{
	CFStringRef		key;
	CFMutableArrayRef	keys		= NULL;
	CFMutableArrayRef	patterns	= NULL;

	if (verbose) {
		_verbose = TRUE;
	}

	/* initialize a few globals */

	store = SCDynamicStoreCreate(NULL, CFSTR("smb-configuration"), configuration_changed, NULL);
	if (store == NULL) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreCreate() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	/* establish notification keys and patterns */

	keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	/* ...watch for primary service / interface changes */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetIPv4);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/* ...watch for DNS configuration changes */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetDNS);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/* ...watch for SMB configuration changes */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetSMB);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/* ...watch for ComputerName changes */
	key = SCDynamicStoreKeyCreateComputerName(NULL);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/* ...watch for local (multicast DNS) hostname changes */
	key = SCDynamicStoreKeyCreateHostNames(NULL);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/* register the keys/patterns */
	if (!SCDynamicStoreSetNotificationKeys(store, keys, patterns)) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreSetNotificationKeys() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!rls) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreCreateRunLoopSource() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);

	CFRelease(keys);
	CFRelease(patterns);
	return;

    error :

	if (keys != NULL)	CFRelease(keys);
	if (patterns != NULL)	CFRelease(patterns);
	if (store != NULL)	CFRelease(store);
	return;
}


#ifdef	MAIN
int
main(int argc, char **argv)
{

#ifdef	DEBUG
	CFStringRef		address;
	CFStringRef		name;
	CFStringRef		serviceID;
	SCDynamicStoreRef	store;

	_sc_log = FALSE;
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
		(void) start_dns_query(store, address);
		CFRelease(address);
	}

    done :

	smb_update_configuration(NULL, (void *)store);

	CFRelease(store);

	CFRunLoopRun();

#else	/* DEBUG */

	_sc_log     = FALSE;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	load_smb_configuration((argc > 1) ? TRUE : FALSE);
	CFRunLoopRun();
	/* not reached */

#endif	/* DEBUG */

	exit(0);
	return 0;
}
#endif	/* MAIN */
