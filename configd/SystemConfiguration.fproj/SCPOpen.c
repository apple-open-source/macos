/*
 * Copyright(c) 2000-2007 Apple Inc. All rights reserved.
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
 * February 16, 2004		Allan Nathanson <ajn@apple.com>
 * - add preference notification APIs
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
#include "SCHelper_client.h"

#include <Security/Security.h>
#include "dy_framework.h"

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/errno.h>


static CFStringRef
__SCPreferencesCopyDescription(CFTypeRef cf) {
	CFAllocatorRef		allocator	= CFGetAllocator(cf);
	SCPreferencesPrivateRef prefsPrivate	= (SCPreferencesPrivateRef)cf;
	CFMutableStringRef	result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCPreferences %p [%p]> {"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("name = %@"), prefsPrivate->name);
	CFStringAppendFormat(result, NULL, CFSTR(", id = %@"), prefsPrivate->prefsID);
	CFStringAppendFormat(result, NULL, CFSTR(", path = %s"),
			     prefsPrivate->newPath ? prefsPrivate->newPath : prefsPrivate->path);
	if (prefsPrivate->accessed) {
		CFStringAppendFormat(result, NULL, CFSTR(", accessed"));
	}
	if (prefsPrivate->changed) {
		CFStringAppendFormat(result, NULL, CFSTR(", changed"));
	}
	if (prefsPrivate->locked) {
		CFStringAppendFormat(result, NULL, CFSTR(", locked"));
	}
	if (prefsPrivate->helper != -1) {
		CFStringAppendFormat(result, NULL, CFSTR(", helper fd=%d"), prefsPrivate->helper);
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCPreferencesDeallocate(CFTypeRef cf)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)cf;

	/* release resources */

	pthread_mutex_destroy(&prefsPrivate->lock);

	if (prefsPrivate->name)			CFRelease(prefsPrivate->name);
	if (prefsPrivate->prefsID)		CFRelease(prefsPrivate->prefsID);
	if (prefsPrivate->path)			CFAllocatorDeallocate(NULL, prefsPrivate->path);
	if (prefsPrivate->newPath)		CFAllocatorDeallocate(NULL, prefsPrivate->newPath);
	if (prefsPrivate->lockFD != -1)	{
		if (prefsPrivate->lockPath != NULL) {
			unlink(prefsPrivate->lockPath);
		}
		close(prefsPrivate->lockFD);
	}
	if (prefsPrivate->lockPath)		CFAllocatorDeallocate(NULL, prefsPrivate->lockPath);
	if (prefsPrivate->signature)		CFRelease(prefsPrivate->signature);
	if (prefsPrivate->session)		CFRelease(prefsPrivate->session);
	if (prefsPrivate->sessionKeyCommit)	CFRelease(prefsPrivate->sessionKeyCommit);
	if (prefsPrivate->sessionKeyApply)	CFRelease(prefsPrivate->sessionKeyApply);
	if (prefsPrivate->rlsContext.release != NULL) {
		(*prefsPrivate->rlsContext.release)(prefsPrivate->rlsContext.info);
	}
	if (prefsPrivate->prefs)		CFRelease(prefsPrivate->prefs);
	if (prefsPrivate->authorizationData != NULL) CFRelease(prefsPrivate->authorizationData);
	if (prefsPrivate->helper != -1) {
		(void) _SCHelperExec(prefsPrivate->helper, SCHELPER_MSG_PREFS_CLOSE, NULL, NULL, NULL);
		_SCHelperClose(prefsPrivate->helper);
	}

	return;
}


static CFTypeID __kSCPreferencesTypeID	= _kCFRuntimeNotATypeID;


static const CFRuntimeClass __SCPreferencesClass = {
	0,				// version
	"SCPreferences",		// className
	NULL,				// init
	NULL,				// copy
	__SCPreferencesDeallocate,	// dealloc
	NULL,				// equal
	NULL,				// hash
	NULL,				// copyFormattingDesc
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

	/* allocate prefs session */
	size  = sizeof(SCPreferencesPrivate) - sizeof(CFRuntimeBase);
	prefsPrivate = (SCPreferencesPrivateRef)_CFRuntimeCreateInstance(allocator,
									 __kSCPreferencesTypeID,
									 size,
									 NULL);
	if (prefsPrivate == NULL) {
		return NULL;
	}

	pthread_mutex_init(&prefsPrivate->lock, NULL);

	prefsPrivate->name				= NULL;
	prefsPrivate->prefsID				= NULL;
	prefsPrivate->path				= NULL;
	prefsPrivate->newPath				= NULL;		// new prefs path
	prefsPrivate->locked				= FALSE;
	prefsPrivate->lockFD				= -1;
	prefsPrivate->lockPath				= NULL;
	prefsPrivate->signature				= NULL;
	prefsPrivate->session				= NULL;
	prefsPrivate->sessionKeyCommit			= NULL;
	prefsPrivate->sessionKeyApply			= NULL;
	prefsPrivate->rls				= NULL;
	prefsPrivate->rlsFunction			= NULL;
	prefsPrivate->rlsContext.info			= NULL;
	prefsPrivate->rlsContext.retain			= NULL;
	prefsPrivate->rlsContext.release		= NULL;
	prefsPrivate->rlsContext.copyDescription	= NULL;
	prefsPrivate->rlList				= NULL;
	prefsPrivate->prefs				= NULL;
	prefsPrivate->accessed				= FALSE;
	prefsPrivate->changed				= FALSE;
	prefsPrivate->isRoot				= (geteuid() == 0);
	prefsPrivate->authorizationData			= NULL;
	prefsPrivate->helper				= -1;

	return prefsPrivate;
}


__private_extern__ Boolean
__SCPreferencesCreate_helper(SCPreferencesRef prefs)
{
	CFDataRef		data		= NULL;
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	uint32_t		status		= kSCStatusOK;

	// start helper
	prefsPrivate->helper = _SCHelperOpen(prefsPrivate->authorizationData);
	if (prefsPrivate->helper == -1) {
		goto fail;
	}

	// serialize the "prefsID"
	if (prefsPrivate->prefsID != NULL) {
		ok = _SCSerializeString(prefsPrivate->prefsID, &data, NULL, NULL);
		if (!ok) {
			goto fail;
		}
	}

	// have the helper "open" the prefs
	ok = _SCHelperExec(prefsPrivate->helper,
			   SCHELPER_MSG_PREFS_OPEN,
			   data,
			   &status,
			   NULL);
	if (data != NULL) CFRelease(data);
	if (!ok) {
		goto fail;
	}

	if (status != kSCStatusOK) {
		goto error;
	}

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
	_SCErrorSet(status);
	return FALSE;
}


static Boolean
__SCPreferencesAccess_helper(SCPreferencesRef prefs)
{
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	CFDictionaryRef		serverDict	= NULL;
	CFDictionaryRef		serverPrefs	= NULL;
	CFDictionaryRef		serverSignature	= NULL;
	uint32_t		status		= kSCStatusOK;
	CFDataRef		reply		= NULL;

	if (prefsPrivate->helper == -1) {
		ok = __SCPreferencesCreate_helper(prefs);
		if (!ok) {
			return FALSE;
		}
	}

	// have the helper "access" the prefs
	ok = _SCHelperExec(prefsPrivate->helper,
			   SCHELPER_MSG_PREFS_ACCESS,
			   NULL,
			   &status,
			   &reply);
	if (!ok) {
		goto fail;
	}

	if (status != kSCStatusOK) {
		goto error;
	}

	if (reply == NULL) {
		goto fail;
	}

	ok = _SCUnserialize((CFPropertyListRef *)&serverDict, reply, NULL, 0);
	CFRelease(reply);
	if (!ok) {
		goto fail;
	}

	if (isA_CFDictionary(serverDict)) {
		serverPrefs     = CFDictionaryGetValue(serverDict, CFSTR("preferences"));
		serverSignature = CFDictionaryGetValue(serverDict, CFSTR("signature"));
	}

	if (!isA_CFDictionary(serverPrefs) || !isA_CFData(serverSignature)) {
		CFRelease(serverDict);
		goto fail;
	}

	prefsPrivate->prefs     = CFDictionaryCreateMutableCopy(NULL, 0, serverPrefs);
	prefsPrivate->signature = CFRetain(serverSignature);
	prefsPrivate->accessed  = TRUE;
	CFRelease(serverDict);

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
	_SCErrorSet(status);
	return FALSE;
}


static SCPreferencesPrivateRef
__SCPreferencesCreate(CFAllocatorRef	allocator,
		      CFStringRef	name,
		      CFStringRef	prefsID,
		      CFDataRef		authorizationData)
{
	int				fd		= -1;
	SCPreferencesPrivateRef		prefsPrivate;
	int				sc_status	= kSCStatusOK;

	/*
	 * allocate and initialize a new prefs session
	 */
	prefsPrivate = __SCPreferencesCreatePrivate(allocator);
	if (prefsPrivate == NULL) {
		return NULL;
	}

	prefsPrivate->name = CFStringCreateCopy(allocator, name);
	if (prefsID != NULL) {
		prefsPrivate->prefsID = CFStringCreateCopy(allocator, prefsID);
	}
	if (authorizationData != NULL) {
		prefsPrivate->authorizationData = CFRetain(authorizationData);
	}

    retry :

	/*
	 * convert prefsID to path
	 */
	prefsPrivate->path = __SCPreferencesPath(allocator,
						 prefsID,
						 (prefsPrivate->newPath == NULL));
	if (prefsPrivate->path == NULL) {
		sc_status = kSCStatusFailed;
		goto error;
	}

	/*
	 * open file
	 */
	fd = open(prefsPrivate->path, O_RDONLY, 0644);
	if (fd != -1) {
		(void) close(fd);
	} else {
		switch (errno) {
			case ENOENT :
				/* no prefs file */
				if ((prefsID == NULL) || !CFStringHasPrefix(prefsID, CFSTR("/"))) {
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
						 * prefs directories without success.  Use
						 * the "new" path.
						 */
						CFAllocatorDeallocate(NULL, prefsPrivate->path);
						prefsPrivate->path = prefsPrivate->newPath;
						prefsPrivate->newPath = NULL;
					}
				}

				/* no preference data, start fresh */
				sc_status = kSCStatusNoConfigFile;
				goto done;
			case EACCES :
				if (prefsPrivate->authorizationData != NULL) {
					/* no problem, we'll be using the helper */
					goto done;
				}

				sc_status = kSCStatusAccessError;
				break;
			default :
				sc_status = kSCStatusFailed;
				break;
		}
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCPreferencesCreate open() failed: %s"), strerror(errno));
		goto error;
	}

    done :

	/* all OK */
	_SCErrorSet(sc_status);
	return prefsPrivate;

    error :

	if (fd != -1) (void) close(fd);
	CFRelease(prefsPrivate);
	_SCErrorSet(sc_status);
	return NULL;
}


__private_extern__ void
__SCPreferencesAccess(SCPreferencesRef	prefs)
{
	CFAllocatorRef			allocator	= CFGetAllocator(prefs);
	int				fd		= -1;
	SCPreferencesPrivateRef		prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	struct  stat			statBuf;

	if (prefsPrivate->accessed) {
		// if preference data has already been accessed
		return;
	}

	fd = open(prefsPrivate->path, O_RDONLY, 0644);
	if (fd != -1) {
		// create signature
		if (fstat(fd, &statBuf) == -1) {
			SCLog(TRUE, LOG_ERR, CFSTR("__SCPreferencesAccess fstat() failed: %s"), strerror(errno));
			bzero(&statBuf, sizeof(statBuf));
		}
	} else {
		switch (errno) {
			case ENOENT :
				/* no preference data, start fresh */
				break;
			case EACCES :
				if (prefsPrivate->authorizationData != NULL) {
					if (__SCPreferencesAccess_helper(prefs)) {
						goto done;
					} else {
						SCLog(TRUE, LOG_ERR,
						      CFSTR("__SCPreferencesAccess_helper() failed: %s"),
						      SCErrorString(SCError()));
					}
					break;
				}
				// fall through
			default :
				SCLog(TRUE, LOG_ERR, CFSTR("__SCPreferencesAccess open() failed: %s"), strerror(errno));
				break;
		}
		bzero(&statBuf, sizeof(statBuf));
	}

	if (prefsPrivate->signature != NULL) CFRelease(prefsPrivate->signature);
	prefsPrivate->signature = __SCPSignatureFromStatbuf(&statBuf);

	if (statBuf.st_size > 0) {
		CFDictionaryRef		dict;
		CFMutableDataRef	xmlData;
		CFStringRef		xmlError;

		/*
		 * extract property list
		 */
		xmlData = CFDataCreateMutable(allocator, statBuf.st_size);
		CFDataSetLength(xmlData, statBuf.st_size);
		if (read(fd, (void *)CFDataGetBytePtr(xmlData), statBuf.st_size) != statBuf.st_size) {
			/* corrupt prefs file, start fresh */
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCPreferencesAccess read(): could not load preference data."));
			CFRelease(xmlData);
			xmlData = NULL;
			goto done;
		}

		/*
		 * load preferences
		 */
		dict = CFPropertyListCreateFromXMLData(allocator,
						       xmlData,
						       kCFPropertyListImmutable,
						       &xmlError);
		CFRelease(xmlData);
		if (dict == NULL) {
			/* corrupt prefs file, start fresh */
			if (xmlError != NULL) {
				SCLog(TRUE, LOG_ERR,
				      CFSTR("__SCPreferencesAccess CFPropertyListCreateFromXMLData(): %@"),
				      xmlError);
				CFRelease(xmlError);
			}
			goto done;
		}

		/*
		 * make sure that we've got a dictionary
		 */
		if (!isA_CFDictionary(dict)) {
			/* corrupt prefs file, start fresh */
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCPreferencesAccess CFGetTypeID(): not a dictionary."));
			CFRelease(dict);
			goto done;
		}

		prefsPrivate->prefs = CFDictionaryCreateMutableCopy(allocator, 0, dict);
		CFRelease(dict);
	}

    done :

	if (fd != -1) {
		(void) close(fd);
		fd = -1;
	}

	if (prefsPrivate->prefs == NULL) {
		/*
		 * new file, create empty preferences
		 */
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCPreferencesAccess(): creating new preferences file."));
		prefsPrivate->prefs = CFDictionaryCreateMutable(allocator,
								0,
								&kCFTypeDictionaryKeyCallBacks,
								&kCFTypeDictionaryValueCallBacks);
		prefsPrivate->changed = TRUE;
	}

	prefsPrivate->accessed = TRUE;
	return;
}


SCPreferencesRef
SCPreferencesCreate(CFAllocatorRef		allocator,
		    CFStringRef			name,
		    CFStringRef			prefsID)
{
	SCPreferencesPrivateRef	prefsPrivate;

	prefsPrivate = __SCPreferencesCreate(allocator, name, prefsID, NULL);
	return (SCPreferencesRef)prefsPrivate;
}


SCPreferencesRef
SCPreferencesCreateWithAuthorization(CFAllocatorRef	allocator,
				     CFStringRef	name,
				     CFStringRef	prefsID,
				     AuthorizationRef	authorization)
{
	CFDataRef			authorizationData;
	AuthorizationExternalForm	extForm;
	OSStatus			os_status;
	SCPreferencesPrivateRef		prefsPrivate;

	os_status = AuthorizationMakeExternalForm(authorization, &extForm);
	if (os_status != errAuthorizationSuccess) {
		SCLog(TRUE, LOG_INFO, CFSTR("_SCHelperOpen AuthorizationMakeExternalForm() failed"));
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	authorizationData = CFDataCreate(NULL, (const UInt8 *)extForm.bytes, sizeof(extForm.bytes));
	prefsPrivate = __SCPreferencesCreate(allocator, name, prefsID, authorizationData);
	CFRelease(authorizationData);

	return (SCPreferencesRef)prefsPrivate;
}


CFTypeID
SCPreferencesGetTypeID(void) {
	pthread_once(&initialized, __SCPreferencesInitialize);	/* initialize runtime */
	return __kSCPreferencesTypeID;
}


static void
prefsNotify(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
	void				*context_info;
	void				(*context_release)(const void *);
	CFIndex				i;
	CFIndex				n;
	SCPreferencesNotification       notify		= 0;
	SCPreferencesRef		prefs		= (SCPreferencesRef)info;
	SCPreferencesPrivateRef		prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	SCPreferencesCallBack		rlsFunction;

	n = (changedKeys != NULL) ? CFArrayGetCount(changedKeys) : 0;
	for (i = 0; i < n; i++) {
		CFStringRef     key;

		key = CFArrayGetValueAtIndex(changedKeys, i);
		if (CFEqual(key, prefsPrivate->sessionKeyCommit)) {
			// if preferences have been saved
			notify |= kSCPreferencesNotificationCommit;
		}
		if (CFEqual(key, prefsPrivate->sessionKeyApply)) {
			// if stored preferences should be applied to current configuration
			notify |= kSCPreferencesNotificationApply;
		}
	}

	if (notify == 0) {
		// if no changes
		return;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	/* callout */
	rlsFunction = prefsPrivate->rlsFunction;
	if (prefsPrivate->rlsContext.retain != NULL) {
		context_info	= (void *)prefsPrivate->rlsContext.retain(prefsPrivate->rlsContext.info);
		context_release	= prefsPrivate->rlsContext.release;
	} else {
		context_info	= prefsPrivate->rlsContext.info;
		context_release	= NULL;
	}

	pthread_mutex_unlock(&prefsPrivate->lock);

	if (rlsFunction != NULL) {
		(*rlsFunction)(prefs, notify, context_info);
	}

	if (context_release != NULL) {
		(*context_release)(context_info);
	}

	return;
}


__private_extern__ Boolean
__SCPreferencesAddSession(SCPreferencesRef prefs)
{
	CFAllocatorRef			allocator	= CFGetAllocator(prefs);
	SCDynamicStoreContext		context		= { 0
							  , (void *)prefs
							  , NULL
							  , NULL
							  , NULL
							  };
	SCPreferencesPrivateRef		prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	/* establish a dynamic store session */
	prefsPrivate->session = SCDynamicStoreCreate(allocator,
						     CFSTR("SCPreferences"),
						     prefsNotify,
						     &context);
	if (prefsPrivate->session == NULL) {
		SCLog(_sc_verbose, LOG_INFO, CFSTR("__SCPreferencesAddSession SCDynamicStoreCreate() failed"));
		return FALSE;
	}

	/* create the session "commit" key */
	prefsPrivate->sessionKeyCommit = _SCPNotificationKey(NULL,
							     prefsPrivate->prefsID,
							     kSCPreferencesKeyCommit);

	/* create the session "apply" key */
	prefsPrivate->sessionKeyApply = _SCPNotificationKey(NULL,
							    prefsPrivate->prefsID,
							    kSCPreferencesKeyApply);

	return TRUE;
}


Boolean
SCPreferencesSetCallback(SCPreferencesRef       prefs,
			 SCPreferencesCallBack  callout,
			 SCPreferencesContext   *context)
{
	SCPreferencesPrivateRef	prefsPrivate = (SCPreferencesPrivateRef)prefs;

	if (prefs == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	if (prefsPrivate->rlsContext.release != NULL) {
		/* let go of the current context */
		(*prefsPrivate->rlsContext.release)(prefsPrivate->rlsContext.info);
	}

	prefsPrivate->rlsFunction   			= callout;
	prefsPrivate->rlsContext.info			= NULL;
	prefsPrivate->rlsContext.retain			= NULL;
	prefsPrivate->rlsContext.release		= NULL;
	prefsPrivate->rlsContext.copyDescription	= NULL;
	if (context != NULL) {
		bcopy(context, &prefsPrivate->rlsContext, sizeof(SCPreferencesContext));
		if (context->retain != NULL) {
			prefsPrivate->rlsContext.info = (void *)(*context->retain)(context->info);
		}
	}

	pthread_mutex_unlock(&prefsPrivate->lock);

	return TRUE;
}


Boolean
SCPreferencesScheduleWithRunLoop(SCPreferencesRef       prefs,
				 CFRunLoopRef		runLoop,
				 CFStringRef		runLoopMode)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	if (prefs == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	if (prefsPrivate->rls == NULL) {
		CFMutableArrayRef       keys;

		if (prefsPrivate->session == NULL) {
			__SCPreferencesAddSession(prefs);
		}

		CFRetain(prefs);	// hold a reference to the prefs

		keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(keys, prefsPrivate->sessionKeyCommit);
		CFArrayAppendValue(keys, prefsPrivate->sessionKeyApply);
		(void) SCDynamicStoreSetNotificationKeys(prefsPrivate->session, keys, NULL);
		CFRelease(keys);

		prefsPrivate->rls    = SCDynamicStoreCreateRunLoopSource(NULL, prefsPrivate->session, 0);
		prefsPrivate->rlList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	if (!_SC_isScheduled(NULL, runLoop, runLoopMode, prefsPrivate->rlList)) {
		/*
		 * if we do not already have notifications scheduled with
		 * this runLoop / runLoopMode
		 */
		CFRunLoopAddSource(runLoop, prefsPrivate->rls, runLoopMode);
	}

	_SC_schedule(prefs, runLoop, runLoopMode, prefsPrivate->rlList);

	pthread_mutex_unlock(&prefsPrivate->lock);
	return TRUE;
}


Boolean
SCPreferencesUnscheduleFromRunLoop(SCPreferencesRef     prefs,
				   CFRunLoopRef		runLoop,
				   CFStringRef		runLoopMode)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	CFIndex			n;
	Boolean			ok		= FALSE;

	if (prefs == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	if (prefsPrivate->rls == NULL) {
		/* if not currently scheduled */
		goto done;
	}

	if (!_SC_unschedule(NULL, runLoop, runLoopMode, prefsPrivate->rlList, FALSE)) {
		/* if not currently scheduled */
		goto done;
	}

	n = CFArrayGetCount(prefsPrivate->rlList);
	if (n == 0 || !_SC_isScheduled(NULL, runLoop, runLoopMode, prefsPrivate->rlList)) {
		/*
		 * if we are no longer scheduled to receive notifications for
		 * this runLoop / runLoopMode
		 */
		CFRunLoopRemoveSource(runLoop, prefsPrivate->rls, runLoopMode);

		if (n == 0) {
			CFArrayRef      changedKeys;

			/*
			 * if *all* notifications have been unscheduled
			 */
			CFRunLoopSourceInvalidate(prefsPrivate->rls);
			CFRelease(prefsPrivate->rls);
			prefsPrivate->rls = NULL;
			CFRelease(prefsPrivate->rlList);
			prefsPrivate->rlList = NULL;

			CFRelease(prefs);	// release our reference to the prefs

			// no need to track changes
			(void) SCDynamicStoreSetNotificationKeys(prefsPrivate->session, NULL, NULL);

			// clear out any pending notifications
			changedKeys = SCDynamicStoreCopyNotifiedKeys(prefsPrivate->session);
			if (changedKeys != NULL) {
				CFRelease(changedKeys);
			}
		}
	}

	ok = TRUE;

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);
	return ok;
}


static void
__SCPreferencesSynchronize_helper(SCPreferencesRef prefs)
{
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	uint32_t		status		= kSCStatusOK;
	
	if (prefsPrivate->helper == -1) {
		// if no helper
		return;
	}
	
	// have the helper "synchronize" the prefs
	ok = _SCHelperExec(prefsPrivate->helper,
			   SCHELPER_MSG_PREFS_SYNCHRONIZE,
			   NULL,
			   &status,
			   NULL);
	if (!ok) {
		// close helper
		if (prefsPrivate->helper != -1) {
			_SCHelperClose(prefsPrivate->helper);
			prefsPrivate->helper = -1;
		}
	}
	
	return;
}


void
SCPreferencesSynchronize(SCPreferencesRef       prefs)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	if (prefs == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return;
	}

	if (prefsPrivate->authorizationData != NULL) {
		__SCPreferencesSynchronize_helper(prefs);
	}

	if (prefsPrivate->prefs != NULL) {
		CFRelease(prefsPrivate->prefs);
		prefsPrivate->prefs = NULL;
	}
	if (prefsPrivate->signature != NULL) {
		CFRelease(prefsPrivate->signature);
		prefsPrivate->signature = NULL;
	}
	prefsPrivate->accessed = FALSE;
	prefsPrivate->changed  = FALSE;
	return;
}
