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
#include <SystemConfiguration/SCValidation.h>
#include "SCPreferencesInternal.h"

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/errno.h>

static CFStringRef
__SCPreferencesCopyDescription(CFTypeRef cf) {
	CFAllocatorRef		allocator	= CFGetAllocator(cf);
	CFMutableStringRef	result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCPreferences %p [%p]> {\n"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCPreferencesDeallocate(CFTypeRef cf)
{
	SCPreferencesPrivateRef	sessionPrivate	= (SCPreferencesPrivateRef)cf;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCPreferencesDeallocate:"));

	/* release resources */
	if (sessionPrivate->name)		CFRelease(sessionPrivate->name);
	if (sessionPrivate->prefsID)		CFRelease(sessionPrivate->prefsID);
	if (sessionPrivate->user)		CFRelease(sessionPrivate->user);
	if (sessionPrivate->path)		CFAllocatorDeallocate(NULL, sessionPrivate->path);
	if (sessionPrivate->signature)		CFRelease(sessionPrivate->signature);
	if (sessionPrivate->session)		CFRelease(sessionPrivate->session);
	if (sessionPrivate->sessionKeyLock)	CFRelease(sessionPrivate->sessionKeyLock);
	if (sessionPrivate->sessionKeyCommit)	CFRelease(sessionPrivate->sessionKeyCommit);
	if (sessionPrivate->sessionKeyApply)	CFRelease(sessionPrivate->sessionKeyApply);
	if (sessionPrivate->prefs)		CFRelease(sessionPrivate->prefs);

	return;
}


static CFTypeID __kSCPreferencesTypeID = _kCFRuntimeNotATypeID;


static const CFRuntimeClass __SCPreferencesClass = {
	0,					// version
	"SCPreferences",			// className
	NULL,					// init
	NULL,					// copy
	__SCPreferencesDeallocate,	// dealloc
	NULL,					// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__SCPreferencesCopyDescription	// copyDebugDesc
};


static pthread_once_t initialized	= PTHREAD_ONCE_INIT;


static void
__SCPreferencesInitialize(void) {
	__kSCPreferencesTypeID = _CFRuntimeRegisterClass(&__SCPreferencesClass);
	return;
}


SCPreferencesRef
__SCPreferencesCreatePrivate(CFAllocatorRef	allocator)
{
	SCPreferencesPrivateRef	prefs;
	UInt32				size;

	/* initialize runtime */
	pthread_once(&initialized, __SCPreferencesInitialize);

	/* allocate session */
	size  = sizeof(SCPreferencesPrivate) - sizeof(CFRuntimeBase);
	prefs = (SCPreferencesPrivateRef)_CFRuntimeCreateInstance(allocator,
									 __kSCPreferencesTypeID,
									 size,
									 NULL);
	if (!prefs) {
		return NULL;
	}

	prefs->name		= NULL;
	prefs->prefsID		= NULL;
	prefs->perUser		= FALSE;
	prefs->user		= NULL;
	prefs->path		= NULL;
	prefs->signature	= NULL;
	prefs->session		= NULL;
	prefs->sessionKeyLock	= NULL;
	prefs->sessionKeyCommit	= NULL;
	prefs->sessionKeyApply	= NULL;
	prefs->prefs		= NULL;
	prefs->accessed		= FALSE;
	prefs->changed		= FALSE;
	prefs->locked		= FALSE;
	prefs->isRoot		= (geteuid() == 0);

	return (SCPreferencesRef)prefs;
}


__private_extern__ SCPreferencesRef
__SCPreferencesCreate(CFAllocatorRef	allocator,
		      CFStringRef	name,
		      CFStringRef	prefsID,
		      Boolean		perUser,
		      CFStringRef	user)
{
	int				fd		= -1;
	SCPreferencesRef		prefs;
	SCPreferencesPrivateRef		prefsPrivate;
	int				sc_status	= kSCStatusOK;
	struct stat			statBuf;
	CFMutableDataRef		xmlData;
	CFStringRef			xmlError;

	/*
	 * allocate and initialize a new session
	 */
	prefs = __SCPreferencesCreatePrivate(allocator);
	if (!prefs) {
		return NULL;
	}
	prefsPrivate = (SCPreferencesPrivateRef)prefs;

	/*
	 * convert prefsID to path
	 */
	prefsPrivate->path = __SCPreferencesPath(NULL, prefsID, perUser, user);
	if (prefsPrivate->path == NULL) {
		sc_status = kSCStatusFailed;
		goto error;
	}

	/*
	 * open file
	 */
	fd = open(prefsPrivate->path, O_RDONLY, 0644);
	if (fd == -1) {
		switch (errno) {
			case ENOENT :
				/* no prefs file, start fresh */
				bzero(&statBuf, sizeof(statBuf));
				goto create_1;
			case EACCES :
				sc_status = kSCStatusAccessError;
				break;
			default :
				sc_status = kSCStatusFailed;
				break;
		}
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("open() failed: %s"), strerror(errno));
		goto error;
	}

	/*
	 * check file, create signature
	 */
	if (fstat(fd, &statBuf) == -1) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("fstat() failed: %s"), strerror(errno));
		sc_status = kSCStatusFailed;
		goto error;
	}

    create_1 :

	prefsPrivate->signature = __SCPSignatureFromStatbuf(&statBuf);

	if (statBuf.st_size > 0) {
		CFDictionaryRef	dict;

		/*
		 * extract property list
		 */
		xmlData = CFDataCreateMutable(NULL, statBuf.st_size);
		CFDataSetLength(xmlData, statBuf.st_size);
		if (read(fd, (void *)CFDataGetBytePtr(xmlData), statBuf.st_size) != statBuf.st_size) {
			/* corrupt prefs file, start fresh */
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("_SCPOpen read(): could not load preference data."));
			CFRelease(xmlData);
			xmlData = NULL;
			goto create_2;
		}

		/*
		 * load preferences
		 */
		dict = CFPropertyListCreateFromXMLData(NULL,
						       xmlData,
						       kCFPropertyListImmutable,
						       &xmlError);
		CFRelease(xmlData);
		if (!dict) {
			/* corrupt prefs file, start fresh */
			if (xmlError) {
				SCLog(TRUE, LOG_ERR,
				      CFSTR("_SCPOpen CFPropertyListCreateFromXMLData(): %@"),
				      xmlError);
				CFRelease(xmlError);
			}
			goto create_2;
		}

		/*
		 * make sure that we've got a dictionary
		 */
		if (!isA_CFDictionary(dict)) {
			/* corrupt prefs file, start fresh */
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("_SCPOpen CFGetTypeID(): not a dictionary."));
			CFRelease(dict);
			goto create_2;
		}

		prefsPrivate->prefs = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		CFRelease(dict);
	}

    create_2 :

	if (fd != -1) {
		(void) close(fd);
		fd = -1;
	}

	if (prefsPrivate->prefs == NULL) {
		/*
		 * new file, create empty preferences
		 */
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("_SCPOpen(): creating new dictionary."));
		prefsPrivate->prefs = CFDictionaryCreateMutable(NULL,
								0,
								&kCFTypeDictionaryKeyCallBacks,
								&kCFTypeDictionaryValueCallBacks);
		prefsPrivate->changed = TRUE;
	}

	/*
	 * all OK
	 */
	prefsPrivate->name = CFRetain(name);
	if (prefsID) {
		prefsPrivate->prefsID = CFRetain(prefsID);
	}
	prefsPrivate->perUser = perUser;
	if (user) {
		prefsPrivate->user = CFRetain(user);
	}
	return prefs;

    error :

	if (fd != -1) {
		(void) close(fd);
	}
	CFRelease(prefs);
	_SCErrorSet(sc_status);
	return NULL;
}


SCPreferencesRef
SCPreferencesCreate(CFAllocatorRef		allocator,
		    CFStringRef			name,
		    CFStringRef			prefsID)
{
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesCreate:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  name    = %@"), name);
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  prefsID = %@"), prefsID);

	return __SCPreferencesCreate(allocator, name, prefsID, FALSE, NULL);
}


SCPreferencesRef
SCUserPreferencesCreate(CFAllocatorRef			allocator,
			CFStringRef			name,
			CFStringRef			prefsID,
			CFStringRef			user)
{
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCUserPreferencesCreate:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  name    = %@"), name);
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  prefsID = %@"), prefsID);
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  user    = %@"), user);

	return __SCPreferencesCreate(allocator, name, prefsID, TRUE, user);
}


CFTypeID
SCPreferencesGetTypeID(void) {
	return __kSCPreferencesTypeID;
}
