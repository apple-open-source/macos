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
#include <unistd.h>
#include <sys/errno.h>

Boolean
SCPreferencesCommitChanges(SCPreferencesRef session)
{
	SCPreferencesPrivateRef	sessionPrivate	= (SCPreferencesPrivateRef)session;
	Boolean			wasLocked;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesCommitChanges:"));

	/*
	 * Determine if the we have exclusive access to the preferences
	 * and acquire the lock if necessary.
	 */
	wasLocked = sessionPrivate->locked;
	if (!wasLocked) {
		if (!SCPreferencesLock(session, TRUE)) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  SCPreferencesLock() failed"));
			return FALSE;
		}
	}

	/*
	 * if necessary, apply changes
	 */
	if (sessionPrivate->changed) {
		struct stat	statBuf;
		int		pathLen;
		char		*newPath;
		int		fd;
		CFDataRef	newPrefs;

		if (stat(sessionPrivate->path, &statBuf) == -1) {
			if (errno == ENOENT) {
				bzero(&statBuf, sizeof(statBuf));
				statBuf.st_mode = 0644;
				statBuf.st_uid  = geteuid();
				statBuf.st_gid  = getegid();
			} else {
				SCLog(_sc_verbose, LOG_ERR, CFSTR("stat() failed: %s"), strerror(errno));
				goto error;
			}
		}

		/* create the (new) preferences file */
		pathLen = strlen(sessionPrivate->path) + sizeof("-new");
		newPath = CFAllocatorAllocate(NULL, pathLen, 0);
		snprintf(newPath, pathLen, "%s-new", sessionPrivate->path);

		/* open the (new) preferences file */
	    reopen :
		fd = open(newPath, O_WRONLY|O_CREAT, statBuf.st_mode);
		if (fd == -1) {
			if ((errno == ENOENT) &&
			    ((sessionPrivate->prefsID == NULL) || !CFStringHasPrefix(sessionPrivate->prefsID, CFSTR("/")))) {
				char	*ch;

				ch = strrchr(newPath, '/');
				if (ch != NULL) {
					int	status;

					*ch = '\0';
					status = mkdir(newPath, 0755);
					*ch = '/';
					if (status == 0) {
						goto reopen;
					}
				}
			}
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPCommit open() failed: %s"), strerror(errno));
			CFAllocatorDeallocate(NULL, newPath);
			goto error;
		}

		/* preserve permissions */
		(void) fchown(fd, statBuf.st_uid, statBuf.st_gid);

		/* write the new preferences */
		newPrefs = CFPropertyListCreateXMLData(NULL, sessionPrivate->prefs);
		if (!newPrefs) {
			_SCErrorSet(kSCStatusFailed);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  CFPropertyListCreateXMLData() failed"));
			CFAllocatorDeallocate(NULL, newPath);
			(void) close(fd);
			goto error;
		}
		(void) write(fd, CFDataGetBytePtr(newPrefs), CFDataGetLength(newPrefs));
		(void) close(fd);
		CFRelease(newPrefs);

		/* rename new->old */
		if (rename(newPath, sessionPrivate->path) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("rename() failed: %s"), strerror(errno));
			CFAllocatorDeallocate(NULL, newPath);
			goto error;
		}
		CFAllocatorDeallocate(NULL, newPath);

		/* update signature */
		if (stat(sessionPrivate->path, &statBuf) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("stat() failed: %s"), strerror(errno));
			goto error;
		}
		CFRelease(sessionPrivate->signature);
		sessionPrivate->signature = __SCPSignatureFromStatbuf(&statBuf);
	}

	if (!sessionPrivate->isRoot) {
		/* CONFIGD REALLY NEEDS NON-ROOT WRITE ACCESS */
		goto perUser;
	}

	/* if necessary, create the session "commit" key */
	if (sessionPrivate->sessionKeyCommit == NULL) {
		sessionPrivate->sessionKeyCommit = _SCPNotificationKey(NULL,
								       sessionPrivate->prefsID,
								       sessionPrivate->perUser,
								       sessionPrivate->user,
								       kSCPreferencesKeyCommit);
	}

	/* post notification */
	if (!SCDynamicStoreNotifyValue(sessionPrivate->session,
				       sessionPrivate->sessionKeyCommit)) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("  SCDynamicStoreNotifyValue() failed"));
		_SCErrorSet(kSCStatusFailed);
		goto error;
	}

    perUser :

	if (!wasLocked)	(void) SCPreferencesUnlock(session);
	sessionPrivate->changed = FALSE;
	return TRUE;

    error :

	if (!wasLocked)	(void) SCPreferencesUnlock(session);
	return FALSE;
}
