/*
 * Copyright (c) 2000, 2001, 2003-2005, 2007, 2008 Apple Inc. All rights reserved.
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
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCPreferencesInternal.h"

#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/param.h>

__private_extern__ CFDataRef
__SCPSignatureFromStatbuf(const struct stat *statBuf)
{
	CFMutableDataRef	signature;
	SCPSignatureDataRef	sig;

	signature = CFDataCreateMutable(NULL, sizeof(SCPSignatureData));
	CFDataSetLength(signature, sizeof(SCPSignatureData));
	sig = (SCPSignatureDataRef)CFDataGetBytePtr(signature);
	sig->st_dev       = statBuf->st_dev;
	sig->st_ino       = statBuf->st_ino;
	sig->tv_sec       = statBuf->st_mtimespec.tv_sec;
	sig->tv_nsec      = statBuf->st_mtimespec.tv_nsec;
	sig->st_size      = statBuf->st_size;
	return signature;
}


__private_extern__ char *
__SCPreferencesPath(CFAllocatorRef	allocator,
		    CFStringRef		prefsID,
		    Boolean		useNewPrefs)
{
	CFStringRef	path		= NULL;
	char		*pathStr;

	if (prefsID == NULL) {
		/* default preference ID */
		path = CFStringCreateWithFormat(allocator,
						NULL,
						CFSTR("%@/%@"),
						useNewPrefs ? PREFS_DEFAULT_DIR    : PREFS_DEFAULT_DIR_OLD,
						useNewPrefs ? PREFS_DEFAULT_CONFIG : PREFS_DEFAULT_CONFIG_OLD);
	} else if (CFStringHasPrefix(prefsID, CFSTR("/"))) {
		/* if absolute path */
		path = CFStringCreateCopy(allocator, prefsID);
	} else {
		/* relative path */
		path = CFStringCreateWithFormat(allocator,
						NULL,
						CFSTR("%@/%@"),
						useNewPrefs ? PREFS_DEFAULT_DIR : PREFS_DEFAULT_DIR_OLD,
						prefsID);
		if (useNewPrefs && CFStringHasSuffix(path, CFSTR(".xml"))) {
			CFMutableStringRef	newPath;

			newPath = CFStringCreateMutableCopy(allocator, 0, path);
			CFStringReplace(newPath,
					CFRangeMake(CFStringGetLength(newPath)-4, 4),
					CFSTR(".plist"));
			CFRelease(path);
			path = newPath;
		}
	}

	/*
	 * convert CFStringRef path to C-string path
	 */
	pathStr = _SC_cfstring_to_cstring(path, NULL, 0, kCFStringEncodingASCII);
	if (pathStr == NULL) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("could not convert path to C string"));
	}

	CFRelease(path);
	return pathStr;
}


CFDataRef
SCPreferencesGetSignature(SCPreferencesRef prefs)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	if (prefs == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return NULL;
	}

	__SCPreferencesAccess(prefs);

	return prefsPrivate->signature;
}


__private_extern__ CFStringRef
_SCPNotificationKey(CFAllocatorRef	allocator,
		    CFStringRef		prefsID,
		    int			keyType)
{
	CFStringRef	keyStr;
	char		*path;
	CFStringRef	pathStr;
	CFStringRef	storeKey;

	switch (keyType) {
		case kSCPreferencesKeyCommit :
			keyStr = CFSTR("commit");
			break;
		case kSCPreferencesKeyApply :
			keyStr = CFSTR("apply");
			break;
		default :
			return NULL;
	}

	path = __SCPreferencesPath(allocator, prefsID, TRUE);
	if (path == NULL) {
		return NULL;
	}

	pathStr = CFStringCreateWithCStringNoCopy(allocator,
						  path,
						  kCFStringEncodingASCII,
						  kCFAllocatorNull);

	storeKey = CFStringCreateWithFormat(allocator,
					    NULL,
					    CFSTR("%@%@:%@"),
					    kSCDynamicStoreDomainPrefs,
					    keyStr,
					    pathStr);

	CFRelease(pathStr);
	CFAllocatorDeallocate(NULL, path);
	return storeKey;
}


CFStringRef
SCDynamicStoreKeyCreatePreferences(CFAllocatorRef	allocator,
				   CFStringRef		prefsID,
				   SCPreferencesKeyType	keyType)
{
	return _SCPNotificationKey(allocator, prefsID, keyType);
}
