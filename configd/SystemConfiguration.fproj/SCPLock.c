/*
 * Copyright(c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
#include <unistd.h>
#include <pthread.h>
#include <sys/errno.h>

Boolean
SCPreferencesLock(SCPreferencesRef prefs, Boolean wait)
{
	CFAllocatorRef		allocator		= CFGetAllocator(prefs);
	CFArrayRef		changes;
	Boolean			haveLock		= FALSE;
	SCPreferencesPrivateRef	prefsPrivate		= (SCPreferencesPrivateRef)prefs;
	CFDateRef		value			= NULL;

	if (prefs == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return FALSE;
	}

	if (prefsPrivate->locked) {
		/* sorry, you already have the lock */
		_SCErrorSet(kSCStatusLocked);
		return FALSE;
	}

	if (!prefsPrivate->isRoot) {
		if (!prefsPrivate->perUser) {
			_SCErrorSet(kSCStatusAccessError);
			return FALSE;
		} else {
			/* CONFIGD REALLY NEEDS NON-ROOT WRITE ACCESS */
			goto perUser;
		}
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	if (prefsPrivate->session == NULL) {
		__SCPreferencesAddSession(prefs);
	}

	if (prefsPrivate->sessionKeyLock == NULL) {
		/* create the session "lock" key */
		prefsPrivate->sessionKeyLock = _SCPNotificationKey(allocator,
								   prefsPrivate->prefsID,
								   prefsPrivate->perUser,
								   prefsPrivate->user,
								   kSCPreferencesKeyLock);
	}

	pthread_mutex_unlock(&prefsPrivate->lock);

	if (!SCDynamicStoreAddWatchedKey(prefsPrivate->session,
					 prefsPrivate->sessionKeyLock,
					 FALSE)) {
		SCLog(_sc_verbose, LOG_INFO, CFSTR("SCPreferencesLock SCDynamicStoreAddWatchedKey() failed"));
		goto error;
	}

	value = CFDateCreate(allocator, CFAbsoluteTimeGetCurrent());

	while (TRUE) {
		CFArrayRef	changedKeys;

		/*
		 * Attempt to acquire the lock
		 */
		if (SCDynamicStoreAddTemporaryValue(prefsPrivate->session,
						    prefsPrivate->sessionKeyLock,
						    value)) {
			haveLock = TRUE;
			goto done;
		} else {
			if (!wait) {
				_SCErrorSet(kSCStatusPrefsBusy);
				goto error;
			}
		}

		/*
		 * Wait for the lock to be released
		 */
		if (!SCDynamicStoreNotifyWait(prefsPrivate->session)) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCPreferencesLock SCDynamicStoreNotifyWait() failed"));
			goto error;
		}
		changedKeys = SCDynamicStoreCopyNotifiedKeys(prefsPrivate->session);
		if (!changedKeys) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCPreferencesLock SCDynamicStoreCopyNotifiedKeys() failed"));
			goto error;
		}
		CFRelease(changedKeys);
	}

    done :

	CFRelease(value);
	value = NULL;

	if (!SCDynamicStoreRemoveWatchedKey(prefsPrivate->session,
					    prefsPrivate->sessionKeyLock,
					    0)) {
		SCLog(_sc_verbose, LOG_INFO, CFSTR("SCPreferencesLock SCDynamicStoreRemoveWatchedKey() failed"));
		goto error;
	}

	changes = SCDynamicStoreCopyNotifiedKeys(prefsPrivate->session);
	if (!changes) {
		SCLog(_sc_verbose, LOG_INFO, CFSTR("SCPreferencesLock SCDynamicStoreCopyNotifiedKeys() failed"));
		goto error;
	}
	CFRelease(changes);

    perUser:

	if (prefsPrivate->accessed) {
		CFDataRef       currentSignature;
		Boolean		match;
		struct stat     statBuf;

		/*
		 * the preferences have been accessed since the
		 * session was created so we need to compare
		 * the signature of the stored preferences.
		 */
		if (stat(prefsPrivate->path, &statBuf) == -1) {
			if (errno == ENOENT) {
				bzero(&statBuf, sizeof(statBuf));
			} else {
				SCLog(TRUE, LOG_DEBUG, CFSTR("SCPreferencesLock stat() failed: %s"), strerror(errno));
				_SCErrorSet(kSCStatusStale);
				goto error;
			}
		}

		currentSignature = __SCPSignatureFromStatbuf(&statBuf);
		match = CFEqual(prefsPrivate->signature, currentSignature);
		CFRelease(currentSignature);
		if (!match) {
			/*
			 * the preferences have been updated since the
			 * session was accessed so we've got no choice
			 * but to deny the lock request.
			 */
			_SCErrorSet(kSCStatusStale);
			goto error;
		}
//	} else {
//		/*
//		 * the file contents have changed but since we
//		 * haven't accessed any of the preference data we
//		 * don't need to return an error.  Simply proceed.
//		 */
	}

	prefsPrivate->locked = TRUE;
	return TRUE;

    error :

	if (haveLock) {
		SCDynamicStoreRemoveValue(prefsPrivate->session,
					  prefsPrivate->sessionKeyLock);
	}
	if (value)		CFRelease(value);

	return FALSE;
}
