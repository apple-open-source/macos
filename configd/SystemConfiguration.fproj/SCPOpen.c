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
#include <SystemConfiguration/SCPPath.h>
#include "SCPPrivate.h"

#include <SystemConfiguration/SCD.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>


static SCPStatus
_SCPOpen(SCPSessionRef	*session,
	 CFStringRef	name,
	 CFStringRef	prefsID,
	 boolean_t	perUser,
	 CFStringRef	user,
	 int		options)
{
	SCPStatus		scp_status;
	SCPSessionPrivateRef	newSession;
	int			fd		= -1;
	struct stat		statBuf;
	CFMutableDataRef	xmlData;
	CFStringRef		xmlError;

	newSession = (SCPSessionPrivateRef)CFAllocatorAllocate(NULL, sizeof(SCPSessionPrivate), 0);
	newSession->name		= NULL;
	newSession->prefsID		= NULL;
	newSession->perUser		= perUser;
	newSession->user		= NULL;
	newSession->path		= NULL;
	newSession->signature		= NULL;
	newSession->session		= NULL;
	newSession->sessionKeyLock	= NULL;
	newSession->sessionKeyCommit	= NULL;
	newSession->sessionKeyApply	= NULL;
	newSession->prefs		= NULL;
	newSession->changed		= FALSE;
	newSession->locked		= FALSE;
	newSession->isRoot		= (geteuid() == 0);

	/*
	 * convert prefsID to path
	 */
	newSession->path = _SCPPrefsPath(prefsID, perUser, user);
	if (newSession->path == NULL) {
		scp_status = SCP_FAILED;
		goto error;
	}

	/*
	 * open file
	 */
	fd = open(newSession->path, O_RDONLY, 0644);
	if (fd == -1) {
		char	*errmsg	= strerror(errno);

		switch (errno) {
			case ENOENT :
				if (options & kSCPOpenCreatePrefs) {
					bzero(&statBuf, sizeof(statBuf));
					goto create_1;
				}
				scp_status = SCP_ENOENT;
				break;
			case EACCES :
				scp_status = SCP_EACCESS;
				break;
			default :
				scp_status = SCP_FAILED;
		}
		SCDLog(LOG_DEBUG, CFSTR("open() failed: %s"), errmsg);
		goto error;
	}

	/*
	 * check file, create signature
	 */
	if (fstat(fd, &statBuf) == -1) {
		SCDLog(LOG_DEBUG, CFSTR("fstat() failed: %s"), strerror(errno));
		scp_status = SCP_FAILED;
		goto error;
	}

    create_1 :

	newSession->signature = _SCPSignatureFromStatbuf(&statBuf);

	if (statBuf.st_size > 0) {
		/*
		 * extract property list
		 */
		xmlData = CFDataCreateMutable(NULL, statBuf.st_size);
		CFDataSetLength(xmlData, statBuf.st_size);
		if (read(fd, (void *)CFDataGetBytePtr(xmlData), statBuf.st_size) != statBuf.st_size) {
			SCDLog(LOG_DEBUG, CFSTR("_SCPOpen read(): could not load preference data."));
			CFRelease(xmlData);
			xmlData = NULL;
			if (options & kSCPOpenCreatePrefs) {
				goto create_2;
			}
			scp_status = SCP_BADCF;
			goto error;
		}

		/*
		 * load preferences
		 */
		newSession->prefs = (CFMutableDictionaryRef)
				    CFPropertyListCreateFromXMLData(NULL,
								    xmlData,
								    kCFPropertyListMutableContainers,
								    &xmlError);
		CFRelease(xmlData);
		if (xmlError) {
			SCDLog(LOG_DEBUG, CFSTR("_SCPOpen CFPropertyListCreateFromXMLData(): %s"), xmlError);
			if (options & kSCPOpenCreatePrefs) {
				goto create_2;
			}
			scp_status = SCP_BADCF;
			goto error;
		}

		/*
		 * make sure that we've got a dictionary
		 */
		if (CFGetTypeID(newSession->prefs) != CFDictionaryGetTypeID()) {
				SCDLog(LOG_DEBUG, CFSTR("_SCPOpen CFGetTypeID(): not a dictionary."));
				CFRelease(newSession->prefs);
				newSession->prefs = NULL;
				if (options & kSCPOpenCreatePrefs) {
					goto create_2;
				}
			scp_status = SCP_BADCF;
			goto error;
		}
	}

    create_2 :

	if (fd != -1) {
		(void) close(fd);
		fd = -1;
	}

	if (newSession->prefs == NULL) {
		/*
		 * new file, create empty preferences
		 */
		SCDLog(LOG_DEBUG, CFSTR("_SCPOpen(): creating new dictionary."));
		newSession->prefs = CFDictionaryCreateMutable(NULL,
							      0,
							      &kCFTypeDictionaryKeyCallBacks,
							      &kCFTypeDictionaryValueCallBacks);
		newSession->changed = TRUE;
	}

	/*
	 * all OK
	 */
	newSession->name		= CFRetain(name);
	if (prefsID) {
		newSession->prefsID	= CFRetain(prefsID);
	}
	newSession->perUser		= perUser;
	if (user) {
		newSession->user	= CFRetain(user);
	}
	*session   = (SCPSessionRef)newSession;
	return SCP_OK;

    error :

	if (fd != -1) {
		(void)close(fd);
	}
	(void) SCPClose((SCPSessionRef *)&newSession);
	return scp_status;
}


SCPStatus
SCPOpen(SCPSessionRef	*session,
	CFStringRef	name,
	CFStringRef	prefsID,
	int		options)
{
	return _SCPOpen(session, name, prefsID, FALSE, NULL, options);
}


SCPStatus
SCPUserOpen(SCPSessionRef	*session,
	    CFStringRef		name,
	    CFStringRef		prefsID,
	    CFStringRef		user,
	    int			options)
{
	return _SCPOpen(session, name, prefsID, TRUE, user, options);
}
