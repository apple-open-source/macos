/*
 * Copyright(c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
#include <unistd.h>
#include <sys/errno.h>

Boolean
SCPreferencesLock(SCPreferencesRef session, Boolean wait)
{
	CFArrayRef		changes;
	CFDataRef		currentSignature	= NULL;
	Boolean			haveLock		= FALSE;
	SCPreferencesPrivateRef	sessionPrivate		= (SCPreferencesPrivateRef)session;
	struct stat		statBuf;
	CFDateRef		value			= NULL;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesLock:"));

	if (sessionPrivate->locked) {
		/* sorry, you already have the lock */
		_SCErrorSet(kSCStatusLocked);
		return FALSE;
	}

	if (!sessionPrivate->isRoot) {
		if (!sessionPrivate->perUser) {
			_SCErrorSet(kSCStatusAccessError);
			return FALSE;
		} else {
			/* CONFIGD REALLY NEEDS NON-ROOT WRITE ACCESS */
			goto perUser;
		}
	}

	if (sessionPrivate->session == NULL) {
		/* open a session */
		sessionPrivate->session = SCDynamicStoreCreate(NULL,
							       CFSTR("SCPreferencesLock"),
							       NULL,
							       NULL);
		if (!sessionPrivate->session) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			return FALSE;
		}
	}

	if (sessionPrivate->sessionKeyLock == NULL) {
		/* create the session "lock" key */
		sessionPrivate->sessionKeyLock = _SCPNotificationKey(NULL,
								     sessionPrivate->prefsID,
								     sessionPrivate->perUser,
								     sessionPrivate->user,
								     kSCPreferencesKeyLock);
	}

	if (!SCDynamicStoreAddWatchedKey(sessionPrivate->session,
					 sessionPrivate->sessionKeyLock,
					 FALSE)) {
		SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreAddWatchedKey() failed"));
		goto error;
	}

	value  = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());

	while (TRUE) {
		CFArrayRef	changedKeys;

		/*
		 * Attempt to acquire the lock
		 */
		if (SCDynamicStoreAddTemporaryValue(sessionPrivate->session,
						    sessionPrivate->sessionKeyLock,
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
		if (!SCDynamicStoreNotifyWait(sessionPrivate->session)) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreNotifyWait() failed"));
			goto error;
		}
		changedKeys = SCDynamicStoreCopyNotifiedKeys(sessionPrivate->session);
		if (!changedKeys) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCopyNotifiedKeys() failed"));
			goto error;
		}
		CFRelease(changedKeys);
	}

    done :

	CFRelease(value);
	value = NULL;

	if (!SCDynamicStoreRemoveWatchedKey(sessionPrivate->session,
					    sessionPrivate->sessionKeyLock,
					    0)) {
		SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreRemoveWatchedKey() failed"));
		goto error;
	}

	changes = SCDynamicStoreCopyNotifiedKeys(sessionPrivate->session);
	if (!changes) {
		SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCopyNotifiedKeys() failed"));
		goto error;
	}
	CFRelease(changes);

    perUser:

	/*
	 * Check the signature
	 */
	if (stat(sessionPrivate->path, &statBuf) == -1) {
		if (errno == ENOENT) {
			bzero(&statBuf, sizeof(statBuf));
		} else {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("stat() failed: %s"), strerror(errno));
			_SCErrorSet(kSCStatusStale);
			goto error;
		}
	}

	currentSignature = __SCPSignatureFromStatbuf(&statBuf);
	if (!CFEqual(sessionPrivate->signature, currentSignature)) {
		if (sessionPrivate->accessed) {
			/*
			 * the preferences have been accessed since the
			 * session was created so we've got no choice
			 * but to deny the lock request.
			 */
			_SCErrorSet(kSCStatusStale);
			goto error;
		} else {
			/*
			 * the file contents have changed but since we
			 * haven't accessed any of the preferences we
			 * don't need to return an error.  Simply reload
			 * the stored data and proceed.
			 */
			SCPreferencesRef	newPrefs;
			SCPreferencesPrivateRef	newPrivate;

			newPrefs = __SCPreferencesCreate(NULL,
							 sessionPrivate->name,
							 sessionPrivate->prefsID,
							 sessionPrivate->perUser,
							 sessionPrivate->user);
			if (!newPrefs) {
				/* if updated preferences could not be loaded */
				_SCErrorSet(kSCStatusStale);
				goto error;
			}

			/* synchronize this sessions prefs/signature */
			newPrivate = (SCPreferencesPrivateRef)newPrefs;
			CFRelease(sessionPrivate->prefs);
			sessionPrivate->prefs = newPrivate->prefs;
			CFRetain(sessionPrivate->prefs);
			CFRelease(sessionPrivate->signature);
			sessionPrivate->signature = CFRetain(newPrivate->signature);
			CFRelease(newPrefs);
		}
	}
	CFRelease(currentSignature);

	sessionPrivate->locked = TRUE;
	return TRUE;

    error :

	if (haveLock) {
		SCDynamicStoreRemoveValue(sessionPrivate->session,
					  sessionPrivate->sessionKeyLock);
	}
	if (currentSignature)	CFRelease(currentSignature);
	if (value)		CFRelease(value);

	return FALSE;
}
