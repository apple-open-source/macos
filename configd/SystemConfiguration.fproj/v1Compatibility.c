/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
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
 * March 28, 2001		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"

#include "v1Compatibility.h"

extern void __Initialize();

typedef struct {

	/* configuration data associated with key */
	CFPropertyListRef	data;

	/* instance value of last fetched data */
	int			instance;

} SCDHandlePrivate, *SCDHandlePrivateRef;


SCDHandleRef
SCDHandleInit()
{
	SCDHandlePrivateRef privateHandle = CFAllocatorAllocate(NULL, sizeof(SCDHandlePrivate), 0);

	/* set data */
	privateHandle->data = NULL;

	/* set instance */
	privateHandle->instance = 0;

	return (SCDHandleRef)privateHandle;
}


void
SCDHandleRelease(SCDHandleRef handle)
{
	SCDHandlePrivateRef privateHandle = (SCDHandlePrivateRef)handle;

	if (privateHandle->data)
		CFRelease(privateHandle->data);

	CFAllocatorDeallocate(NULL, privateHandle);
	return;
}


int
SCDHandleGetInstance(SCDHandleRef handle)
{
	SCDHandlePrivateRef privateHandle = (SCDHandlePrivateRef)handle;

	return privateHandle->instance;
}


void
_SCDHandleSetInstance(SCDHandleRef handle, int instance)
{
	SCDHandlePrivateRef privateHandle = (SCDHandlePrivateRef)handle;

	privateHandle->instance = instance;
	return;
}


CFPropertyListRef
SCDHandleGetData(SCDHandleRef handle)
{
	SCDHandlePrivateRef privateHandle = (SCDHandlePrivateRef)handle;

	if (privateHandle->data == NULL) {
		return CFSTR("SCDHandleRef not initialized.");
	}

	return privateHandle->data;
}


void
SCDHandleSetData(SCDHandleRef handle, CFPropertyListRef data)
{
	SCDHandlePrivateRef privateHandle = (SCDHandlePrivateRef)handle;

	/* remove reference to data previously associated with handle */
	if (privateHandle->data)
		CFRelease(privateHandle->data);

	/* associate new data with handle, keep a reference as needed */
	privateHandle->data = data;
	if (privateHandle->data)
		CFRetain(privateHandle->data);

	return;
}

static int
convert_SCDStatus_To_SCStatus(SCDStatus status)
{
	switch (status) {
		case SCD_OK			: return kSCStatusOK;
		case SCD_NOSESSION		: return kSCStatusNoStoreSession;
		case SCD_NOSERVER		: return kSCStatusNoStoreServer;
		case SCD_LOCKED			: return kSCStatusLocked;
		case SCD_NEEDLOCK		: return kSCStatusNeedLock;
		case SCD_EACCESS		: return kSCStatusAccessError;
		case SCD_NOKEY			: return kSCStatusNoKey;
		case SCD_EXISTS			: return kSCStatusKeyExists;
		case SCD_STALE			: return kSCStatusStale;
		case SCD_INVALIDARGUMENT	: return kSCStatusInvalidArgument;
		case SCD_NOTIFIERACTIVE		: return kSCStatusNotifierActive;
		case SCD_FAILED			: return kSCStatusFailed;
		default				: return kSCStatusFailed;
	}
}

SCDStatus
SCDOpen(SCDSessionRef *session, CFStringRef name)
{
	SCDynamicStoreRef	newStore;

	__Initialize();		/* initialize framework */

	newStore = SCDynamicStoreCreate(NULL, name, NULL, NULL);
	if (!newStore) {
		return SCD_NOSERVER;
	}

	*session = (SCDSessionRef)newStore;
	return SCD_OK;
}

SCDStatus
SCDClose(SCDSessionRef *session)
{
	CFRelease(*session);
	*session = NULL;
	return SCD_OK;
}

SCDStatus
SCDLock(SCDSessionRef session)
{
	return SCDynamicStoreLock((SCDynamicStoreRef)session) ? SCD_OK : SCD_FAILED;
}

SCDStatus
SCDUnlock(SCDSessionRef session)
{
	return SCDynamicStoreUnlock((SCDynamicStoreRef)session) ? SCD_OK : SCD_FAILED;
}

SCDStatus
SCDList(SCDSessionRef session, CFStringRef key, int regexOptions, CFArrayRef *subKeys)
{
	CFMutableStringRef	pattern;

	pattern = CFStringCreateMutableCopy(NULL, 0, key);
	if ((regexOptions & kSCDRegexKey) != kSCDRegexKey) {
		CFStringAppend(pattern, CFSTR(".*"));
	}
	*subKeys = SCDynamicStoreCopyKeyList((SCDynamicStoreRef)session, pattern);
	CFRelease(pattern);

	return (*subKeys) ? SCD_OK : SCD_FAILED;
}

SCDStatus
SCDAdd(SCDSessionRef session, CFStringRef key, SCDHandleRef handle)
{
	CFTypeRef	value = SCDHandleGetData(handle);
	return SCDynamicStoreAddValue((SCDynamicStoreRef)session, key, value) ? SCD_OK : SCD_EXISTS;
}

SCDStatus
SCDAddSession(SCDSessionRef session, CFStringRef key, SCDHandleRef handle)
{
	CFTypeRef	value = SCDHandleGetData(handle);
	return SCDynamicStoreAddTemporaryValue((SCDynamicStoreRef)session, key, value) ? SCD_OK : SCD_EXISTS;
}

SCDStatus
SCDGet(SCDSessionRef session, CFStringRef key, SCDHandleRef *handle)
{
	CFTypeRef	value;

	value = SCDynamicStoreCopyValue((SCDynamicStoreRef)session, key);
	if (value) {
		*handle = SCDHandleInit();
		SCDHandleSetData(*handle, value);
		CFRelease(value);
		return SCD_OK;
	}
	return SCD_NOKEY;
}

SCDStatus
SCDSet(SCDSessionRef session, CFStringRef key, SCDHandleRef handle)
{
	CFTypeRef	value = SCDHandleGetData(handle);
	return SCDynamicStoreSetValue((SCDynamicStoreRef)session, key, value) ? SCD_OK : SCD_EXISTS;
}

SCDStatus
SCDRemove(SCDSessionRef session, CFStringRef key)
{
	return SCDynamicStoreRemoveValue((SCDynamicStoreRef)session, key) ? SCD_OK : SCD_NOKEY;
}

SCDStatus
SCDTouch(SCDSessionRef session, CFStringRef key)
{
	return SCDynamicStoreTouchValue((SCDynamicStoreRef)session, key) ? SCD_OK : SCD_FAILED;
}

SCDStatus
SCDNotifierList(SCDSessionRef session, int regexOptions, CFArrayRef *notifierKeys)
{
	*notifierKeys = SCDynamicStoreCopyWatchedKeyList((SCDynamicStoreRef)session,
							 ((regexOptions & kSCDRegexKey) == kSCDRegexKey));
	return (*notifierKeys) ? SCD_OK : SCD_FAILED;
}

SCDStatus
SCDNotifierAdd(SCDSessionRef session, CFStringRef key, int regexOptions)
{
	return SCDynamicStoreAddWatchedKey((SCDynamicStoreRef)session,
					   key,
					   ((regexOptions & kSCDRegexKey) == kSCDRegexKey)) ? SCD_OK : SCD_EXISTS;
}

SCDStatus
SCDNotifierRemove(SCDSessionRef session, CFStringRef key, int regexOptions)
{
	return SCDynamicStoreRemoveWatchedKey((SCDynamicStoreRef)session,
					      key,
					      ((regexOptions & kSCDRegexKey) == kSCDRegexKey)) ? SCD_OK : SCD_NOKEY;
}

SCDStatus
SCDNotifierGetChanges(SCDSessionRef session, CFArrayRef *changedKeys)
{
	*changedKeys = SCDynamicStoreCopyNotifiedKeys((SCDynamicStoreRef)session);
	return (*changedKeys) ? SCD_OK : SCD_FAILED;
}

SCDStatus
SCDNotifierWait(SCDSessionRef session)
{
	return SCDynamicStoreNotifyWait((SCDynamicStoreRef)session) ? SCD_OK : SCD_FAILED;
}

SCDStatus
SCDNotifierInformViaCallback(SCDSessionRef session, SCDCallbackRoutine_t func, void *arg)
{
	return SCDynamicStoreNotifyCallback((SCDynamicStoreRef)session,
					    CFRunLoopGetCurrent(),
					    (SCDynamicStoreCallBack_v1)func,
					    arg) ? SCD_OK : SCD_NOTIFIERACTIVE;
}

SCDStatus
SCDNotifierInformViaMachPort(SCDSessionRef session, mach_msg_id_t msgid, mach_port_t *port)
{
	return SCDynamicStoreNotifyMachPort((SCDynamicStoreRef)session, msgid, port) ? SCD_OK : SCD_NOTIFIERACTIVE;
}

SCDStatus
SCDNotifierInformViaFD(SCDSessionRef session, int32_t identifier, int *fd)
{
	return SCDynamicStoreNotifyFileDescriptor((SCDynamicStoreRef)session, identifier, fd) ? SCD_OK : SCD_NOTIFIERACTIVE;
}

SCDStatus
SCDNotifierInformViaSignal(SCDSessionRef session, pid_t pid, int sig)
{
	return SCDynamicStoreNotifySignal((SCDynamicStoreRef)session, pid, sig) ? SCD_OK : SCD_NOTIFIERACTIVE;
}

SCDStatus
SCDNotifierCancel(SCDSessionRef session)
{
	return SCDynamicStoreNotifyCancel((SCDynamicStoreRef)session) ? SCD_OK : SCD_NOTIFIERACTIVE;
}

SCDStatus
SCDSnapshot(SCDSessionRef session)
{
	return SCDynamicStoreSnapshot((SCDynamicStoreRef)session) ? SCD_OK : SCD_NOTIFIERACTIVE;
}

int
SCDOptionGet(SCDSessionRef session, int option)
{
	int             value     = 0;

	if (session) {
		static	Boolean	warned = FALSE;
		if (!warned) {
			SCLog(TRUE, LOG_NOTICE, CFSTR("per-session options are no longer supported, using global options."));
			warned = TRUE;
		}
	}

	switch (option) {
		case kSCDOptionDebug :
			value = _sc_debug ? 1 : 0;
			break;

		case kSCDOptionVerbose :
			value = _sc_verbose ? 1 : 0;
			break;

		case kSCDOptionUseSyslog :
			value = _sc_log ? 1 : 0;
			break;

		case kSCDOptionUseCFRunLoop :
			value = 1;	/* always TRUE */
			break;
	}

	return value;
}

void
SCDOptionSet(SCDSessionRef session, int option, int value)
{
	if (session) {
		static Boolean	warned = FALSE;
		if (!warned) {
			SCLog(TRUE, LOG_NOTICE, CFSTR("per-session options are no longer supported, using global options."));
			warned = TRUE;
		}
	}

	switch (option) {
		case kSCDOptionDebug :
			_sc_debug = (value != 0);
			_sc_log   = (value == 0);
			break;

		case kSCDOptionVerbose :
			_sc_verbose = (value != 0);
			break;

		case kSCDOptionUseSyslog :
		{
			_sc_log = (value != 0);
			break;
		}

		case kSCDOptionUseCFRunLoop :
		{
			static Boolean warned = FALSE;
			if ((value == FALSE) && !warned) {
				SCLog(TRUE, LOG_NOTICE, CFSTR("The kSCDOptionUseCFRunLoop option can no longer be set FALSE.  The"));
				SCLog(TRUE, LOG_NOTICE, CFSTR("SCDNotifierInformViaCallback requires the use of a CFRunLoop."));
				warned = TRUE;
			}
			break;
		}
	}

	return;
}

void
SCDSessionLog(SCDSessionRef session, int level, CFStringRef formatString, ...)
{
	va_list		argList;
	FILE            *f = (LOG_PRI(level) > LOG_NOTICE) ? stderr : stdout;
	CFStringRef	resultString;

	if ((LOG_PRI(level) == LOG_DEBUG) && !SCDOptionGet(session, kSCDOptionVerbose)) {
		/* it's a debug message and we haven't requested verbose logging */
		return;
	}

	va_start(argList, formatString);
	resultString = CFStringCreateWithFormatAndArguments(NULL, NULL, formatString, argList);
	va_end(argList);

	if (SCDOptionGet(session, kSCDOptionUseSyslog)) {
		__SCLog(level, resultString);
	} else {
		CFStringRef     newString;

		/* add a new-line */
		newString = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@\n"), resultString);
		__SCPrint(f, newString);
		CFRelease(newString);
	}
	CFRelease(resultString);
}

void
SCDLog(int level, CFStringRef formatString, ...)
{
	va_list		argList;
	FILE            *f = (LOG_PRI(level) > LOG_NOTICE) ? stderr : stdout;
	CFStringRef	resultString;

	if ((LOG_PRI(level) == LOG_DEBUG) && !SCDOptionGet(NULL, kSCDOptionVerbose)) {
		/* it's a debug message and we haven't requested verbose logging */
		return;
	}

	va_start(argList, formatString);
	resultString = CFStringCreateWithFormatAndArguments(NULL, NULL, formatString, argList);
	va_end(argList);

	if (SCDOptionGet(NULL, kSCDOptionUseSyslog)) {
		__SCLog(level, resultString);
	} else {
		CFStringRef     newString;

		/* add a new-line */
		newString = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@\n"), resultString);
		__SCPrint(f, newString);
		CFRelease(newString);
	}
	CFRelease(resultString);
}

const char *
SCDError(SCDStatus status)
{
	return SCErrorString(convert_SCDStatus_To_SCStatus(status));
}

CFStringRef
SCDKeyCreate(CFStringRef fmt, ...)
{
	va_list	args;
	va_start(args, fmt);
	return (CFStringCreateWithFormatAndArguments(NULL,
						     NULL,
						     fmt,
						     args));
}

CFStringRef
SCDKeyCreateNetworkGlobalEntity(CFStringRef domain, CFStringRef entity)
{
	return SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, domain, entity);
}

CFStringRef
SCDKeyCreateNetworkInterface(CFStringRef domain)
{
	return SCDynamicStoreKeyCreateNetworkInterface(NULL, domain);
}

CFStringRef
SCDKeyCreateNetworkInterfaceEntity(CFStringRef domain, CFStringRef ifname, CFStringRef entity)
{
	return SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, domain, ifname, entity);
}

CFStringRef
SCDKeyCreateNetworkServiceEntity(CFStringRef domain, CFStringRef serviceID, CFStringRef entity)
{
	return SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, domain, serviceID, entity);
}

static int
convert_SCPStatus_To_SCStatus(SCPStatus status)
{
	switch (status) {
		case SCP_OK			: return kSCStatusOK;
		case SCP_NOSESSION		: return kSCStatusNoPrefsSession;
		case SCP_BUSY			: return kSCStatusPrefsBusy;
		case SCP_NEEDLOCK		: return kSCStatusNeedLock;
		case SCP_EACCESS		: return kSCStatusAccessError;
		case SCP_ENOENT			: return kSCStatusNoConfigFile;
		case SCP_BADCF			: return kSCStatusFailed;
		case SCP_NOKEY			: return kSCStatusNoKey;
		case SCP_NOLINK			: return kSCStatusNoLink;
		case SCP_EXISTS			: return kSCStatusKeyExists;
		case SCP_STALE			: return kSCStatusStale;
		case SCP_INVALIDARGUMENT	: return kSCStatusInvalidArgument;
		case SCP_FAILED			: return kSCStatusFailed;
		default				: return kSCStatusFailed;
	}
}

SCPStatus
SCPOpen(SCPSessionRef *session, CFStringRef name, CFStringRef prefsID, int options)
{
	CFArrayRef	keys;
	CFIndex		nKeys;

	__Initialize();		/* initialize framework */

	*session = (SCPSessionRef)SCPreferencesCreate(NULL, name, prefsID);
	if (*session == NULL) {
		return SCP_EACCESS;
	}

	keys  = SCPreferencesCopyKeyList(*session);
	nKeys = CFArrayGetCount(keys);
	CFRelease(keys);

	if ((nKeys == 0) &&
	    ((options & kSCPOpenCreatePrefs) != kSCPOpenCreatePrefs)) {
		/* if no keys and not requesting the file be created */
		return SCP_ENOENT;
	}

	return SCP_OK;
}

SCPStatus
SCPUserOpen(SCPSessionRef *session, CFStringRef name, CFStringRef prefsID, CFStringRef user, int options)
{
	CFArrayRef	keys;
	CFIndex		nKeys;

	__Initialize();		/* initialize framework */

	*session = (SCPSessionRef)SCUserPreferencesCreate(NULL, name, prefsID, user);
	if (*session == NULL) {
		return SCP_EACCESS;
	}

	keys  = SCPreferencesCopyKeyList(*session);
	nKeys = CFArrayGetCount(keys);
	CFRelease(keys);

	if ((nKeys == 0) &&
	    ((options & kSCPOpenCreatePrefs) != kSCPOpenCreatePrefs)) {
		/* if no keys and not requesting the file be created */
		return SCP_ENOENT;
	}

	return SCP_OK;
}

SCPStatus
SCPClose(SCPSessionRef *session)
{
	CFRelease(*session);
	*session = NULL;
	return SCD_OK;
}

SCPStatus
SCPLock(SCPSessionRef session, boolean_t wait)
{
	/* XXXXX: old API error codes included kSCStatusPrefsBusy, kSCStatusAccessError, and kSCStatusStale */
	return SCPreferencesLock((SCPreferencesRef)session, wait) ? SCP_OK : SCP_FAILED;
}

SCPStatus
SCPCommit(SCPSessionRef session)
{
	/* XXXXX: old API error codes included kSCStatusAccessError, kSCStatusStale */
	return SCPreferencesCommitChanges((SCPreferencesRef)session) ? SCP_OK : SCP_FAILED;
}

SCPStatus
SCPApply(SCPSessionRef session)
{
	return SCPreferencesApplyChanges((SCPreferencesRef)session) ? SCP_OK : SCP_EACCESS;
}

SCPStatus
SCPUnlock(SCPSessionRef session)
{
	return SCPreferencesUnlock((SCPreferencesRef)session) ? SCP_OK : SCP_FAILED;
}

SCPStatus
SCPGetSignature(SCPSessionRef session, CFDataRef *signature)
{
	*signature = SCPreferencesGetSignature((SCPreferencesRef)session);
	return (*signature) ? SCP_OK : SCP_FAILED;
}

SCPStatus
SCPList(SCPSessionRef session, CFArrayRef *keys)
{
	*keys = SCPreferencesCopyKeyList((SCPreferencesRef)session);
	return (*keys) ? SCP_OK : SCP_FAILED;
}

SCPStatus
SCPGet(SCPSessionRef session, CFStringRef key, CFPropertyListRef *data)
{
	*data = SCPreferencesGetValue((SCPreferencesRef)session, key);
	return (*data) ? SCP_OK : SCP_NOKEY;
}

SCPStatus
SCPAdd(SCPSessionRef session, CFStringRef key, CFPropertyListRef data)
{
	return SCPreferencesAddValue((SCPreferencesRef)session, key, data) ? SCP_OK : SCP_EXISTS;
}

SCPStatus
SCPSet(SCPSessionRef session, CFStringRef key, CFPropertyListRef data)
{
	return SCPreferencesSetValue((SCPreferencesRef)session, key, data) ? SCP_OK : SCP_FAILED;
}

SCPStatus
SCPRemove(SCPSessionRef session, CFStringRef key)
{
	return SCPreferencesRemoveValue((SCPreferencesRef)session, key) ? SCP_OK : SCP_NOKEY;
}

CFStringRef
SCPNotificationKeyCreate(CFStringRef prefsID, int keyType)
{
	return SCDynamicStoreKeyCreatePreferences(NULL, prefsID, keyType);
}

CFStringRef
SCPUserNotificationKeyCreate(CFStringRef prefsID, CFStringRef user, int keyType)
{
	return SCDynamicStoreKeyCreateUserPreferences(NULL, prefsID, user, keyType);
}

SCPStatus
SCPPathCreateUniqueChild(SCPSessionRef session, CFStringRef prefix, CFStringRef *newPath)
{
	*newPath = SCPreferencesPathCreateUniqueChild((SCPreferencesRef)session, prefix);
	return (*newPath) ? SCP_OK : SCP_NOKEY;
}

SCPStatus
SCPPathGetValue(SCPSessionRef session, CFStringRef path, CFDictionaryRef *value)
{
	*value = SCPreferencesPathGetValue((SCPreferencesRef)session, path);
	return (*value) ? SCP_OK : SCP_NOKEY;
}

SCPStatus
SCPPathGetLink(SCPSessionRef session, CFStringRef path, CFStringRef *link)
{
	*link = SCPreferencesPathGetLink((SCPreferencesRef)session, path);
	return (*link) ? SCP_OK : SCP_NOKEY;
}

SCPStatus
SCPPathSetValue(SCPSessionRef session, CFStringRef path, CFDictionaryRef value)
{
	return SCPreferencesPathSetValue((SCPreferencesRef)session, path, value) ? SCP_OK : SCP_FAILED;
}

SCPStatus
SCPPathSetLink(SCPSessionRef session, CFStringRef path, CFStringRef link)
{
	return SCPreferencesPathSetLink((SCPreferencesRef)session, path, link) ? SCP_OK : SCP_FAILED;
}

SCPStatus
SCPPathRemove(SCPSessionRef session, CFStringRef path)
{
	return SCPreferencesPathRemoveValue((SCPreferencesRef)session, path) ? SCP_OK : SCP_NOKEY;
}

const char *
SCPError(SCPStatus status)
{
	return SCErrorString(convert_SCPStatus_To_SCStatus(status));
}

CFStringRef
SCDKeyCreateConsoleUser()
{
	return SCDynamicStoreKeyCreateConsoleUser(NULL);
}

SCDStatus
SCDConsoleUserGet(char *user, int userlen, uid_t *uid, gid_t *gid)
{
	CFStringRef	consoleUser;

	consoleUser = SCDynamicStoreCopyConsoleUser(NULL, uid, gid);
	if (!consoleUser) {
		return SCD_NOKEY;
	}

	if (user && (userlen > 0)) {
		CFIndex         len;
		CFRange         range;

		bzero(user, userlen);
		range = CFRangeMake(0, CFStringGetLength(consoleUser));
		(void) CFStringGetBytes(consoleUser,
					range,
					kCFStringEncodingMacRoman,
					0,
					FALSE,
					user,
					userlen,
					&len);
	}
	CFRelease(consoleUser);
	return SCD_OK;
}

SCDStatus
SCDConsoleUserSet(const char *user, uid_t uid, gid_t gid)
{
	return SCDynamicStoreSetConsoleUser(NULL, user, uid, gid) ? SCD_OK : SCD_FAILED;
}

CFStringRef
SCDKeyCreateHostName()
{
	return SCDynamicStoreKeyCreateComputerName(NULL);
}

SCDStatus
SCDHostNameGet(CFStringRef *name, CFStringEncoding *nameEncoding)
{
	*name = SCDynamicStoreCopyComputerName(NULL, nameEncoding);
	return (*name) ? SCD_OK : SCD_FAILED;
}

static SCNStatus
convertReachability(int newFlags, int *oldFlags)
{
	SCNStatus	scn_status = SCN_REACHABLE_NO;

	if (newFlags & kSCNetworkFlagsTransientConnection) {
		if (oldFlags) {
			*oldFlags |= kSCNFlagsTransientConnection;
		}
	}

	if (newFlags & kSCNetworkFlagsReachable) {
		scn_status = SCN_REACHABLE_YES;

		if (newFlags & kSCNetworkFlagsConnectionRequired) {
			scn_status = SCN_REACHABLE_CONNECTION_REQUIRED;
		}

		if (newFlags & kSCNetworkFlagsConnectionAutomatic) {
			if (oldFlags) {
				*oldFlags |= kSCNFlagsConnectionAutomatic;
			}
		}

		if (newFlags & kSCNetworkFlagsInterventionRequired) {
			if (oldFlags) {
				*oldFlags |= kSCNFlagsInterventionRequired;
			}
		}
	}

	return scn_status;
}

SCNStatus
SCNIsReachableByAddress(const struct sockaddr *address, const int addrlen, int *flags, const char **errorMessage)
{
	SCNetworkConnectionFlags	newFlags;

	if (!SCNetworkCheckReachabilityByAddress(address, addrlen, &newFlags)) {
		if (errorMessage) {
			*errorMessage = SCErrorString(kSCStatusReachabilityUnknown);
		}
		return SCN_REACHABLE_UNKNOWN;
	}

	return convertReachability(newFlags, flags);

}

SCNStatus
SCNIsReachableByName(const char *nodename, int *flags, const char **errorMessage)
{
	SCNetworkConnectionFlags	newFlags;

	if (!SCNetworkCheckReachabilityByName(nodename, &newFlags)) {
		if (errorMessage) {
			*errorMessage = SCErrorString(kSCStatusReachabilityUnknown);
		}
		return SCN_REACHABLE_UNKNOWN;
	}

	return convertReachability(newFlags, flags);
}
