/*
 * Copyright(c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1(the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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
	sig->st_mtimespec = statBuf->st_mtimespec;
	sig->st_size      = statBuf->st_size;
	return signature;
}


__private_extern__ char *
__SCPreferencesPath(CFAllocatorRef	allocator,
		    CFStringRef		prefsID,
		    Boolean		perUser,
		    CFStringRef		user)
{
	CFStringRef	path		= NULL;
	int		pathLen;
	char		*pathStr;

	if (perUser) {
		if (prefsID == NULL) {
			/* no user prefsID specified */
			return NULL;
		} else if (CFStringHasPrefix(prefsID, CFSTR("/"))) {
			/* if absolute path */
			path = CFRetain(prefsID);
		} else {
			/*
			 * relative (to the user's preferences) path
			 */
			char		login[MAXLOGNAME+1];
			struct passwd	*pwd;

			bzero(&login, sizeof(login));
			if (user == NULL) {
				CFStringRef	u;

				/* get current console user */
				u = SCDynamicStoreCopyConsoleUser(NULL, NULL, NULL);
				if (!u) {
					/* if could not get console user */
					return NULL;
				}
				(void) CFStringGetBytes(u,
							CFRangeMake(0, CFStringGetLength(u)),
							kCFStringEncodingMacRoman,
							0,
							FALSE,
							login,
							MAXLOGNAME,
							NULL);
				CFRelease(u);
			} else {
				/* use specified user */
				(void) CFStringGetBytes(user,
							CFRangeMake(0, CFStringGetLength(user)),
							kCFStringEncodingMacRoman,
							0,
							FALSE,
							login,
							MAXLOGNAME,
							NULL);
			}

			/* get password entry for user */
			pwd = getpwnam(login);
			if (pwd == NULL) {
				/* if no home directory */
				return NULL;
			}

			/* create prefs ID */
			path = CFStringCreateWithFormat(allocator,
							NULL,
							CFSTR("%s/%@/%@"),
							pwd->pw_dir,
							PREFS_DEFAULT_USER_DIR,
							prefsID);
		}
	} else {
		if (prefsID == NULL) {
			/* default preference ID */
			path = CFStringCreateWithFormat(allocator,
							NULL,
							CFSTR("%@/%@"),
							PREFS_DEFAULT_DIR,
							PREFS_DEFAULT_CONFIG);
		} else if (CFStringHasPrefix(prefsID, CFSTR("/"))) {
			/* if absolute path */
			path = CFRetain(prefsID);
		} else {
			/* relative path */
			path = CFStringCreateWithFormat(allocator,
							NULL,
							CFSTR("%@/%@"),
							PREFS_DEFAULT_DIR,
							prefsID);
		}
	}

	/*
	 * convert CFStringRef path to C-string path
	 */
	pathLen = CFStringGetLength(path) + 1;
	pathStr = CFAllocatorAllocate(allocator, pathLen, 0);
	if (!CFStringGetCString(path,
				pathStr,
				pathLen,
				kCFStringEncodingMacRoman)) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("could not convert path to C string"));
		CFAllocatorDeallocate(allocator, pathStr);
		pathStr = NULL;
	}

	CFRelease(path);
	return pathStr;
}


CFDataRef
SCPreferencesGetSignature(SCPreferencesRef session)
{
	SCPreferencesPrivateRef	sessionPrivate	= (SCPreferencesPrivateRef)session;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesGetSignature:"));

	sessionPrivate->accessed = TRUE;
	return sessionPrivate->signature;
}


__private_extern__ CFStringRef
_SCPNotificationKey(CFAllocatorRef	allocator,
		    CFStringRef		prefsID,
		    Boolean		perUser,
		    CFStringRef		user,
		    int			keyType)
{
	CFStringRef	key		= NULL;
	char		*pathStr;
	char		*typeStr;

	pathStr = __SCPreferencesPath(allocator, prefsID, perUser, user);
	if (pathStr == NULL) {
		return NULL;
	}

	/* create notification key */
	switch (keyType) {
		case kSCPreferencesKeyLock :
			typeStr = "lock";
			break;
		case kSCPreferencesKeyCommit :
			typeStr = "commit";
			break;
		case kSCPreferencesKeyApply :
			typeStr = "apply";
			break;
		default :
			typeStr = "?";
	}

	key = CFStringCreateWithFormat(allocator,
				       NULL,
				       CFSTR("%@%s:%s"),
				       kSCDynamicStoreDomainPrefs,
				       typeStr,
				       pathStr);

	CFAllocatorDeallocate(allocator, pathStr);
	return key;
}


CFStringRef
SCDynamicStoreKeyCreatePreferences(CFAllocatorRef	allocator,
				   CFStringRef		prefsID,
				   int			keyType)
{
	return _SCPNotificationKey(allocator, prefsID, FALSE, NULL, keyType);
}


CFStringRef
SCDynamicStoreKeyCreateUserPreferences(CFAllocatorRef	allocator,
				       CFStringRef	prefsID,
				       CFStringRef	user,
				       int		keyType)
{
	return _SCPNotificationKey(allocator, prefsID, TRUE, user, keyType);
}
