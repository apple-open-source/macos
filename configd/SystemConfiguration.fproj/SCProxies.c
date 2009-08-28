/*
 * Copyright (c) 2000-2004, 2006-2008 Apple Inc. All rights reserved.
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
 * May 18, 2001			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <netdb.h>




CFStringRef
SCDynamicStoreKeyCreateProxies(CFAllocatorRef allocator)
{
	return SCDynamicStoreKeyCreateNetworkGlobalEntity(allocator,
							  kSCDynamicStoreDomainState,
							  kSCEntNetProxies);
}


static void
validate_proxy_content(CFMutableDictionaryRef	proxies,
		       CFStringRef		proxy_enable,
		       CFStringRef		proxy_host,
		       CFStringRef		proxy_port,
		       const char *		proxy_service,
		       int			proxy_defaultport)
{
	int		enabled	= 0;
	CFNumberRef	num;

	num = CFDictionaryGetValue(proxies, proxy_enable);
	if (num != NULL) {
		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &enabled)) {
			// if we don't like the enabled key/value
			goto disable;
		}
	}

	if (proxy_host != NULL) {
		CFStringRef	host;

		host = CFDictionaryGetValue(proxies, proxy_host);
		if (((enabled == 0) && (host != NULL)) ||
		    ((enabled != 0) && !isA_CFString(host))) {
			// pass only valid proxy hosts and only when enabled
			goto disable;
		}
	}

	if (proxy_port != NULL) {
		CFNumberRef	port;

		port = CFDictionaryGetValue(proxies, proxy_port);
		if (((enabled == 0) && (port != NULL)) ||
		    ((enabled != 0) && (port != NULL) && !isA_CFNumber(port))) {
			// pass only provided/valid proxy ports and only when enabled
			goto disable;
		}

		if ((enabled != 0) && (port == NULL)) {
			struct servent	*service;
			int		s_port;

			service = getservbyname(proxy_service, "tcp");
			if (service != NULL) {
				s_port = ntohs(service->s_port);
			} else {
				s_port = proxy_defaultport;
			}
			num = CFNumberCreate(NULL, kCFNumberIntType, &s_port);
			CFDictionarySetValue(proxies, proxy_port, num);
			CFRelease(num);
		}
	}

	return;

    disable :

	enabled = 0;
	num = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
	CFDictionarySetValue(proxies, proxy_enable, num);
	CFRelease(num);
	if (proxy_host != NULL) {
		CFDictionaryRemoveValue(proxies, proxy_host);
	}
	if (proxy_port != NULL) {
		CFDictionaryRemoveValue(proxies, proxy_port);
	}

	return;
}


CFDictionaryRef
SCDynamicStoreCopyProxies(SCDynamicStoreRef store)
{
	CFArrayRef		array;
	CFStringRef		key;
	CFMutableDictionaryRef	newProxies	= NULL;
	CFNumberRef		num;
	CFDictionaryRef		proxies;
	Boolean			tempSession	= FALSE;


	/* copy proxy information from dynamic store */

	if (store == NULL) {
		store = SCDynamicStoreCreate(NULL,
					     CFSTR("SCDynamicStoreCopyProxies"),
					     NULL,
					     NULL);
		if (store == NULL) {
			return NULL;
		}
		tempSession = TRUE;
	}

	key = SCDynamicStoreKeyCreateProxies(NULL);
	proxies = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);

    validate :

	if (proxies != NULL) {
		if (isA_CFDictionary(proxies)) {
			newProxies = CFDictionaryCreateMutableCopy(NULL, 0, proxies);
		}
		CFRelease(proxies);
	}

	if (newProxies == NULL) {
		newProxies = CFDictionaryCreateMutable(NULL,
						       0,
						       &kCFTypeDictionaryKeyCallBacks,
						       &kCFTypeDictionaryValueCallBacks);
	}

	/* validate [and augment] proxy content */

	validate_proxy_content(newProxies,
			       kSCPropNetProxiesFTPEnable,
			       kSCPropNetProxiesFTPProxy,
			       kSCPropNetProxiesFTPPort,
			       "ftp",
			       21);
	validate_proxy_content(newProxies,
			       kSCPropNetProxiesGopherEnable,
			       kSCPropNetProxiesGopherProxy,
			       kSCPropNetProxiesGopherPort,
			       "gopher",
			       70);
	validate_proxy_content(newProxies,
			       kSCPropNetProxiesHTTPEnable,
			       kSCPropNetProxiesHTTPProxy,
			       kSCPropNetProxiesHTTPPort,
			       "http",
			       80);
	validate_proxy_content(newProxies,
			       kSCPropNetProxiesHTTPSEnable,
			       kSCPropNetProxiesHTTPSProxy,
			       kSCPropNetProxiesHTTPSPort,
			       "https",
			       443);
	validate_proxy_content(newProxies,
			       kSCPropNetProxiesRTSPEnable,
			       kSCPropNetProxiesRTSPProxy,
			       kSCPropNetProxiesRTSPPort,
			       "rtsp",
			       554);
	validate_proxy_content(newProxies,
			       kSCPropNetProxiesSOCKSEnable,
			       kSCPropNetProxiesSOCKSProxy,
			       kSCPropNetProxiesSOCKSPort,
			       "socks",
			       1080);
	validate_proxy_content(newProxies,
			       kSCPropNetProxiesProxyAutoConfigEnable,
			       kSCPropNetProxiesProxyAutoConfigURLString,
			       NULL,
			       NULL,
			       0);
	validate_proxy_content(newProxies,
			       kSCPropNetProxiesProxyAutoDiscoveryEnable,
			       NULL,
			       NULL,
			       NULL,
			       0);

	// validate FTP passive setting
	num = CFDictionaryGetValue(newProxies, kSCPropNetProxiesFTPPassive);
	if (num != NULL) {
		int	enabled	= 0;

		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &enabled)) {
			// if we don't like the enabled key/value
			enabled = 1;
			num = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
			CFDictionarySetValue(newProxies,
					     kSCPropNetProxiesFTPPassive,
					     num);
			CFRelease(num);
		}
	}

	// validate proxy exception list
	array = CFDictionaryGetValue(newProxies, kSCPropNetProxiesExceptionsList);
	if (array != NULL) {
		CFIndex		i;
		CFIndex		n;

		n = isA_CFArray(array) ? CFArrayGetCount(array) : 0;
		for (i = 0; i < n; i++) {
			CFStringRef	str;

			str = CFArrayGetValueAtIndex(array, i);
			if (!isA_CFString(str)) {
				// if we don't like the array contents
				n = 0;
				break;
			}
		}

		if (n == 0) {
			CFDictionaryRemoveValue(newProxies, kSCPropNetProxiesExceptionsList);
		}
	}

	// validate exclude simple hostnames setting
	num = CFDictionaryGetValue(newProxies, kSCPropNetProxiesExcludeSimpleHostnames);
	if (num != NULL) {
		int	enabled;

		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &enabled)) {
			// if we don't like the enabled key/value
			enabled = 0;
			num = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
			CFDictionarySetValue(newProxies,
					     kSCPropNetProxiesExcludeSimpleHostnames,
					     num);
			CFRelease(num);
		}
	}


	proxies = CFDictionaryCreateCopy(NULL, newProxies);
	CFRelease(newProxies);

	if (tempSession)	CFRelease(store);
	return proxies;
}
