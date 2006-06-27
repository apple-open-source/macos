/*
 * Copyright (c) 2004,2005 Apple Computer, Inc. All rights reserved.
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
 * May 27, 2004		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <sys/ioctl.h>
#include <net/if.h>


__private_extern__ CFDictionaryRef
__getPrefsConfiguration(SCPreferencesRef prefs, CFStringRef path)
{
	CFDictionaryRef config;
	CFIndex		n;

	config = SCPreferencesPathGetValue(prefs, path);

	n = isA_CFDictionary(config) ? CFDictionaryGetCount(config) : 0;
	switch (n) {
		case 0 :
			// ignore empty configuration entities
			config = NULL;
			break;
		case 1 :
			if (CFDictionaryContainsKey(config, kSCResvInactive)) {
				// ignore [effectively] empty configuration entities
				config = NULL;
			}
			break;
		default :
			break;
	}

	return config;
}


__private_extern__ Boolean
__setPrefsConfiguration(SCPreferencesRef	prefs,
			CFStringRef		path,
			CFDictionaryRef		config,
			Boolean			keepInactive)
{
	CFMutableDictionaryRef  newConfig;
	Boolean			ok;

	if (!isA_CFDictionary(config)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	newConfig = CFDictionaryCreateMutableCopy(NULL, 0, config);

	if (keepInactive) {
		CFDictionaryRef	curConfig;

		// preserve enabled/disabled state
		curConfig = SCPreferencesPathGetValue(prefs, path);
		if (isA_CFDictionary(curConfig) && CFDictionaryContainsKey(curConfig, kSCResvInactive)) {
			// if currently disabled
			CFDictionarySetValue(newConfig, kSCResvInactive, kCFBooleanTrue);
		} else {
			// if currently enabled
			CFDictionaryRemoveValue(newConfig, kSCResvInactive);
		}
	}

	// set new configuration
	ok = SCPreferencesPathSetValue(prefs, path, newConfig);

	CFRelease(newConfig);
	return ok;
}


__private_extern__ Boolean
__getPrefsEnabled(SCPreferencesRef prefs, CFStringRef path)
{
	CFDictionaryRef config;

	config = SCPreferencesPathGetValue(prefs, path);
	if (isA_CFDictionary(config) && CFDictionaryContainsKey(config, kSCResvInactive)) {
		return FALSE;
	}

	return TRUE;
}


__private_extern__ Boolean
__setPrefsEnabled(SCPreferencesRef      prefs,
		  CFStringRef		path,
		  Boolean		enabled)
{
	CFDictionaryRef		curConfig       = NULL;
	CFMutableDictionaryRef  newConfig       = NULL;
	Boolean			ok		= FALSE;

	// preserve current configuration
	curConfig = SCPreferencesPathGetValue(prefs, path);
	if (curConfig != NULL) {
		if (!isA_CFDictionary(curConfig)) {
			_SCErrorSet(kSCStatusFailed);
			return FALSE;
		}
		newConfig = CFDictionaryCreateMutableCopy(NULL, 0, curConfig);
	} else {
		newConfig = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}

	if (enabled) {
		// enable
		CFDictionaryRemoveValue(newConfig, kSCResvInactive);
	} else {
		// disable
		CFDictionarySetValue(newConfig, kSCResvInactive, kCFBooleanTrue);
	}

	// update configuration
	if (CFDictionaryGetCount(newConfig) == 0) {
		CFRelease(newConfig);
		newConfig = NULL;
	}

	if (newConfig == NULL) {
		ok = SCPreferencesPathRemoveValue(prefs, path);
	} else {
		ok = SCPreferencesPathSetValue(prefs, path, newConfig);
	}

	if (newConfig != NULL)  CFRelease(newConfig);
	return ok;
}


#define SYSTEMCONFIGURATION_BUNDLE_ID   CFSTR("com.apple.SystemConfiguration")
#define SYSTEMCONFIGURATION_FRAMEWORK   "SystemConfiguration.framework"


static CFDictionaryRef
__copyTemplates()
{
	CFBundleRef     bundle;
	Boolean		ok;
	CFDictionaryRef templates;
	CFURLRef	url;
	CFStringRef     xmlError	= NULL;
	CFDataRef       xmlTemplates    = NULL;

	bundle = CFBundleGetBundleWithIdentifier(SYSTEMCONFIGURATION_BUNDLE_ID);
	if (bundle == NULL) {
		return NULL;
	}

	url = CFBundleCopyResourceURL(bundle, CFSTR("NetworkConfiguration"), CFSTR("plist"), NULL);
	if (url == NULL) {
		return NULL;
	}

	ok = CFURLCreateDataAndPropertiesFromResource(NULL, url, &xmlTemplates, NULL, NULL, NULL);
	CFRelease(url);
	if (!ok || (xmlTemplates == NULL)) {
		return NULL;
	}

	// convert the XML data into a property list
	templates = CFPropertyListCreateFromXMLData(NULL, xmlTemplates, kCFPropertyListImmutable, &xmlError);
	CFRelease(xmlTemplates);
	if (templates == NULL) {
		if (xmlError != NULL) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("could not load SCNetworkConfiguration templates: %@"), xmlError);
			CFRelease(xmlError);
		}
		return NULL;
	}

	if (!isA_CFDictionary(templates)) {
		CFRelease(templates);
		return NULL;
	}

	return templates;
}


__private_extern__ CFDictionaryRef
__copyInterfaceTemplate(CFStringRef      interfaceType,
			CFStringRef      childInterfaceType)
{
	CFDictionaryRef interface       = NULL;
	CFDictionaryRef interfaces;
	CFDictionaryRef templates;

	templates = __copyTemplates();
	if (templates == NULL) {
		return NULL;
	}

	interfaces = CFDictionaryGetValue(templates, CFSTR("Interface"));
	if (!isA_CFDictionary(interfaces)) {
		CFRelease(templates);
		return NULL;
	}

	if (childInterfaceType == NULL) {
		interface = CFDictionaryGetValue(interfaces, interfaceType);
	} else {
		CFStringRef     expandedType;

		expandedType = CFStringCreateWithFormat(NULL,
							NULL,
							CFSTR("%@-%@"),
							interfaceType,
							childInterfaceType);
		interface = CFDictionaryGetValue(interfaces, expandedType);
		CFRelease(expandedType);
	}

	if (isA_CFDictionary(interface) && (CFDictionaryGetCount(interface) > 0)) {
		CFRetain(interface);
	} else {
		interface = NULL;
	}

	CFRelease(templates);

	return interface;
}


__private_extern__ CFDictionaryRef
__copyProtocolTemplate(CFStringRef      interfaceType,
		       CFStringRef      childInterfaceType,
		       CFStringRef      protocolType)
{
	CFDictionaryRef interface       = NULL;
	CFDictionaryRef protocol	= NULL;
	CFDictionaryRef protocols;
	CFDictionaryRef templates;

	templates = __copyTemplates();
	if (templates == NULL) {
		return NULL;
	}

	protocols = CFDictionaryGetValue(templates, CFSTR("Protocol"));
	if (!isA_CFDictionary(protocols)) {
		CFRelease(templates);
		return NULL;
	}

	if (childInterfaceType == NULL) {
		interface = CFDictionaryGetValue(protocols, interfaceType);
	} else {
		CFStringRef     expandedType;

		expandedType = CFStringCreateWithFormat(NULL,
							NULL,
							CFSTR("%@-%@"),
							interfaceType,
							childInterfaceType);
		interface = CFDictionaryGetValue(protocols, expandedType);
		CFRelease(expandedType);
	}

	if (isA_CFDictionary(interface)) {
		protocol = CFDictionaryGetValue(interface, protocolType);
		if (isA_CFDictionary(protocol) && (CFDictionaryGetCount(protocol) > 0)) {
			CFRetain(protocol);
		} else {
			protocol = NULL;
		}
	}

	CFRelease(templates);

	return protocol;
}


__private_extern__ Boolean
__createInterface(int s, CFStringRef interface)
{
	struct ifreq	ifr;

	bzero(&ifr, sizeof(ifr));
	(void) _SC_cfstring_to_cstring(interface,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);

	if (ioctl(s, SIOCIFCREATE, &ifr) == -1) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("could not create interface \"%@\": %s"),
		      interface,
		      strerror(errno));
		return FALSE;
	}

	return TRUE;
}


__private_extern__ Boolean
__destroyInterface(int s, CFStringRef interface)
{
	struct ifreq	ifr;

	bzero(&ifr, sizeof(ifr));
	(void) _SC_cfstring_to_cstring(interface,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);

	if (ioctl(s, SIOCIFDESTROY, &ifr) == -1) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("could not destroy interface \"%@\": %s"),
		      interface,
		      strerror(errno));
		return FALSE;
	}

	return TRUE;
}


__private_extern__ Boolean
__markInterfaceUp(int s, CFStringRef interface)
{
	struct ifreq	ifr;

	bzero(&ifr, sizeof(ifr));
	(void) _SC_cfstring_to_cstring(interface,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) == -1) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("could not get flags for interface \"%@\": %s"),
		      interface,
		      strerror(errno));
		return FALSE;
	}

	ifr.ifr_flags |= IFF_UP;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) == -1) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("could not set flags for interface \"%@\": %s"),
		      interface,
		      strerror(errno));
		return FALSE;
	}

	return TRUE;
}
