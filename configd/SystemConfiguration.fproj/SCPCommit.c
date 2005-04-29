/*
 * Copyright(c) 2000-2003 Apple Computer, Inc. All rights reserved.
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
#include <sys/errno.h>

static ssize_t
writen(int d, const void *buf, size_t nbytes)
{
	size_t		left	= nbytes;
	const void	*p	= buf;

	while (left > 0) {
		ssize_t	n;

		n = write(d, p, left);
		if (n >= 0) {
			left -= n;
			p    +=	n;
		} else {
			if (errno != EINTR) {
				return -1;
			}
		}
	}
	return nbytes;
}


Boolean
SCPreferencesCommitChanges(SCPreferencesRef prefs)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	Boolean			wasLocked;

	if (prefs == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return FALSE;
	}

	/*
	 * Determine if the we have exclusive access to the preferences
	 * and acquire the lock if necessary.
	 */
	wasLocked = prefsPrivate->locked;
	if (!wasLocked) {
		if (!SCPreferencesLock(prefs, TRUE)) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges SCPreferencesLock() failed"));
			return FALSE;
		}
	}

	/*
	 * if necessary, apply changes
	 */
	if (prefsPrivate->changed) {
		int		fd;
		CFDataRef	newPrefs;
		char *		path;
		int		pathLen;
		struct stat	statBuf;
		char *		thePath;

		if (stat(prefsPrivate->path, &statBuf) == -1) {
			if (errno == ENOENT) {
				bzero(&statBuf, sizeof(statBuf));
				statBuf.st_mode = 0644;
				statBuf.st_uid  = geteuid();
				statBuf.st_gid  = getegid();
			} else {
				SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges stat() failed: %s"), strerror(errno));
				goto error;
			}
		}

		/* create the (new) preferences file */
		path = prefsPrivate->newPath ? prefsPrivate->newPath : prefsPrivate->path;
		pathLen = strlen(path) + sizeof("-new");
		thePath = CFAllocatorAllocate(NULL, pathLen, 0);
		snprintf(thePath, pathLen, "%s-new", path);

		/* open the (new) preferences file */
	    reopen :
		fd = open(thePath, O_WRONLY|O_CREAT, statBuf.st_mode);
		if (fd == -1) {
			if ((errno == ENOENT) &&
			    ((prefsPrivate->prefsID == NULL) || !CFStringHasPrefix(prefsPrivate->prefsID, CFSTR("/")))) {
				char	*ch;

				ch = strrchr(thePath, '/');
				if (ch != NULL) {
					int	status;

					*ch = '\0';
					status = mkdir(thePath, 0755);
					*ch = '/';
					if (status == 0) {
						goto reopen;
					}
				}
			}
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges open() failed: %s"), strerror(errno));
			CFAllocatorDeallocate(NULL, thePath);
			goto error;
		}

		/* preserve permissions */
		(void) fchown(fd, statBuf.st_uid, statBuf.st_gid);
		(void) fchmod(fd, statBuf.st_mode);

		/* write the new preferences */
		newPrefs = CFPropertyListCreateXMLData(NULL, prefsPrivate->prefs);
		if (!newPrefs) {
			_SCErrorSet(kSCStatusFailed);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges CFPropertyListCreateXMLData() failed"));
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  prefs = %s"), path);
			CFAllocatorDeallocate(NULL, thePath);
			(void) close(fd);
			goto error;
		}
		if (writen(fd, (void *)CFDataGetBytePtr(newPrefs), CFDataGetLength(newPrefs)) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges write() failed: %s"), strerror(errno));
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  path = %s"), thePath);
			(void) unlink(thePath);
			CFAllocatorDeallocate(NULL, thePath);
			(void) close(fd);
			CFRelease(newPrefs);
			goto error;
		}
		if (fsync(fd) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges fsync() failed: %s"), strerror(errno));
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  path = %s"), thePath);
			(void) unlink(thePath);
			CFAllocatorDeallocate(NULL, thePath);
			(void) close(fd);
			CFRelease(newPrefs);
			goto error;
		}
		if (close(fd) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges close() failed: %s"), strerror(errno));
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  path = %s"), thePath);
			(void) unlink(thePath);
			CFAllocatorDeallocate(NULL, thePath);
			CFRelease(newPrefs);
			goto error;
		}
		CFRelease(newPrefs);

		/* rename new->old */
		if (rename(thePath, path) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges rename() failed: %s"), strerror(errno));
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  path = %s --> %s"), thePath, path);
			CFAllocatorDeallocate(NULL, thePath);
			goto error;
		}
		CFAllocatorDeallocate(NULL, thePath);

		if (prefsPrivate->newPath) {
			/* prefs file saved in "new" directory */
			(void) unlink(prefsPrivate->path);
			(void) symlink(prefsPrivate->newPath, prefsPrivate->path);
			CFAllocatorDeallocate(NULL, prefsPrivate->path);
			prefsPrivate->path = path;
			prefsPrivate->newPath = NULL;
		}

		/* update signature */
		if (stat(path, &statBuf) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges stat() failed: %s"), strerror(errno));
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  path = %s"), thePath);
			goto error;
		}
		CFRelease(prefsPrivate->signature);
		prefsPrivate->signature = __SCPSignatureFromStatbuf(&statBuf);
	}

	if (!prefsPrivate->isRoot) {
		/* CONFIGD REALLY NEEDS NON-ROOT WRITE ACCESS */
		goto perUser;
	}

	/* post notification */
	if (!SCDynamicStoreNotifyValue(prefsPrivate->session,
				       prefsPrivate->sessionKeyCommit)) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges SCDynamicStoreNotifyValue() failed"));
		_SCErrorSet(kSCStatusFailed);
		goto error;
	}

    perUser :

	if (!wasLocked)	(void) SCPreferencesUnlock(prefs);
	prefsPrivate->changed = FALSE;
	return TRUE;

    error :

	if (!wasLocked)	(void) SCPreferencesUnlock(prefs);
	return FALSE;
}
