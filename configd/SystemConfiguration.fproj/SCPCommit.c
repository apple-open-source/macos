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

#include <SystemConfiguration/SCP.h>
#include "SCPPrivate.h"

#include <SystemConfiguration/SCD.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>


SCPStatus
SCPCommit(SCPSessionRef session)
{
	SCPSessionPrivateRef	sessionPrivate;
	SCPStatus		scp_status = SCP_OK;
	SCDStatus		scd_status;
	boolean_t		wasLocked;

	if (session == NULL) {
		return SCP_FAILED;           /* you can't do anything with a closed session */
	}
	sessionPrivate = (SCPSessionPrivateRef)session;

	/*
	 * Determine if the we have exclusive access to the preferences
	 * and acquire the lock if necessary.
	 */
	wasLocked = sessionPrivate->locked;
	if (!wasLocked) {
		scp_status = SCPLock(session, TRUE);
		if (scp_status != SCD_OK) {
			SCDLog(LOG_DEBUG, CFSTR("  SCPLock(): %s"), SCPError(scp_status));
			return scp_status;
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
				SCDLog(LOG_DEBUG, CFSTR("stat() failed: %s"), strerror(errno));
				scp_status = SCP_FAILED;
				goto done;
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
			SCDLog(LOG_DEBUG, CFSTR("SCPCommit open() failed: %s"), strerror(errno));
			CFAllocatorDeallocate(NULL, newPath);
			scp_status = SCP_FAILED;
			goto done;
		}

		/* preserve permissions */
		(void)fchown(fd, statBuf.st_uid, statBuf.st_gid);

		/* write the new preferences */
		newPrefs = CFPropertyListCreateXMLData(NULL, sessionPrivate->prefs);
		(void) write(fd, CFDataGetBytePtr(newPrefs), CFDataGetLength(newPrefs));
		(void) close(fd);
		CFRelease(newPrefs);

		/* rename new->old */
		if (rename(newPath, sessionPrivate->path) == -1) {
			SCDLog(LOG_DEBUG, CFSTR("rename() failed: %s"), strerror(errno));
			CFAllocatorDeallocate(NULL, newPath);
			scp_status = SCP_FAILED;
			goto done;
		}
		CFAllocatorDeallocate(NULL, newPath);

		/* update signature */
		if (stat(sessionPrivate->path, &statBuf) == -1) {
			SCDLog(LOG_DEBUG, CFSTR("stat() failed: %s"), strerror(errno));
			scp_status = SCP_FAILED;
			goto done;
		}
		CFRelease(sessionPrivate->signature);
		sessionPrivate->signature = _SCPSignatureFromStatbuf(&statBuf);
	}

	if (!sessionPrivate->isRoot) {
		/* CONFIGD REALLY NEEDS NON-ROOT WRITE ACCESS */
		goto done;
	}

	/* if necessary, create the session "commit" key */
	if (sessionPrivate->sessionKeyCommit == NULL) {
		sessionPrivate->sessionKeyCommit = _SCPNotificationKey(sessionPrivate->prefsID,
								       sessionPrivate->perUser,
								       sessionPrivate->user,
								       kSCPKeyCommit);
	}

	/* post notification */
	scd_status = SCDLock(sessionPrivate->session);
	if (scd_status == SCD_OK) {
		(void) SCDTouch (sessionPrivate->session, sessionPrivate->sessionKeyCommit);
		(void) SCDRemove(sessionPrivate->session, sessionPrivate->sessionKeyCommit);
		(void) SCDUnlock(sessionPrivate->session);
	} else {
		SCDLog(LOG_DEBUG, CFSTR("  SCDLock(): %s"), SCDError(scd_status));
		scp_status = SCP_FAILED;
	}

    done :

	if (!wasLocked)
		(void) SCPUnlock(session);

	sessionPrivate->changed = FALSE;

	return scp_status;
}
