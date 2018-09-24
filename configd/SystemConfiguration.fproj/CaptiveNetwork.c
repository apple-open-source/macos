/*
 * Copyright (c) 2009, 2010, 2012, 2013, 2015, 2018 Apple Inc. All rights reserved.
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


#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <mach-o/dyld_priv.h>
#include <sys/codesign.h>

#include <SystemConfiguration/CaptiveNetwork.h>
#include <SystemConfiguration/SCPrivate.h>


#pragma mark -
#pragma mark CaptiveNetwork.framework APIs (exported through the SystemConfiguration.framework)


const CFStringRef kCNNetworkInfoKeySSIDData    = CFSTR("SSIDDATA");
const CFStringRef kCNNetworkInfoKeySSID        = CFSTR("SSID");
const CFStringRef kCNNetworkInfoKeyBSSID       = CFSTR("BSSID");


static void *
__loadCaptiveNetwork(void) {
	static void		*image	= NULL;
	static dispatch_once_t	once;

	dispatch_once(&once, ^{
		image = _SC_dlopen("/System/Library/PrivateFrameworks/CaptiveNetwork.framework/CaptiveNetwork");
	});

	return image;
}


Boolean
CNSetSupportedSSIDs(CFArrayRef ssidArray)
{
	static typeof (CNSetSupportedSSIDs) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadCaptiveNetwork();
		if (image) dyfunc = dlsym(image, "__CNSetSupportedSSIDs");
	}
	return dyfunc ? dyfunc(ssidArray) : FALSE;
}


Boolean
CNMarkPortalOnline(CFStringRef interfaceName)
{
	static typeof (CNMarkPortalOnline) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadCaptiveNetwork();
		if (image) dyfunc = dlsym(image, "__CNMarkPortalOnline");
	}
	return dyfunc ? dyfunc(interfaceName) : FALSE;
}


Boolean
CNMarkPortalOffline(CFStringRef interfaceName)
{
	static typeof (CNMarkPortalOffline) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadCaptiveNetwork();
		if (image) dyfunc = dlsym(image, "__CNMarkPortalOffline");
	}
	return dyfunc ? dyfunc(interfaceName) : FALSE;
}

CFArrayRef
CNCopySupportedInterfaces(void)
{
	static typeof (CNCopySupportedInterfaces) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadCaptiveNetwork();
		if (image) dyfunc = dlsym(image, "__CNCopySupportedInterfaces");
	}
	return dyfunc ? dyfunc() : NULL;
}

#if	TARGET_OS_IPHONE

#define CN_COPY_ENTITLEMENT CFSTR("com.apple.developer.networking.wifi-info")

static CFDictionaryRef
__CopyEntitlementsForPID(pid_t pid)
{
	uint8_t *buffer = NULL;
	size_t bufferlen = 0L;
	int64_t datalen = 0L;
	CFDataRef cfdata = NULL;
	struct csheader {
		uint32_t magic;
		uint32_t length;
	} csheader = { 0, 0 };
	int error = -1;
	CFPropertyListRef plist = NULL;

	/*
	 * Get the length of the actual entitlement data
	 */
	error = csops(pid, CS_OPS_ENTITLEMENTS_BLOB, &csheader, sizeof(csheader));

	if (error == -1 && errno == ERANGE) {
		bufferlen = ntohl(csheader.length);
		if (bufferlen > 1024 * 1024 || bufferlen < 8) {
			errno = EINVAL;
			goto out;
		}
		buffer = malloc(bufferlen);
		if (buffer == NULL) {
			goto out;
		}
		error = csops(pid, CS_OPS_ENTITLEMENTS_BLOB, buffer, bufferlen);
		if (error < 0) {
			goto out;
		}
	}

	datalen = bufferlen - sizeof(csheader);

	if (error == 0 && buffer && datalen > 0) {
		cfdata = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, buffer + sizeof(csheader), datalen, kCFAllocatorNull);
		if (cfdata == NULL) {
			goto out;
		}

		plist = CFPropertyListCreateWithData(NULL, cfdata, kCFPropertyListImmutable, NULL, NULL);
		if (!plist) {
			SC_log(LOG_ERR, "Could not decode entitlements for pid %d", pid);
		}
	}
	else {
		SC_log(LOG_ERR, "Could not get valid codesigning data for pid %d. Error %d", pid, error);
	}
out:
	if (error < 0) {
		SC_log(LOG_ERR, "Error getting entitlements for pid %d: %s", pid, strerror(errno));
	}
	if (cfdata != NULL) {
		CFRelease(cfdata);
	}
	if (buffer != NULL) {
		free(buffer);
	}
	if (plist && !isA_CFDictionary(plist)) {
		SC_log(LOG_ERR, "Could not decode entitlements for pid %d as a dictionary.", pid);
		CFRelease(plist);
		plist = NULL;
	}

	return plist;
}

static Boolean
__isApplicationEntitled(void)
{
	if (dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_12_0) {
		/* application is linked on or after iOS 12.0 SDK so it must have the entitlement */
		CFTypeRef entitlement = NULL;
		CFDictionaryRef entitlements = __CopyEntitlementsForPID(getpid());
		if (entitlements != NULL) {
			Boolean entitled = FALSE;
			entitlement = CFDictionaryGetValue(entitlements, CN_COPY_ENTITLEMENT);
			if(isA_CFBoolean(entitlement)) {
				entitled = CFBooleanGetValue(entitlement);
			}
			CFRelease(entitlements);
			return entitled;
		}
		/* application is linked on or after iOS 12.0 SDK but missing entitlement */
		return FALSE;
	}
	/* application is linked before iOS 12.0 SDK */
	return TRUE;
}

#endif /* TARGET_OS_IPHONE */

CFDictionaryRef
CNCopyCurrentNetworkInfo(CFStringRef	interfaceName)
{
#if	TARGET_OS_IPHONE && !TARGET_OS_IOSMAC
	if (__isApplicationEntitled() == FALSE) {
		SC_log(LOG_DEBUG, "Application does not have %@ entitlement", CN_COPY_ENTITLEMENT);
		return NULL;
	}
	static typeof (CNCopyCurrentNetworkInfo) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadCaptiveNetwork();
		if (image) dyfunc = dlsym(image, "__CNCopyCurrentNetworkInfo");
	}
	return dyfunc ? dyfunc(interfaceName) : NULL;
#else	// TARGET_OS_IPHONE && !TARGET_OS_IOSMAC
#pragma unused(interfaceName)
	return NULL;
#endif	// TARGET_OS_IPHONE && !TARGET_OS_IOSMAC
}

