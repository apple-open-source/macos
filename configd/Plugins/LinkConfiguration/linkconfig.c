/*
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
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
//#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <net/if.h>
#include <net/if_media.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/LinkConfiguration.h>
#include <SystemConfiguration/SCDPlugin.h>		// for _SCDPluginExecCommand


static CFMutableDictionaryRef	baseSettings	= NULL;
static SCDynamicStoreRef	store		= NULL;
static CFRunLoopSourceRef	rls		= NULL;

static Boolean			_verbose	= FALSE;


/* in SystemConfiguration/LinkConfiguration.c */
int
__createMediaOptions(CFStringRef interface, CFDictionaryRef media_options);


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


__private_extern__
Boolean
_NetworkInterfaceSetMediaOptions(CFStringRef		interface,
				 CFDictionaryRef	options)
{
	CFArrayRef		available	= NULL;
	CFDictionaryRef		current		= NULL;
	struct ifmediareq	ifm;
	struct ifreq		ifr;
	Boolean			ok		= FALSE;
	int			newOptions;
	CFDictionaryRef		requested;
	int			sock		= -1;

	/* get current & available options */
	if (!NetworkInterfaceCopyMediaOptions(interface, &current, NULL, &available, FALSE)) {
		/* could not get current media options */
		return FALSE;
	}

	/* extract just the dictionary key/value pairs of interest */
	requested = __copyMediaOptions(options);
	if (requested == NULL) {
		CFDictionaryRef	baseOptions;

		/* get base options */
		baseOptions = CFDictionaryGetValue(baseSettings, interface);
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
		SCLog(TRUE, LOG_DEBUG, CFSTR("requested media settings unavailable"));
		goto done;
	}

	newOptions = __createMediaOptions(interface, requested);
	if (newOptions == -1) {
		/* since we have just validated, this should never happen */
		goto done;
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("socket() failed: %s"), strerror(errno));
		goto done;
	}

	bzero((char *)&ifm, sizeof(ifm));
	(void)_SC_cfstring_to_cstring(interface, ifm.ifm_name, sizeof(ifm.ifm_name), kCFStringEncodingASCII);

	if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifm) < 0) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("ioctl(SIOCGIFMEDIA) failed: %s"), strerror(errno));
		goto done;
	}

	bzero((char *)&ifr, sizeof(ifr));
	bcopy(ifm.ifm_name, ifr.ifr_name, sizeof(ifr.ifr_name));
	ifr.ifr_media =  ifm.ifm_current & ~(IFM_NMASK|IFM_TMASK|IFM_OMASK|IFM_GMASK);
	ifr.ifr_media |= newOptions;

//SCLog(TRUE, LOG_INFO, CFSTR("old media settings: 0x%8.8x (0x%8.8x)"), ifm.ifm_current, ifm.ifm_active);
//SCLog(TRUE, LOG_INFO, CFSTR("new media settings: 0x%8.8x"), ifr.ifr_media);

	if (ioctl(sock, SIOCSIFMEDIA, (caddr_t)&ifr) < 0) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("ioctl(SIOCSIFMEDIA) failed: %s"), strerror(errno));
		goto done;
	}

	ok = TRUE;

    done :

	if (available)	CFRelease(available);
	if (current)	CFRelease(current);
	if (requested)	CFRelease(requested);
	if (sock >= 0)	(void)close(sock);

	return ok;
}


#ifndef	USE_SIOCSIFMTU
static void
ifconfig_exit(pid_t pid, int status, struct rusage *rusage, void *context)
{
	char	*if_name	= (char *)context;

	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) != 0) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("ifconfig %s failed, exit status = %d"),
			      if_name,
			      WEXITSTATUS(status));
		}
	} else if (WIFSIGNALED(status)) {
		SCLog(TRUE, LOG_DEBUG,
		      CFSTR("ifconfig %s: terminated w/signal = %d"),
		      if_name,
		      WTERMSIG(status));
	} else {
		SCLog(TRUE, LOG_DEBUG,
		      CFSTR("ifconfig %s: exit status = %d"),
		      if_name,
		      status);
	}

	CFAllocatorDeallocate(NULL, if_name);
	return;
}
#endif	/* !USE_SIOCSIFMTU */


__private_extern__
Boolean
_NetworkInterfaceSetMTU(CFStringRef	interface,
			CFDictionaryRef	options)
{
	int		mtu_cur		= -1;
	int		mtu_max		= -1;
	int		mtu_min		= -1;
	int		requested;
	CFNumberRef	val;

	if (!NetworkInterfaceCopyMTU(interface, &mtu_cur, &mtu_min, &mtu_max)) {
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
		baseOptions = CFDictionaryGetValue(baseSettings, interface);
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

#ifdef	USE_SIOCSIFMTU
{
	struct ifreq	ifr;
	int		ret;
	int		sock;

	bzero((char *)&ifr, sizeof(ifr));
	(void)_SC_cfstring_to_cstring(interface, ifr.ifr_name, sizeof(ifr.ifr_name), kCFStringEncodingASCII);
	ifr.ifr_mtu = requested;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("socket() failed: %s"), strerror(errno));
		return FALSE;
	}

	ret = ioctl(sock, SIOCSIFMTU, (caddr_t)&ifr);
	(void)close(sock);
	if (ret == -1) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("ioctl(SIOCSIFMTU) failed: %s"), strerror(errno));
		return FALSE;
	}
}
#else	/* !USE_SIOCSIFMTU */
{
	char	*ifconfig_argv[] = { "ifconfig", NULL, "mtu", NULL, NULL };
	pid_t	pid;

	ifconfig_argv[1] = _SC_cfstring_to_cstring(interface, NULL, 0, kCFStringEncodingASCII);
	(void)asprintf(&ifconfig_argv[3], "%d", requested);

	pid = _SCDPluginExecCommand(ifconfig_exit,	// callout,
				    ifconfig_argv[1],	// context
				    0,			// uid
				    0,			// gid
				    "/sbin/ifconfig",	// path
				    ifconfig_argv	// argv
				   );

//	CFAllocatorDeallocate(NULL, ifconfig_argv[1]);	// released in ifconfig_exit()
	free(ifconfig_argv[3]);

	if (pid <= 0) {
		return FALSE;
	}
}
#endif	/* !USE_SIOCSIFMTU */

	return TRUE;
}


/*
 * Function: parse_component
 * Purpose:
 *   Given a string 'key' and a string prefix 'prefix',
 *   return the next component in the slash '/' separated
 *   key.
 *
 * Examples:
 * 1. key = "a/b/c" prefix = "a/"
 *    returns "b"
 * 2. key = "a/b/c" prefix = "a/b/"
 *    returns "c"
 */
static CFStringRef
parse_component(CFStringRef key, CFStringRef prefix)
{
	CFMutableStringRef	comp;
	CFRange			range;

	if (CFStringHasPrefix(key, prefix) == FALSE) {
		return NULL;
	}
	comp = CFStringCreateMutableCopy(NULL, 0, key);
	CFStringDelete(comp, CFRangeMake(0, CFStringGetLength(prefix)));
	range = CFStringFind(comp, CFSTR("/"), 0);
	if (range.location == kCFNotFound) {
		return comp;
	}
	range.length = CFStringGetLength(comp) - range.location;
	CFStringDelete(comp, range);
	return comp;
}


static void
updateLink(CFStringRef ifKey, CFDictionaryRef options)
{
	CFStringRef		interface	= NULL;
	static CFStringRef	prefix		= NULL;

	if (!prefix) {
		prefix = SCDynamicStoreKeyCreate(NULL,
						 CFSTR("%@/%@/%@/"),
						 kSCDynamicStoreDomainSetup,
						 kSCCompNetwork,
						 kSCCompInterface);
	}

	interface = parse_component(ifKey, prefix);
	if (!interface) {
		goto done;
	}

	if (options) {
		if (!CFDictionaryContainsKey(baseSettings, interface)) {
			CFDictionaryRef		cur_media	= NULL;
			CFMutableDictionaryRef	new_media	= NULL;
			int			cur_mtu		= -1;

			/* preserve current media options */
			if (NetworkInterfaceCopyMediaOptions(interface, &cur_media, NULL, NULL, FALSE)) {
				if (cur_media != NULL) {
					new_media = CFDictionaryCreateMutableCopy(NULL, 0, cur_media);
					CFRelease(cur_media);
				}
			}

			/* preserve current MTU */
			if (NetworkInterfaceCopyMTU(interface, &cur_mtu, NULL, NULL)) {
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

			if (new_media != NULL) {
				CFDictionarySetValue(baseSettings, interface, new_media);
				CFRelease(new_media);
			}
		}

		/* establish new settings */
		(void)_NetworkInterfaceSetMediaOptions(interface, options);
		(void)_NetworkInterfaceSetMTU         (interface, options);
	} else {
		/* no requested settings */
		options = CFDictionaryGetValue(baseSettings, interface);
		if (options) {
			/* restore original settings */
			(void)_NetworkInterfaceSetMediaOptions(interface, options);
			(void)_NetworkInterfaceSetMTU         (interface, options);
			CFDictionaryRemoveValue(baseSettings, interface);
		}
	}

    done :

	if (interface)	CFRelease(interface);
	return;
}


static void
linkConfigChangedCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void *arg)
{
	CFIndex			i;
	CFIndex			n;
	CFDictionaryRef		linkInfo;

	linkInfo = SCDynamicStoreCopyMultiple(store, changedKeys, NULL);

	n = CFArrayGetCount(changedKeys);
	for (i = 0; i < n; i++) {
		CFStringRef	key;
		CFDictionaryRef	link;

		key  = CFArrayGetValueAtIndex(changedKeys, i);
		link = CFDictionaryGetValue(linkInfo, key);
		updateLink(key, link);
	}

	CFRelease(linkInfo);

	return;
}


__private_extern__
void
load_LinkConfiguration(CFBundleRef bundle, Boolean bundleVerbose)
{
	CFStringRef		key;
	CFMutableArrayRef	patterns	= NULL;

	if (bundleVerbose) {
		_verbose = TRUE;
	}

	SCLog(_verbose, LOG_DEBUG, CFSTR("load() called"));
	SCLog(_verbose, LOG_DEBUG, CFSTR("  bundle ID = %@"), CFBundleGetIdentifier(bundle));

	/* initialize a few globals */

	baseSettings = CFDictionaryCreateMutable(NULL,
						 0,
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);

	/* open a "configd" store to allow cache updates */
	store = SCDynamicStoreCreate(NULL,
				     CFSTR("Link Configuraton plug-in"),
				     linkConfigChangedCallback,
				     NULL);
	if (!store) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreCreate() failed: %s"), SCErrorString(SCError()));
		goto error;
	}

	/* establish notification keys and patterns */

	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

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

	/* ...watch for (per-interface) FireWire configuration changes */
	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainSetup,
							    kSCCompAnyRegex,
							    kSCEntNetFireWire);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);

	/* register the keys/patterns */
	if (!SCDynamicStoreSetNotificationKeys(store, NULL, patterns)) {
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

	CFRelease(patterns);
	return;

    error :

	if (baseSettings)	CFRelease(baseSettings);
	if (store) 		CFRelease(store);
	if (patterns)		CFRelease(patterns);
	return;
}


#ifdef	MAIN
int
main(int argc, char **argv)
{
	_sc_log     = FALSE;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	load_LinkConfiguration(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif
