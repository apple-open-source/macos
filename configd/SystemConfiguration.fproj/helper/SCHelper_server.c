/*
 * Copyright (c) 2005-2007 Apple Inc. All rights reserved.
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
#include <unistd.h>
#include <sys/types.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <Security/Security.h>

#include "SCPreferencesInternal.h"
#include "SCHelper_client.h"
#include "helper_comm.h"


static AuthorizationRef	authorization	= NULL;
static SCPreferencesRef	prefs		= NULL;


/*
 * EXIT
 *   (in)  data   = N/A
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_Exit(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	return FALSE;
}


/*
 * AUTHORIZE
 *   (in)  data   = AuthorizationExternalForm
 *   (out) status = OSStatus
 *   (out) reply  = N/A
 */
static Boolean
do_Auth(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	if (authorization != NULL) {
		AuthorizationFree(authorization, kAuthorizationFlagDefaults);
//		AuthorizationFree(authorization, kAuthorizationFlagDestroyRights);
		authorization = NULL;
	}

	if (data != NULL) {
		AuthorizationExternalForm	extForm;

		if (CFDataGetLength(data) == sizeof(extForm.bytes)) {
			OSStatus	err;

			bcopy(CFDataGetBytePtr(data), extForm.bytes, sizeof(extForm.bytes));
			err = AuthorizationCreateFromExternalForm(&extForm, &authorization);
			if (err != errAuthorizationSuccess) {
				SCLog(TRUE, LOG_ERR,
				      CFSTR("AuthorizationCreateFromExternalForm() failed: status = %d"),
				      (int)err);
			}
		}

		CFRelease(data);
	}

	*status = (authorization != NULL) ? 0 : 1;

	return TRUE;
}


/*
 * SCHELPER_MSG_KEYCHAIN_COPY
 *   (in)  data   = unique_id
 *   (out) status = SCError()
 *   (out) reply  = password
 */
static Boolean
do_keychain_copy(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	CFStringRef	unique_id	= NULL;

	if ((data != NULL) && !_SCUnserializeString(&unique_id, data, NULL, 0)) {
		return FALSE;
	}

	if (!isA_CFString(unique_id)) {
		return FALSE;
	}

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
do_keychain_exists(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean		ok;
	CFStringRef	unique_id	= NULL;

	if ((data != NULL) && !_SCUnserializeString(&unique_id, data, NULL, 0)) {
		return FALSE;
	}

	if (!isA_CFString(unique_id)) {
		return FALSE;
	}

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
do_keychain_remove(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean		ok;
	CFStringRef	unique_id	= NULL;

	if ((data != NULL) && !_SCUnserializeString(&unique_id, data, NULL, 0)) {
		return FALSE;
	}

	if (!isA_CFString(unique_id)) {
		return FALSE;
	}

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
do_keychain_set(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	CFStringRef	account;
	CFStringRef	description;
	CFArrayRef	executablePaths	= NULL;
	CFStringRef	label;
	Boolean		ok;
	CFDictionaryRef	options		= NULL;
	CFDataRef	password;
	CFStringRef	unique_id;

	if ((data != NULL) && !_SCUnserialize((CFPropertyListRef *)&options, data, NULL, 0)) {
		return FALSE;
	}

	if (!isA_CFDictionary(options)) {
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


/*
 * SCHELPER_MSG_INTERFACE_REFRESH
 *   (in)  data   = ifName
 *   (out) status = SCError()
 *   (out) reply  = N/A
 */
static Boolean
do_interface_refresh(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	CFStringRef	ifName	= NULL;
	Boolean		ok;

	if ((data != NULL) && !_SCUnserializeString(&ifName, data, NULL, 0)) {
		SCLog(TRUE, LOG_ERR, CFSTR("interface name not valid"));
		return FALSE;
	}

	if (!isA_CFString(ifName)) {
		SCLog(TRUE, LOG_ERR, CFSTR("interface name not valid"));
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
do_prefs_Open(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	CFStringRef	prefsID	= NULL;

	if (prefs != NULL) {
		return FALSE;
	}

	if ((data != NULL) && !_SCUnserializeString(&prefsID, data, NULL, 0)) {
		SCLog(TRUE, LOG_ERR, CFSTR("prefsID not valid"));
		return FALSE;
	}

	prefs = SCPreferencesCreate(NULL, CFSTR("SCHelper"), prefsID);
	if (prefsID != NULL) CFRelease(prefsID);

	if (prefs == NULL) {
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
do_prefs_Access(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	CFDataRef		signature;

	if (prefs == NULL) {
		return FALSE;
	}

	signature = SCPreferencesGetSignature(prefs);
	if (signature != NULL) {
		const void *	dictKeys[2];
		const void *	dictVals[2];
		CFDictionaryRef	replyDict;

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
do_prefs_Lock(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	CFDataRef	clientSignature	= (CFDataRef)data;
	Boolean		ok;
	Boolean		wait		= (info == (void *)FALSE) ? FALSE : TRUE;

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
do_prefs_Commit(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	if (prefs == NULL) {
		return FALSE;
	}

	if (data != NULL) {
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
do_prefs_Apply(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean	ok;

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
do_prefs_Unlock(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean	ok;

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
do_prefs_Close(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	if (prefs == NULL) {
		return FALSE;
	}

	CFRelease(prefs);
	prefs = NULL;

	return TRUE;
}


/*
 * SYNCHRONIZE
 *   (in)  data   = N/A
 *   (out) status = kSCStatusOK
 *   (out) reply  = N/A
 */
static Boolean
do_prefs_Synchronize(void *info, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	if (prefs == NULL) {
		return FALSE;
	}
	
	SCPreferencesSynchronize(prefs);
	*status = kSCStatusOK;
	return TRUE;
}


static Boolean
hasAuthorization()
{
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

if (items[0].flags != 0) SCLog(TRUE, LOG_DEBUG, CFSTR("***** success w/flags (%u) != 0"), items[0].flags);
	return TRUE;
}


typedef Boolean (*helperFunction)	(void		*info,
					 CFDataRef	data,
					 uint32_t	*status,
					 CFDataRef	*reply);


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

	{ SCHELPER_MSG_KEYCHAIN_COPY,		"KEYCHAIN copy",	TRUE,	do_keychain_copy	, NULL		},
	{ SCHELPER_MSG_KEYCHAIN_EXISTS,		"KEYCHAIN exists",	TRUE,	do_keychain_exists	, NULL		},
	{ SCHELPER_MSG_KEYCHAIN_REMOVE,		"KEYCHAIN remove",	TRUE,	do_keychain_remove	, NULL		},
	{ SCHELPER_MSG_KEYCHAIN_SET,		"KEYCHAIN set",		TRUE,	do_keychain_set		, NULL		},

	{ SCHELPER_MSG_EXIT,			"EXIT",			FALSE,	do_Exit			, NULL		}
};
#define nHELPERS (sizeof(helpers)/sizeof(struct helper))


static int
findHelper(command)
{
	int	i;

	for (i = 0; i < (int)nHELPERS; i++) {
		if (helpers[i].command == command) {
			return i;
		}
	}

	return -1;
}


int
main(int argc, char **argv)
{
	int		err	= 0;
	Boolean		ok	= TRUE;

	openlog("SCHelper", LOG_CONS|LOG_PID, LOG_DAEMON);

	if (geteuid() != 0) {
		(void)__SCHelper_txMessage(STDOUT_FILENO, EACCES, NULL);
		exit(EACCES);
	}

	// send "we are here" message
	if (!__SCHelper_txMessage(STDOUT_FILENO, 0, NULL)) {
		exit(EIO);
	}

	while (ok) {
		uint32_t	command;
		CFDataRef	data;
		int		i;
		CFDataRef	reply;
		uint32_t	status;

		command = 0;
		data    = NULL;
		if (!__SCHelper_rxMessage(STDIN_FILENO, &command, &data)) {
			SCLog(TRUE, LOG_ERR, CFSTR("no command"));
			err = EIO;
			break;
		}

		i = findHelper(command);
		if (i == -1) {
			SCLog(TRUE, LOG_ERR, CFSTR("received unknown command : %u"), command);
			err = EINVAL;
			break;
		}

		SCLog(TRUE, LOG_DEBUG,
		      CFSTR("processing command \"%s\"%s"),
		      helpers[i].commandName,
		      (data != NULL) ? " w/data" : "");

		status = kSCStatusOK;
		reply  = NULL;

		if (helpers[i].needsAuthorization && !hasAuthorization()) {
			SCLog(TRUE, LOG_DEBUG,
			      CFSTR("command \"%s\" : not authorized"),
			      helpers[i].commandName);
			status = kSCStatusAccessError;
		}

		if (status == kSCStatusOK) {
			ok = (*helpers[i].func)(helpers[i].info, data, &status, &reply);
		}

		SCLog(TRUE, LOG_DEBUG,
		      CFSTR("sending status %u%s"),
		      status,
		      (reply != NULL) ? " w/reply" : "");

		if (!__SCHelper_txMessage(STDOUT_FILENO, status, reply)) {
			err = EIO;
			break;
		}

		if (reply != NULL) {
			CFRelease(reply);
		}
	}

	if (prefs != NULL) {
		CFRelease(prefs);
	}

	if (authorization != NULL) {
		AuthorizationFree(authorization, kAuthorizationFlagDefaults);
//		AuthorizationFree(authorization, kAuthorizationFlagDestroyRights);
	}

	exit(err);
}
