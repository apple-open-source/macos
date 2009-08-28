/*
 * Copyright (c) 2005-2009 Apple Inc. All rights reserved.
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

#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <pthread.h>
#include <sysexits.h>

#include "SCPreferencesInternal.h"
#include "SCHelper_client.h"
#include "helper_comm.h"


#if	TARGET_OS_IPHONE
#include <grp.h>

__private_extern__ int
getgrnam_r(const char *name, __unused struct group *grp, __unused char *buf, __unused size_t bufsize, struct group **grpP)
{
	*grpP = getgrnam(name);
	return (*grpP == NULL) ? -1 : 0;
}
#endif	// TARGET_OS_IPHONE


#pragma mark -
#pragma mark Session managment


typedef const struct __SCHelperSession * SCHelperSessionRef;

typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// authorization
	AuthorizationRef	authorization;
#if	TARGET_OS_IPHONE
	uid_t			peer_euid;
	gid_t			peer_egid;
#endif	// TARGET_OS_IPHONE

	// preferences
	SCPreferencesRef	prefs;

} SCHelperSessionPrivate, *SCHelperSessionPrivateRef;


static AuthorizationRef
__SCHelperSessionGetAuthorization(SCHelperSessionRef session)
{
	SCHelperSessionPrivateRef	sessionPrivate	= (SCHelperSessionPrivateRef)session;

	return sessionPrivate->authorization;
}


static Boolean
__SCHelperSessionSetAuthorization(SCHelperSessionRef session, CFTypeRef authorizationData)
{
	Boolean				ok		= TRUE;
	SCHelperSessionPrivateRef	sessionPrivate	= (SCHelperSessionPrivateRef)session;

#if	!TARGET_OS_IPHONE
	if (sessionPrivate->authorization != NULL) {
		AuthorizationFree(sessionPrivate->authorization, kAuthorizationFlagDefaults);
//		AuthorizationFree(sessionPrivate->authorization, kAuthorizationFlagDestroyRights);
		sessionPrivate->authorization = NULL;
	}

	if (isA_CFData(authorizationData)) {
		AuthorizationExternalForm	extForm;

		if (CFDataGetLength(authorizationData) == sizeof(extForm.bytes)) {
			OSStatus	err;

			bcopy(CFDataGetBytePtr(authorizationData), extForm.bytes, sizeof(extForm.bytes));
			err = AuthorizationCreateFromExternalForm(&extForm,
								  &sessionPrivate->authorization);
			if (err != errAuthorizationSuccess) {
				SCLog(TRUE, LOG_ERR,
				      CFSTR("AuthorizationCreateFromExternalForm() failed: status = %d"),
				      (int)err);
				sessionPrivate->authorization = NULL;
				ok = FALSE;
			}
		}
	}
#else	// !TARGET_OS_IPHONE
	if (sessionPrivate->authorization != NULL) {
		CFRelease(sessionPrivate->authorization);
		sessionPrivate->authorization = NULL;
	}

	if (isA_CFString(authorizationData)) {
		sessionPrivate->authorization = (void *)CFRetain(authorizationData);
	}
#endif	// !TARGET_OS_IPHONE

	return ok;
}


#if	TARGET_OS_IPHONE
static void
__SCHelperSessionGetCredentials(SCHelperSessionRef session, uid_t *euid, gid_t *egid)
{
	SCHelperSessionPrivateRef	sessionPrivate	= (SCHelperSessionPrivateRef)session;

	if (euid != NULL) *euid = sessionPrivate->peer_euid;
	if (egid != NULL) *egid = sessionPrivate->peer_egid;
	return;
}


static Boolean
__SCHelperSessionSetCredentials(SCHelperSessionRef session, uid_t euid, gid_t egid)
{
	SCHelperSessionPrivateRef	sessionPrivate	= (SCHelperSessionPrivateRef)session;

	sessionPrivate->peer_euid = euid;
	sessionPrivate->peer_egid = egid;
	return TRUE;
}
#endif	// TARGET_OS_IPHONE

static SCPreferencesRef
__SCHelperSessionGetPreferences(SCHelperSessionRef session)
{
	SCHelperSessionPrivateRef	sessionPrivate	= (SCHelperSessionPrivateRef)session;

	return sessionPrivate->prefs;
}


static Boolean
__SCHelperSessionSetPreferences(SCHelperSessionRef session, SCPreferencesRef prefs)
{
	SCHelperSessionPrivateRef	sessionPrivate	= (SCHelperSessionPrivateRef)session;

	if (prefs != NULL) {
		CFRetain(prefs);
	}
	if (sessionPrivate->prefs != NULL) {
		CFRelease(sessionPrivate->prefs);
	}
	sessionPrivate->prefs = prefs;

	return TRUE;
}


static CFStringRef	__SCHelperSessionCopyDescription	(CFTypeRef cf);
static void		__SCHelperSessionDeallocate		(CFTypeRef cf);


static CFTypeID		__kSCHelperSessionTypeID	= _kCFRuntimeNotATypeID;
static Boolean		debug				= FALSE;
static pthread_once_t	initialized			= PTHREAD_ONCE_INIT;
static CFRunLoopRef	main_runLoop			= NULL;
static CFMutableSetRef	sessions			= NULL;
static int		sessions_closed			= 0;	// count of sessions recently closed
static pthread_mutex_t	sessions_lock			= PTHREAD_MUTEX_INITIALIZER;


static const CFRuntimeClass __SCHelperSessionClass = {
	0,					// version
	"SCHelperSession",			// className
	NULL,					// init
	NULL,					// copy
	__SCHelperSessionDeallocate,		// dealloc
	NULL,					// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__SCHelperSessionCopyDescription	// copyDebugDesc
};


static CFStringRef
__SCHelperSessionCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	CFMutableStringRef		result;
	SCHelperSessionPrivateRef	sessionPrivate	= (SCHelperSessionPrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCHelperSession %p [%p]> {"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("authorization = %p"), sessionPrivate->authorization);
	CFStringAppendFormat(result, NULL, CFSTR(", prefs = %p"), sessionPrivate->prefs);
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCHelperSessionDeallocate(CFTypeRef cf)
{
	SCHelperSessionPrivateRef	sessionPrivate	= (SCHelperSessionPrivateRef)cf;

	// release resources
	__SCHelperSessionSetAuthorization((SCHelperSessionRef)sessionPrivate, NULL);
	__SCHelperSessionSetPreferences  ((SCHelperSessionRef)sessionPrivate, NULL);

	// we no longer need/want to track this session
	pthread_mutex_lock(&sessions_lock);
	CFSetRemoveValue(sessions, sessionPrivate);
	sessions_closed++;
	pthread_mutex_unlock(&sessions_lock);
	CFRunLoopWakeUp(main_runLoop);

	return;
}


static void
__SCHelperSessionInitialize(void)
{
	__kSCHelperSessionTypeID = _CFRuntimeRegisterClass(&__SCHelperSessionClass);
	return;
}


static SCHelperSessionRef
__SCHelperSessionCreate(CFAllocatorRef allocator)
{
	SCHelperSessionPrivateRef	sessionPrivate;
	uint32_t			size;

	/* initialize runtime */
	pthread_once(&initialized, __SCHelperSessionInitialize);

	/* allocate session */
	size           = sizeof(SCHelperSessionPrivate) - sizeof(CFRuntimeBase);
	sessionPrivate = (SCHelperSessionPrivateRef)_CFRuntimeCreateInstance(allocator,
									    __kSCHelperSessionTypeID,
									    size,
									    NULL);
	if (sessionPrivate == NULL) {
		return NULL;
	}

	sessionPrivate->authorization	= NULL;
#if	TARGET_OS_IPHONE
	sessionPrivate->peer_euid	= 0;
	sessionPrivate->peer_egid	= 0;
#endif	// TARGET_OS_IPHONE
	sessionPrivate->prefs		= NULL;

	// keep track this session
	pthread_mutex_lock(&sessions_lock);
	if (sessions == NULL) {
		sessions = CFSetCreateMutable(NULL, 0, NULL);	// create a non-retaining set
	}
	CFSetAddValue(sessions, sessionPrivate);
	pthread_mutex_unlock(&sessions_lock);

	return (SCHelperSessionRef)sessionPrivate;
}


#pragma mark -
#pragma mark Helpers


/*
 * EXIT
 *   (in)  data   = N/A
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_Exit(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	*status = -1;
	return FALSE;
}


/*
 * AUTHORIZE
 *   (in)  data   = AuthorizationExternalForm
 *   (out) status = OSStatus
 *   (out) reply  = N/A
 */
static Boolean
do_Auth(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean	ok;

#if	!TARGET_OS_IPHONE

	ok = __SCHelperSessionSetAuthorization(session, data);

#else	//!TARGET_OS_IPHONE

	CFStringRef	authorizationInfo	= NULL;

	if ((data != NULL) && !_SCUnserializeString(&authorizationInfo, data, NULL, 0)) {
		return FALSE;
	}

	if (!isA_CFString(authorizationInfo)) {
		if (authorizationInfo != NULL) CFRelease(authorizationInfo);
		return FALSE;
	}

	ok = __SCHelperSessionSetAuthorization(session, authorizationInfo);
	if (authorizationInfo != NULL) CFRelease(authorizationInfo);

#endif	// !TARGET_OS_IPHONE

	*status = ok ? 0 : 1;
	return TRUE;
}


#if	!TARGET_OS_IPHONE


/*
 * SCHELPER_MSG_KEYCHAIN_COPY
 *   (in)  data   = unique_id
 *   (out) status = SCError()
 *   (out) reply  = password
 */
static Boolean
do_keychain_copy(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	SCPreferencesRef	prefs;
	CFStringRef		unique_id	= NULL;

	if ((data != NULL) && !_SCUnserializeString(&unique_id, data, NULL, 0)) {
		return FALSE;
	}

	if (!isA_CFString(unique_id)) {
		return FALSE;
	}

	prefs = __SCHelperSessionGetPreferences(session);
	*reply = _SCPreferencesSystemKeychainPasswordItemCopy(prefs, unique_id);
	CFRelease(unique_id);
	if (*reply == NULL) {
		*status = SCError();
	}

	return TRUE;
}


/*
 * SCHELPER_MSG_KEYCHAIN_EXISTS
 *   (in)  data   = unique_id
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_keychain_exists(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean			ok;
	SCPreferencesRef	prefs;
	CFStringRef		unique_id	= NULL;

	if ((data != NULL) && !_SCUnserializeString(&unique_id, data, NULL, 0)) {
		return FALSE;
	}

	if (!isA_CFString(unique_id)) {
		if (unique_id != NULL) CFRelease(unique_id);
		return FALSE;
	}

	prefs = __SCHelperSessionGetPreferences(session);
	ok = _SCPreferencesSystemKeychainPasswordItemExists(prefs, unique_id);
	CFRelease(unique_id);
	if (!ok) {
		*status = SCError();
	}

	return TRUE;
}


/*
 * SCHELPER_MSG_KEYCHAIN_REMOVE
 *   (in)  data   = unique_id
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_keychain_remove(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean			ok;
	SCPreferencesRef	prefs;
	CFStringRef		unique_id	= NULL;

	if ((data != NULL) && !_SCUnserializeString(&unique_id, data, NULL, 0)) {
		return FALSE;
	}

	if (!isA_CFString(unique_id)) {
		if (unique_id != NULL) CFRelease(unique_id);
		return FALSE;
	}

	prefs = __SCHelperSessionGetPreferences(session);
	ok = _SCPreferencesSystemKeychainPasswordItemRemove(prefs, unique_id);
	CFRelease(unique_id);
	if (!ok) {
		*status = SCError();
	}

	return TRUE;
}


/*
 * SCHELPER_MSG_KEYCHAIN_SET
 *   (in)  data   = options dictionary
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_keychain_set(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	CFStringRef		account;
	CFStringRef		description;
	CFArrayRef		executablePaths	= NULL;
	CFStringRef		label;
	Boolean			ok;
	CFDictionaryRef		options		= NULL;
	CFDataRef		password;
	SCPreferencesRef	prefs;
	CFStringRef		unique_id;

	if ((data != NULL) && !_SCUnserialize((CFPropertyListRef *)&options, data, NULL, 0)) {
		return FALSE;
	}

	if (!isA_CFDictionary(options)) {
		if (options != NULL) CFRelease(options);
		return FALSE;
	}

	if (CFDictionaryGetValueIfPresent(options,
					  kSCKeychainOptionsAllowedExecutables,
					  (const void **)&executablePaths)) {
		CFMutableArrayRef	executableURLs;
		CFIndex			i;
		CFIndex			n;
		CFMutableDictionaryRef	newOptions;

		executableURLs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		n = CFArrayGetCount(executablePaths);
		for (i = 0; i < n; i++) {
			CFDataRef	path;
			CFURLRef	url;

			path = CFArrayGetValueAtIndex(executablePaths, i);
			url  = CFURLCreateFromFileSystemRepresentation(NULL,
								       CFDataGetBytePtr(path),
								       CFDataGetLength(path),
								       FALSE);
			if (url != NULL) {
				CFArrayAppendValue(executableURLs, url);
				CFRelease(url);
			}
		}

		newOptions = CFDictionaryCreateMutableCopy(NULL, 0, options);
		CFDictionarySetValue(newOptions, kSCKeychainOptionsAllowedExecutables, executableURLs);
		CFRelease(executableURLs);

		CFRelease(options);
		options = newOptions;
	}

	unique_id   = CFDictionaryGetValue(options, kSCKeychainOptionsUniqueID);
	label       = CFDictionaryGetValue(options, kSCKeychainOptionsLabel);
	description = CFDictionaryGetValue(options, kSCKeychainOptionsDescription);
	account     = CFDictionaryGetValue(options, kSCKeychainOptionsAccount);
	password    = CFDictionaryGetValue(options, kSCKeychainOptionsPassword);

	prefs = __SCHelperSessionGetPreferences(session);
	ok = _SCPreferencesSystemKeychainPasswordItemSet(prefs,
							 unique_id,
							 label,
							 description,
							 account,
							 password,
							 options);
	CFRelease(options);
	if (!ok) {
		*status = SCError();
	}

	return TRUE;
}


#endif	// !TARGET_OS_IPHONE


/*
 * SCHELPER_MSG_INTERFACE_REFRESH
 *   (in)  data   = ifName
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_interface_refresh(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	CFStringRef	ifName	= NULL;
	Boolean		ok;

	if ((data != NULL) && !_SCUnserializeString(&ifName, data, NULL, 0)) {
		SCLog(TRUE, LOG_ERR, CFSTR("interface name not valid"));
		return FALSE;
	}

	if (!isA_CFString(ifName)) {
		SCLog(TRUE, LOG_ERR, CFSTR("interface name not valid"));
		if (ifName != NULL) CFRelease(ifName);
		return FALSE;
	}

	ok = _SCNetworkInterfaceForceConfigurationRefresh(ifName);
	CFRelease(ifName);
	if (!ok) {
		*status = SCError();
	}

	return TRUE;
}


/*
 * OPEN
 *   (in)  data   = prefsID
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_prefs_Open(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	CFStringRef		name;
	CFNumberRef		pid;
	SCPreferencesRef	prefs		= __SCHelperSessionGetPreferences(session);
	CFDictionaryRef		prefsInfo	= NULL;
	CFStringRef		prefsID;
	CFStringRef		prefsName;

	if (prefs != NULL) {
		return FALSE;
	}

	if ((data != NULL) && !_SCUnserialize((CFPropertyListRef *)&prefsInfo, data, NULL, 0)) {
		SCLog(TRUE, LOG_ERR, CFSTR("data not valid, %@"), data);
		return FALSE;
	}

	if ((prefsInfo == NULL) || !isA_CFDictionary(prefsInfo)) {
		SCLog(TRUE, LOG_ERR, CFSTR("info not valid"));
		if (prefsInfo != NULL) CFRelease(prefsInfo);
		return FALSE;
	}

	// get [optional] prefsID
	prefsID = CFDictionaryGetValue(prefsInfo, CFSTR("prefsID"));
	prefsID = isA_CFString(prefsID);
	if (prefsID != NULL) {
		if (CFStringHasPrefix(prefsID, CFSTR("/")) ||
		    CFStringHasPrefix(prefsID, CFSTR("../")) ||
		    CFStringHasSuffix(prefsID, CFSTR("/..")) ||
		    (CFStringFind(prefsID, CFSTR("/../"), 0).location != kCFNotFound)) {
			// if we're trying to escape from the preferences directory
			SCLog(TRUE, LOG_ERR, CFSTR("prefsID (%@) not valid"), prefsID);
			CFRelease(prefsInfo);
			*status = kSCStatusInvalidArgument;
			return TRUE;
		}
	}

	// get preferences session "name"
	name = CFDictionaryGetValue(prefsInfo, CFSTR("name"));
	if (!isA_CFString(name)) {
		SCLog(TRUE, LOG_ERR, CFSTR("session \"name\" not valid"));
		CFRelease(prefsInfo);
		return FALSE;
	}

	// get PID of caller
	pid = CFDictionaryGetValue(prefsInfo, CFSTR("PID"));
	if (!isA_CFNumber(pid)) {
		SCLog(TRUE, LOG_ERR, CFSTR("PID not valid"));
		CFRelease(prefsInfo);
		return FALSE;
	}

	// build [helper] preferences "name" (used for debugging) and estabish
	// a preferences session.
	prefsName = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@:%@"), pid, name);
	prefs = SCPreferencesCreate(NULL, prefsName, prefsID);
	CFRelease(prefsName);
	CFRelease(prefsInfo);

	__SCHelperSessionSetPreferences(session, prefs);
	if (prefs != NULL) {
		CFRelease(prefs);
	} else {
		*status = SCError();
	}

	return TRUE;
}


/*
 * ACCESS
 *   (in)  data   = N/A
 *   (out) status = SCError()
 *   (out) reply  = current signature + current preferences
 */
static Boolean
do_prefs_Access(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean			ok;
	SCPreferencesRef	prefs		= __SCHelperSessionGetPreferences(session);
	CFDataRef		signature;

	if (prefs == NULL) {
		return FALSE;
	}

	signature = SCPreferencesGetSignature(prefs);
	if (signature != NULL) {
		const void *		dictKeys[2];
		const void *		dictVals[2];
		SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
		CFDictionaryRef		replyDict;

		dictKeys[0] = CFSTR("signature");
		dictVals[0] = signature;

		dictKeys[1] = CFSTR("preferences");
		dictVals[1] = prefsPrivate->prefs;

		replyDict = CFDictionaryCreate(NULL,
					       (const void **)&dictKeys,
					       (const void **)&dictVals,
					       sizeof(dictKeys)/sizeof(dictKeys[0]),
					       &kCFTypeDictionaryKeyCallBacks,
					       &kCFTypeDictionaryValueCallBacks);

		ok = _SCSerialize(replyDict, reply, NULL, NULL);
		CFRelease(replyDict);
		if (!ok) {
			return FALSE;
		}
	} else {
		*status = SCError();
	}

	return TRUE;
}


/*
 * LOCK
 *   (in)  data   = client prefs signature (NULL if check not needed)
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_prefs_Lock(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	CFDataRef		clientSignature	= (CFDataRef)data;
	Boolean			ok;
	SCPreferencesRef	prefs		= __SCHelperSessionGetPreferences(session);
	Boolean			wait		= (info == (void *)FALSE) ? FALSE : TRUE;

	if (prefs == NULL) {
		return FALSE;
	}

	ok = SCPreferencesLock(prefs, wait);
	if (!ok) {
		*status = SCError();
		return TRUE;
	}

	if (clientSignature != NULL) {
		CFDataRef	serverSignature;

		serverSignature = SCPreferencesGetSignature(prefs);
		if (!CFEqual(clientSignature, serverSignature)) {
			(void)SCPreferencesUnlock(prefs);
			*status = kSCStatusStale;
		}
	}

	return TRUE;
}


/*
 * COMMIT
 *   (in)  data   = new preferences (NULL if commit w/no changes)
 *   (out) status = SCError()
 *   (out) reply  = new signature
 */
static Boolean
do_prefs_Commit(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean			ok;
	SCPreferencesRef	prefs	= __SCHelperSessionGetPreferences(session);

	if (prefs == NULL) {
		return FALSE;
	}

	if (data != NULL) {
		SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

		if (prefsPrivate->prefs != NULL) {
			CFRelease(prefsPrivate->prefs);
		}

		ok = _SCUnserialize((CFPropertyListRef *)&prefsPrivate->prefs, data, NULL, 0);
		if (!ok) {
			return FALSE;
		}

		prefsPrivate->accessed = TRUE;
		prefsPrivate->changed  = TRUE;
	}

	ok = SCPreferencesCommitChanges(prefs);
	if (ok) {
		*reply = SCPreferencesGetSignature(prefs);
		CFRetain(*reply);
	} else {
		*status = SCError();
	}

	return TRUE;
}


/*
 * APPLY
 *   (in)  data   = N/A
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_prefs_Apply(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean			ok;
	SCPreferencesRef	prefs	= __SCHelperSessionGetPreferences(session);

	if (prefs == NULL) {
		return FALSE;
	}

	ok = SCPreferencesApplyChanges(prefs);
	if (!ok) {
		*status = SCError();
	}

	return TRUE;
}


/*
 * UNLOCK
 *   (in)  data   = N/A
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_prefs_Unlock(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean			ok;
	SCPreferencesRef	prefs	= __SCHelperSessionGetPreferences(session);

	if (prefs == NULL) {
		return FALSE;
	}

	ok = SCPreferencesUnlock(prefs);
	if (!ok) {
		*status = SCError();
	}

	return TRUE;
}


/*
 * CLOSE
 *   (in)  data   = N/A
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_prefs_Close(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	SCPreferencesRef	prefs	= __SCHelperSessionGetPreferences(session);

	if (prefs == NULL) {
		return FALSE;
	}

	__SCHelperSessionSetPreferences(session, NULL);
	*status = -1;
	return TRUE;
}


/*
 * SYNCHRONIZE
 *   (in)  data   = N/A
 *   (out) status = kSCStatusOK
 *   (out) reply  = N/A
 */
static Boolean
do_prefs_Synchronize(SCHelperSessionRef session, void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	SCPreferencesRef	prefs	= __SCHelperSessionGetPreferences(session);

	if (prefs == NULL) {
		return FALSE;
	}

	SCPreferencesSynchronize(prefs);
	*status = kSCStatusOK;
	return TRUE;
}


#pragma mark -
#pragma mark Process commands


static Boolean
hasAuthorization(SCHelperSessionRef session)
{
	AuthorizationRef	authorization	= __SCHelperSessionGetAuthorization(session);

#if	!TARGET_OS_IPHONE
	AuthorizationFlags	flags;
	AuthorizationItem	items[1];
	AuthorizationRights	rights;
	OSStatus		status;

	if (authorization == NULL) {
		return FALSE;
	}

	items[0].name        = "system.preferences";
	items[0].value       = NULL;
	items[0].valueLength = 0;
	items[0].flags       = 0;

	rights.count = sizeof(items) / sizeof(items[0]);
	rights.items = items;

	flags = kAuthorizationFlagDefaults;
	flags |= kAuthorizationFlagExtendRights;
	flags |= kAuthorizationFlagInteractionAllowed;
//	flags |= kAuthorizationFlagPartialRights;
//	flags |= kAuthorizationFlagPreAuthorize;

	status = AuthorizationCopyRights(authorization,
					 &rights,
					 kAuthorizationEmptyEnvironment,
					 flags,
					 NULL);
	if (status != errAuthorizationSuccess) {
		return FALSE;
	}
#else	// !TARGET_OS_IPHONE
	uid_t	peer_euid;
	gid_t	peer_egid;

	if (authorization == NULL) {
		return FALSE;
	}

	__SCHelperSessionGetCredentials(session, &peer_euid, &peer_egid);
	if ((peer_euid != 0) && (peer_egid != 0)) {
		static gid_t	mobile_gid	= -1;

		/*
		 * if peer is not user "root" nor group "wheel" then
		 * we check to see if we are one of the authorized
		 * callers.
		 */
		if (mobile_gid == -1) {
			char		buffer[1024];
			struct group	grp;
			struct group	*grpP;

			if (getgrnam_r("mobile", &grp, buffer, sizeof(buffer), &grpP) == 0) {
				mobile_gid = grpP->gr_gid;
			}
		}

		if (peer_egid != mobile_gid) {
			return FALSE;
		}
	}
#endif	// !TARGET_OS_IPHONE

//	if (items[0].flags != 0) SCLog(TRUE, LOG_DEBUG, CFSTR("***** success w/flags (%u) != 0"), items[0].flags);
	return TRUE;
}


typedef Boolean (*helperFunction)	(SCHelperSessionRef	session,
					 void			*info,
					 CFDataRef		data,
					 uint32_t		*status,
					 CFDataRef		*reply);


static const struct helper {
	int		command;
	const char	*commandName;
	Boolean		needsAuthorization;
	helperFunction	func;
	void		*info;
} helpers[] = {
	{ SCHELPER_MSG_AUTH,			"AUTH",			FALSE,	do_Auth			, NULL		},

	{ SCHELPER_MSG_PREFS_OPEN,		"PREFS open",		FALSE,	do_prefs_Open		, NULL		},
	{ SCHELPER_MSG_PREFS_ACCESS,		"PREFS access",		TRUE,	do_prefs_Access		, NULL		},
	{ SCHELPER_MSG_PREFS_LOCK,		"PREFS lock",		TRUE,	do_prefs_Lock		, (void *)FALSE	},
	{ SCHELPER_MSG_PREFS_LOCKWAIT,		"PREFS lock/wait",	TRUE,	do_prefs_Lock		, (void *)TRUE	},
	{ SCHELPER_MSG_PREFS_COMMIT,		"PREFS commit",		TRUE,	do_prefs_Commit		, NULL		},
	{ SCHELPER_MSG_PREFS_APPLY,		"PREFS apply",		TRUE,	do_prefs_Apply		, NULL		},
	{ SCHELPER_MSG_PREFS_UNLOCK,		"PREFS unlock",		FALSE,	do_prefs_Unlock		, NULL		},
	{ SCHELPER_MSG_PREFS_CLOSE,		"PREFS close",		FALSE,	do_prefs_Close		, NULL		},
	{ SCHELPER_MSG_PREFS_SYNCHRONIZE,	"PREFS synchronize",	FALSE,	do_prefs_Synchronize	, NULL		},

	{ SCHELPER_MSG_INTERFACE_REFRESH,	"INTERFACE refresh",	TRUE,	do_interface_refresh	, NULL		},

#if	!TARGET_OS_IPHONE
	{ SCHELPER_MSG_KEYCHAIN_COPY,		"KEYCHAIN copy",	TRUE,	do_keychain_copy	, NULL		},
	{ SCHELPER_MSG_KEYCHAIN_EXISTS,		"KEYCHAIN exists",	TRUE,	do_keychain_exists	, NULL		},
	{ SCHELPER_MSG_KEYCHAIN_REMOVE,		"KEYCHAIN remove",	TRUE,	do_keychain_remove	, NULL		},
	{ SCHELPER_MSG_KEYCHAIN_SET,		"KEYCHAIN set",		TRUE,	do_keychain_set		, NULL		},
#endif	// !TARGET_OS_IPHONE

	{ SCHELPER_MSG_EXIT,			"EXIT",			FALSE,	do_Exit			, NULL		}
};
#define nHELPERS (sizeof(helpers)/sizeof(struct helper))


static int
findHelper(uint32_t command)
{
	int	i;

	for (i = 0; i < (int)nHELPERS; i++) {
		if (helpers[i].command == command) {
			return i;
		}
	}

	return -1;
}


static Boolean
process_command(SCHelperSessionRef session, int fd, int *err)
{
	uint32_t	command	= 0;
	CFDataRef	data	= NULL;
	int		i;
	Boolean		ok	= FALSE;
	CFDataRef	reply	= NULL;
	uint32_t	status	= kSCStatusOK;

	if (!__SCHelper_rxMessage(fd, &command, &data)) {
		SCLog(TRUE, LOG_ERR, CFSTR("no command"));
		*err = EIO;
		goto done;
	}

	i = findHelper(command);
	if (i == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("received unknown command : %u"), command);
		*err = EINVAL;
		goto done;
	}

	SCLog(debug, LOG_DEBUG,
	      CFSTR("processing command \"%s\"%s"),
	      helpers[i].commandName,
	      (data != NULL) ? " w/data" : "");

	if (helpers[i].needsAuthorization && !hasAuthorization(session)) {
		SCLog(debug, LOG_DEBUG,
		      CFSTR("command \"%s\" : not authorized"),
		      helpers[i].commandName);
		status = kSCStatusAccessError;
	}

	if (status == kSCStatusOK) {
		ok = (*helpers[i].func)(session, helpers[i].info, data, &status, &reply);
	}

	if ((status != -1) || (reply != NULL)) {
		SCLog(debug, LOG_DEBUG,
		      CFSTR("sending status %u%s"),
		      status,
		      (reply != NULL) ? " w/reply" : "");

		if (!__SCHelper_txMessage(fd, status, reply)) {
			*err = EIO;
			ok = FALSE;
			goto done;
		}
	}

    done :

	if (data != NULL) {
		CFRelease(data);
	}

	if (reply != NULL) {
		CFRelease(reply);
	}

	return ok;
}


#pragma mark -
#pragma mark Main loop


static void
readCallback(CFSocketRef		s,
	     CFSocketCallBackType	callbackType,
	     CFDataRef			address,
	     const void			*data,
	     void			*info)
{
	CFSocketNativeHandle	fd;
	int			err	= 0;
	Boolean			ok;
	SCHelperSessionRef	session	= (SCHelperSessionRef)info;

	if (callbackType != kCFSocketReadCallBack) {
		SCLog(TRUE, LOG_ERR, CFSTR("readCallback w/callbackType = %d"), callbackType);
		return;
	}

	fd = CFSocketGetNative(s);
	ok = process_command(session, fd, &err);
	if (!ok) {
		SCLog(debug, LOG_DEBUG, CFSTR("per-session socket : invalidate fd %d"), fd);
		CFSocketInvalidate(s);
	}

	return;
}


static void *
newHelper(void *arg)
{
	CFSocketContext		context	= { 0, NULL, CFRetain, CFRelease, CFCopyDescription };
	CFSocketNativeHandle	fd	= (CFSocketNativeHandle)(intptr_t)arg;
	CFRunLoopSourceRef	rls;
	SCHelperSessionRef	session;
	CFSocketRef		sock;

#if	TARGET_OS_IPHONE
	uid_t			peer_euid;
	gid_t			peer_egid;
#endif	// TARGET_OS_IPHONE

	session = __SCHelperSessionCreate(NULL);
#if	TARGET_OS_IPHONE
	if (getpeereid(fd, &peer_euid, &peer_egid) == 0) {
		__SCHelperSessionSetCredentials(session, peer_euid, peer_egid);
	} else {
		SCLog(TRUE, LOG_ERR, CFSTR("getpeereid() failed: %s"), strerror(errno));
	}
#endif	// TARGET_OS_IPHONE

	context.info = (void *)session;
	sock = CFSocketCreateWithNative(NULL,
					fd,
					kCFSocketReadCallBack,
					readCallback,
					&context);
	CFRelease(session);

	rls = CFSocketCreateRunLoopSource(NULL, sock, 0);
	CFRelease(sock);

	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	CFRelease(rls);

	CFRunLoopRun();
	return NULL;
}


static void
acceptCallback(CFSocketRef		s,
	       CFSocketCallBackType	callbackType,
	       CFDataRef		address,
	       const void		*data,
	       void			*info)
{
	CFSocketNativeHandle	fd;
	pthread_attr_t		tattr;
	pthread_t		tid;
	static int		yes	= 1;

	if (callbackType != kCFSocketAcceptCallBack) {
		SCLog(TRUE, LOG_ERR, CFSTR("acceptCallback w/callbackType = %d"), callbackType);
		return;
	}

	if ((data == NULL) ||
	    ((fd = *((CFSocketNativeHandle *)data)) == -1)) {
		SCLog(TRUE, LOG_ERR, CFSTR("accept w/no FD"));
		return;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (const void *)&yes, sizeof(yes)) == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("setsockopt(SO_NOSIGPIPE) failed: %s"), strerror(errno));
		return;
	}

	// start per-session thread
	pthread_attr_init(&tattr);
	pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&tattr, 96 * 1024);	// each thread gets a 96K stack
	pthread_create(&tid, &tattr, newHelper, (void *)(intptr_t)fd);
	pthread_attr_destroy(&tattr);

	return;
}


#include <launch.h>


static const struct option longopts[] = {
	{ "debug",	no_argument,	0,	'd'	},
	{ 0,		0,		0,	0	}
};


int
main(int argc, char **argv)
{
	Boolean			done	= FALSE;
	int			err	= 0;
	int			i;
	launch_data_t		l_listeners;
	launch_data_t		l_msg;
	launch_data_t		l_reply;
	launch_data_t		l_sockets;
	launch_data_type_t	l_type;
	int			n	= 0;
	extern int		optind;
	int			opt;
	int			opti;

	openlog("SCHelper", LOG_CONS|LOG_PID, LOG_DAEMON);

	// process any arguments
	while ((opt = getopt_long(argc, argv, "d", longopts, &opti)) != -1) {
		switch(opt) {
			case 'd':
				debug = TRUE;
				break;
			case 0 :
//				if (strcmp(longopts[opti].name, "debug") == 1) {
//				}
				break;
			case '?':
			default :
				SCLog(TRUE, LOG_ERR,
				      CFSTR("ignoring unknown or ambiguous command line option"));
				break;
		}
	}
//	argc -= optind;
//	argv += optind;

	if (geteuid() != 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("%s"), strerror(EACCES));
		exit(EACCES);
	}

	main_runLoop = CFRunLoopGetCurrent();

	l_msg = launch_data_new_string(LAUNCH_KEY_CHECKIN);
	l_reply = launch_msg(l_msg);
	launch_data_free(l_msg);
	l_type = (l_reply != NULL) ? launch_data_get_type(l_reply) : 0;
	if (l_type != LAUNCH_DATA_DICTIONARY) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCHelper: error w/launchd " LAUNCH_KEY_CHECKIN " dictionary (%p, %d)"),
		      l_reply,
		      l_type);
		err = 1;
		goto done;
	}

	l_sockets = launch_data_dict_lookup(l_reply, LAUNCH_JOBKEY_SOCKETS);
	l_type = (l_sockets != NULL) ? launch_data_get_type(l_sockets) : 0;
	if (l_type != LAUNCH_DATA_DICTIONARY) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCHelper: error w/" LAUNCH_JOBKEY_SOCKETS " (%p, %d)"),
		      l_sockets,
		      l_type);
		err = 1;
		goto done;
	}

	l_listeners = launch_data_dict_lookup(l_sockets, "Listeners");
	l_type = (l_listeners != NULL) ? launch_data_get_type(l_listeners) : 0;
	if (l_type != LAUNCH_DATA_ARRAY) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCHelper: error w/Listeners (%p, %d)"),
		      l_listeners,
		      l_type);
		goto done;
	}

	n = launch_data_array_get_count(l_listeners);
	for (i = 0; i < n; i++) {
		CFSocketNativeHandle	fd;
		launch_data_t		l_fd;
		CFRunLoopSourceRef	rls;
		CFSocketRef		sock;

		l_fd = launch_data_array_get_index(l_listeners, i);
		l_type = (l_fd != NULL) ? launch_data_get_type(l_fd) : 0;
		if (l_type != LAUNCH_DATA_FD) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCHelper: error w/Listeners[%d] (%p, %d)"),
			      i,
			      l_fd,
			      l_type);
			err = 1;
			goto done;
		}

		fd = launch_data_get_fd(l_fd);
		sock = CFSocketCreateWithNative(NULL,
						fd,
						kCFSocketAcceptCallBack,
						acceptCallback,
						NULL);
		rls = CFSocketCreateRunLoopSource(NULL, sock, 0);
		CFRunLoopAddSource(main_runLoop, rls, kCFRunLoopDefaultMode);
		CFRelease(rls);
		CFRelease(sock);
	}

    done :

	if (l_reply != NULL) launch_data_free(l_reply);

	if ((err != 0) || (n == 0)) {
		exit(err);
	}

	while (!done) {
		SInt32	rlStatus;

		rlStatus = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 15.0, TRUE);
		if (rlStatus == kCFRunLoopRunTimedOut) {
			pthread_mutex_lock(&sessions_lock);
			done = ((sessions != NULL) &&
				(CFSetGetCount(sessions) == 0) &&
				(sessions_closed == 0));
			sessions_closed = 0;
			pthread_mutex_unlock(&sessions_lock);
		}
	}

	exit(EX_OK);
}
