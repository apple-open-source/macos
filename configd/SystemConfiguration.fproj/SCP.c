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

#include <SystemConfiguration/SystemConfiguration.h>
#include "SCPPrivate.h"

#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/param.h>


static const struct scp_errmsg {
	SCPStatus	status;
	char		*message;
} scp_errmsgs[] = {
	{ SCP_OK,		"Success!" },
	{ SCP_BUSY,		"Configuration daemon busy" },
	{ SCP_NEEDLOCK,		"Lock required for this operation" },
	{ SCP_EACCESS,		"Permission denied (must be root to obtain lock)" },
	{ SCP_ENOENT,		"Configuration file not found" },
	{ SCP_BADCF,		"Configuration file corrupt" },
	{ SCP_NOKEY,		"No such key" },
	{ SCP_NOLINK,		"No such link" },
	{ SCP_EXISTS,		"Key already defined" },
	{ SCP_STALE,		"Write attempted on stale version of object" },
	{ SCP_INVALIDARGUMENT,	"Invalid argument" },
	{ SCP_FAILED,		"Failed!" }
};
#define nSCP_ERRMSGS (sizeof(scp_errmsgs)/sizeof(struct scp_errmsg))


__private_extern__ CFDataRef
_SCPSignatureFromStatbuf(const struct stat *statBuf)
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
_SCPPrefsPath(CFStringRef prefsID, boolean_t perUser, CFStringRef user)
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
				/* get current console user */
				if (SCDConsoleUserGet(login,
						      MAXLOGNAME,
						      NULL,
						      NULL) != SCD_OK) {
					/* if could not get console user */
					return NULL;
				}
			} else {
				/* use specified user */
				(void)CFStringGetBytes(user,
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
			path = CFStringCreateWithFormat(NULL,
							NULL,
							CFSTR("%s/%@/%@"),
							pwd->pw_dir,
							PREFS_DEFAULT_USER_DIR,
							prefsID);
		}
	} else {
		if (prefsID == NULL) {
			/* default preference ID */
			path = CFStringCreateWithFormat(NULL,
							NULL,
							CFSTR("%@/%@"),
							PREFS_DEFAULT_DIR,
							PREFS_DEFAULT_CONFIG);
		} else if (CFStringHasPrefix(prefsID, CFSTR("/"))) {
			/* if absolute path */
			path = CFRetain(prefsID);
		} else {
			/* relative path */
			path = CFStringCreateWithFormat(NULL,
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
	pathStr = CFAllocatorAllocate(NULL, pathLen, 0);
	if (!CFStringGetCString(path,
				pathStr,
				pathLen,
				kCFStringEncodingMacRoman)) {
		SCDLog(LOG_DEBUG, CFSTR("could not convert path to C string"));
		CFAllocatorDeallocate(NULL, pathStr);
		pathStr = NULL;
	}

	CFRelease(path);
	return pathStr;
}


SCPStatus
SCPGetSignature(SCPSessionRef session, CFDataRef *signature)
{
	SCPSessionPrivateRef	sessionPrivate;

	if (session == NULL) {
		return SCP_FAILED;           /* you can't do anything with a closed session */
	}
	sessionPrivate = (SCPSessionPrivateRef)session;

	*signature = sessionPrivate->signature;
	return SCP_OK;
}


__private_extern__ CFStringRef
_SCPNotificationKey(CFStringRef	prefsID,
		    boolean_t	perUser,
		    CFStringRef	user,
		    int		keyType)
{
	CFStringRef	key		= NULL;
	char		*pathStr;
	char		*typeStr;

	pathStr = _SCPPrefsPath(prefsID, perUser, user);
	if (pathStr == NULL) {
		return NULL;
	}

	/* create notification key */
	switch (keyType) {
		case kSCPKeyLock :
			typeStr = "lock";
			break;
		case kSCPKeyCommit :
			typeStr = "commit";
			break;
		case kSCPKeyApply :
			typeStr = "apply";
			break;
		default :
			typeStr = "?";
	}

	key = CFStringCreateWithFormat(NULL,
				       NULL,
				       CFSTR("%@%s:%s"),
				       kSCCacheDomainPrefs,
				       typeStr,
				       pathStr);

	CFAllocatorDeallocate(NULL, pathStr);
	return key;
}


CFStringRef
SCPNotificationKeyCreate(CFStringRef prefsID, int keyType)
{
	return _SCPNotificationKey(prefsID, FALSE, NULL, keyType);
}


CFStringRef
SCPUserNotificationKeyCreate(CFStringRef prefsID, CFStringRef user, int keyType)
{
	return _SCPNotificationKey(prefsID, TRUE, user, keyType);
}


const char *
SCPError(SCPStatus status)
{
	int i;

	for (i = 0; i < nSCP_ERRMSGS; i++) {
		if (scp_errmsgs[i].status == status) {
			return scp_errmsgs[i].message;
		}
	}
	return "(unknown error)";
}
