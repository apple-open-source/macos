/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
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
#include "SCHelper_client.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>

static Boolean
__SCPreferencesCommitChanges_helper(SCPreferencesRef prefs)
{
	CFDataRef		data		= NULL;
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	uint32_t		status		= kSCStatusOK;
	CFDataRef		reply		= NULL;

	if (prefsPrivate->helper == -1) {
		// if no helper
		goto fail;
	}

	if (prefsPrivate->changed) {
		ok = _SCSerialize(prefsPrivate->prefs, &data, NULL, NULL);
		if (!ok) {
			goto fail;
		}
	}

	// have the helper "commit" the prefs
//	status = kSCStatusOK;
//	reply  = NULL;
	ok = _SCHelperExec(prefsPrivate->helper,
			   SCHELPER_MSG_PREFS_COMMIT,
			   data,
			   &status,
			   &reply);
	if (data != NULL) CFRelease(data);
	if (!ok) {
		goto fail;
	}

	if (status != kSCStatusOK) {
		goto error;
	}

	if (prefsPrivate->changed) {
		if (prefsPrivate->signature != NULL) CFRelease(prefsPrivate->signature);
		prefsPrivate->signature = reply;
	}

	prefsPrivate->changed = FALSE;
	return TRUE;

    fail :

	// close helper
	if (prefsPrivate->helper != -1) {
		_SCHelperClose(prefsPrivate->helper);
		prefsPrivate->helper = -1;
	}

	status = kSCStatusAccessError;

    error :

	// return error
	if (reply != NULL) CFRelease(reply);
	_SCErrorSet(status);
	return FALSE;
}


static ssize_t
writen(int ref, const void *data, size_t len)
{
	size_t		left	= len;
	ssize_t		n;
	const void	*p	= data;

	while (left > 0) {
		if ((n = write(ref, p, left)) == -1) {
			if (errno != EINTR) {
				return -1;
			}
			n = 0;
		}
		left -= n;
		p += n;
	}
	return len;
}


Boolean
SCPreferencesCommitChanges(SCPreferencesRef prefs)
{
	Boolean			ok		= FALSE;
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

	if (prefsPrivate->authorizationData != NULL) {
		ok = __SCPreferencesCommitChanges_helper(prefs);
		if (ok) {
			prefsPrivate->changed = FALSE;
		}
		goto done;
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
				goto done;
			}
		}

		/* create the (new) preferences file */
		path = prefsPrivate->newPath ? prefsPrivate->newPath : prefsPrivate->path;
		pathLen = strlen(path) + sizeof("-new");
		thePath = CFAllocatorAllocate(NULL, pathLen, 0);
		snprintf(thePath, pathLen, "%s-new", path);

		fd = open(thePath, O_WRONLY|O_CREAT, statBuf.st_mode);
		if (fd == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges open() failed: %s"), strerror(errno));
			CFAllocatorDeallocate(NULL, thePath);
			goto done;
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
			goto done;
		}
		if (writen(fd, (const void *)CFDataGetBytePtr(newPrefs), CFDataGetLength(newPrefs)) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges write() failed: %s"), strerror(errno));
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  path = %s"), thePath);
			(void) unlink(thePath);
			CFAllocatorDeallocate(NULL, thePath);
			(void) close(fd);
			CFRelease(newPrefs);
			goto done;
		}

#if	!TARGET_OS_IPHONE
		/* synchronize the file's in-core state with that on disk */
		if (fsync(fd) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges fsync() failed: %s"), strerror(errno));
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  path = %s"), thePath);
			(void) unlink(thePath);
			CFAllocatorDeallocate(NULL, thePath);
			(void) close(fd);
			CFRelease(newPrefs);
			goto done;
		}

		/*
		 * ... and ask the drive to flush to the media
		 *
		 * Note: at present, this only works on HFS filesystems
		 */
		(void) fcntl(fd, F_FULLFSYNC, 0);
#endif	// !TARGET_OS_IPHONE

		/* new preferences have been written */
		if (close(fd) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges close() failed: %s"), strerror(errno));
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  path = %s"), thePath);
			(void) unlink(thePath);
			CFAllocatorDeallocate(NULL, thePath);
			CFRelease(newPrefs);
			goto done;
		}
		CFRelease(newPrefs);

		/* rename new->old */
		if (rename(thePath, path) == -1) {
			_SCErrorSet(errno);
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges rename() failed: %s"), strerror(errno));
			SCLog(_sc_verbose, LOG_ERR, CFSTR("  path = %s --> %s"), thePath, path);
			CFAllocatorDeallocate(NULL, thePath);
			goto done;
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
			goto done;
		}
		if (prefsPrivate->signature != NULL) CFRelease(prefsPrivate->signature);
		prefsPrivate->signature = __SCPSignatureFromStatbuf(&statBuf);
	}

	/* post notification */
	if (prefsPrivate->session == NULL) {
		ok = TRUE;
	} else {
		ok = SCDynamicStoreNotifyValue(prefsPrivate->session, prefsPrivate->sessionKeyCommit);
		if (!ok) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("SCPreferencesCommitChanges SCDynamicStoreNotifyValue() failed"));
			_SCErrorSet(kSCStatusFailed);
			goto done;
		}
	}

	prefsPrivate->changed = FALSE;

    done :

	if (!wasLocked)	(void) SCPreferencesUnlock(prefs);
	return ok;
}
