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
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
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
	if (sessionPrivate->newPath)		CFAllocatorDeallocate(NULL, sessionPrivate->newPath);
	if (sessionPrivate->signature)		CFRelease(sessionPrivate->signature);
	if (sessionPrivate->session)		CFRelease(sessionPrivate->session);
	if (sessionPrivate->sessionKeyLock)	CFRelease(sessionPrivate->sessionKeyLock);
	if (sessionPrivate->sessionKeyCommit)	CFRelease(sessionPrivate->sessionKeyCommit);
	if (sessionPrivate->sessionKeyApply)	CFRelease(sessionPrivate->sessionKeyApply);
	if (sessionPrivate->prefs)		CFRelease(sessionPrivate->prefs);

	return;
}


static CFTypeID __kSCPreferencesTypeID	= _kCFRuntimeNotATypeID;


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


static SCPreferencesPrivateRef
__SCPreferencesCreatePrivate(CFAllocatorRef	allocator)
{
	SCPreferencesPrivateRef	prefsPrivate;
	uint32_t		size;

	/* initialize runtime */
	pthread_once(&initialized, __SCPreferencesInitialize);

	/* allocate session */
	size  = sizeof(SCPreferencesPrivate) - sizeof(CFRuntimeBase);
	prefsPrivate = (SCPreferencesPrivateRef)_CFRuntimeCreateInstance(allocator,
									 __kSCPreferencesTypeID,
									 size,
									 NULL);
	if (!prefsPrivate) {
		return NULL;
	}

	prefsPrivate->name		= NULL;
	prefsPrivate->prefsID		= NULL;
	prefsPrivate->perUser		= FALSE;
	prefsPrivate->user		= NULL;
	prefsPrivate->path		= NULL;
	prefsPrivate->newPath		= NULL;		// new prefs path
	prefsPrivate->signature		= NULL;
	prefsPrivate->session		= NULL;
	prefsPrivate->sessionKeyLock	= NULL;
	prefsPrivate->sessionKeyCommit	= NULL;
	prefsPrivate->sessionKeyApply	= NULL;
	prefsPrivate->prefs		= NULL;
	prefsPrivate->accessed		= FALSE;
	prefsPrivate->changed		= FALSE;
	prefsPrivate->locked		= FALSE;
	prefsPrivate->isRoot		= (geteuid() == 0);

	return prefsPrivate;
}


__private_extern__ SCPreferencesRef
__SCPreferencesCreate(CFAllocatorRef	allocator,
		      CFStringRef	name,
		      CFStringRef	prefsID,
		      Boolean		perUser,
		      CFStringRef	user)
{
	int				fd		= -1;
	SCPreferencesPrivateRef		prefsPrivate;
	int				sc_status	= kSCStatusOK;
	struct stat			statBuf;
	CFMutableDataRef		xmlData;
	CFStringRef			xmlError;

	/*
	 * allocate and initialize a new session
	 */
	prefsPrivate = __SCPreferencesCreatePrivate(allocator);
	if (!prefsPrivate) {
		return NULL;
	}

    retry :

	/*
	 * convert prefsID to path
	 */
	prefsPrivate->path = __SCPreferencesPath(allocator,
						 prefsID,
						 perUser,
						 user,
						 (prefsPrivate->newPath == NULL));
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
				/* no prefs file */
				if (!perUser &&
				    ((prefsID == NULL) || !CFStringHasPrefix(prefsID, CFSTR("/")))) {
					/* if default preference ID or relative path */
					if (prefsPrivate->newPath == NULL) {
						/*
						 * we've looked in the "new" prefs directory
						 * without success.  Save the "new" path and
						 * look in the "old" prefs directory.
						 */
						prefsPrivate->newPath = prefsPrivate->path;
						goto retry;
					} else {
						/*
						 * we've looked in both the "new" and "old"
						 * prefs directories without success.  USe
						 * the "new" path.
						 */
						CFAllocatorDeallocate(NULL, prefsPrivate->path);
						prefsPrivate->path = prefsPrivate->newPath;
						prefsPrivate->newPath = NULL;
					}
				}

				/* start fresh */
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
		xmlData = CFDataCreateMutable(allocator, statBuf.st_size);
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
		dict = CFPropertyListCreateFromXMLData(allocator,
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

		prefsPrivate->prefs = CFDictionaryCreateMutableCopy(allocator, 0, dict);
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
		prefsPrivate->prefs = CFDictionaryCreateMutable(allocator,
								0,
								&kCFTypeDictionaryKeyCallBacks,
								&kCFTypeDictionaryValueCallBacks);
		prefsPrivate->changed = TRUE;
	}

	/*
	 * all OK
	 */
	prefsPrivate->name = CFStringCreateCopy(allocator, name);
	if (prefsID)	prefsPrivate->prefsID = CFStringCreateCopy(allocator, prefsID);
	prefsPrivate->perUser = perUser;
	if (user)	prefsPrivate->user    = CFStringCreateCopy(allocator, user);
	return (SCPreferencesRef)prefsPrivate;

    error :

	if (fd != -1) 	(void) close(fd);
	CFRelease(prefsPrivate);
	_SCErrorSet(sc_status);
	return NULL;
}


SCPreferencesRef
SCPreferencesCreate(CFAllocatorRef		allocator,
		    CFStringRef			name,
		    CFStringRef			prefsID)
{
	if (_sc_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCPreferencesCreate:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  name    = %@"), name);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  prefsID = %@"), prefsID);
	}

	return __SCPreferencesCreate(allocator, name, prefsID, FALSE, NULL);
}


SCPreferencesRef
SCUserPreferencesCreate(CFAllocatorRef			allocator,
			CFStringRef			name,
			CFStringRef			prefsID,
			CFStringRef			user)
{
	if (_sc_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCUserPreferencesCreate:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  name    = %@"), name);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  prefsID = %@"), prefsID);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  user    = %@"), user);
	}

	return __SCPreferencesCreate(allocator, name, prefsID, TRUE, user);
}


CFTypeID
SCPreferencesGetTypeID(void) {
	pthread_once(&initialized, __SCPreferencesInitialize);	/* initialize runtime */
	return __kSCPreferencesTypeID;
}
