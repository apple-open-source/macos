/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * June 2, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#ifndef _S_SCD_H
#define _S_SCD_H

#include <sys/cdefs.h>


/*
 * keys in the "storeData" dictionary
 */

/*
 * data associated with a key
 */
#define	kSCDData	CFSTR("data")
/*
 * instance value associated with a key
 */
#define	kSCDInstance	CFSTR("instance")
/*
 * client session ids watching a key and, since we can possibly have
 * multiple regex keys which reference the key, a count of active
 * references
 */
#define	kSCDWatchers	CFSTR("watchers")
#define	kSCDWatcherRefs	CFSTR("watcherRefs")
/*
 * client session id for per-session keys.
 */
#define	kSCDSession	CFSTR("session")


/*
 * keys in the "sessionData" dictionary
 */

/*
 * the name of the calling application / plug-in
 */
#define	kSCDName	CFSTR("name")
/*
 * keys which have changed since last call to SCDNotifierGetChanges()
 */
#define	kSCDChangedKeys	CFSTR("changedKeys")
/*
 * for notification keys which consist of a regular expression we keep
 * both the pattern string and the compiled regular expression
 */
#define	kSCDRegexKeys	CFSTR("regexKeys")
#define	kSCDRegexData	CFSTR("regexData")
/*
 * keys which are to be removed when the session is closed
 */
#define	kSCDSessionKeys	CFSTR("sessionKeys")


extern int			storeLocked;
extern CFMutableDictionaryRef	storeData;
extern CFMutableDictionaryRef	sessionData;
extern CFMutableSetRef		changedKeys;
extern CFMutableSetRef		deferredRemovals;
extern CFMutableSetRef		removedSessionKeys;
extern CFMutableSetRef		needsNotification;

extern CFMutableDictionaryRef	storeData_s;
extern CFMutableSetRef		changedKeys_s;
extern CFMutableSetRef		deferredRemovals_s;
extern CFMutableSetRef		removedSessionKeys_s;

/*
 * "context" argument for CFDictionaryArrayApplier _addRegexWatcherByKey(),
 * _addRegexWatcherBySession(), and _removeRegexWatcherByKey() functions.
 */
typedef struct {
	SCDynamicStorePrivateRef	store;
	regex_t				*preg;
} addContext, *addContextRef;


typedef struct {
	SCDynamicStorePrivateRef	store;
	regex_t				*preg;
} removeContext, *removeContextRef;


__BEGIN_DECLS

int
__SCDynamicStoreOpen			(SCDynamicStoreRef	*store,
					 CFStringRef		name);
int
__SCDynamicStoreClose			(SCDynamicStoreRef	*store);

int
__SCDynamicStoreLock			(SCDynamicStoreRef	store,
					 Boolean		recursive);

int
__SCDynamicStoreUnlock			(SCDynamicStoreRef	store,
					 Boolean		recursive);

int
__SCDynamicStoreCopyKeyList		(SCDynamicStoreRef	store,
					 CFStringRef		prefix,
					 Boolean		isRegex,
					 CFArrayRef		*subKeys);

int
__SCDynamicStoreAddValue		(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 CFPropertyListRef	value);

int
__SCDynamicStoreAddTemporaryValue	(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 CFPropertyListRef	value);

int
__SCDynamicStoreCopyValue		(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 CFPropertyListRef	*value);

int
__SCDynamicStoreSetValue		(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 CFPropertyListRef	value);

int
__SCDynamicStoreRemoveValue		(SCDynamicStoreRef	store,
					 CFStringRef		key);

int
__SCDynamicStoreTouchValue		(SCDynamicStoreRef	store,
					 CFStringRef		key);

int
__SCDynamicStoreNotifyValue		(SCDynamicStoreRef	store,
					 CFStringRef		key);

int
__SCDynamicStoreSnapshot		(SCDynamicStoreRef	store);

int
__SCDynamicStoreAddWatchedKey		(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 Boolean		isRegex);

int
__SCDynamicStoreRemoveWatchedKey	(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 Boolean		isRegex);

int
__SCDynamicStoreCopyNotifiedKeys	(SCDynamicStoreRef	store,
					 CFArrayRef		*notifierKeys);

int
__SCDynamicStoreNotifyMachPort		(SCDynamicStoreRef	store,
					 mach_msg_id_t		msgid,
					 mach_port_t		*port);

int
__SCDynamicStoreNotifyFileDescriptor	(SCDynamicStoreRef	store,
					 int32_t		identifier,
					 int			*fd);

int
__SCDynamicStoreNotifySignal		(SCDynamicStoreRef	store,
					 pid_t			pid,
					 int			sig);

int
__SCDynamicStoreNotifyCancel		(SCDynamicStoreRef	store);

void
_swapLockedStoreData			();

void
_addWatcher				(CFNumberRef		sessionNum,
					 CFStringRef		watchedKey);

void
_addRegexWatcherByKey			(const void		*key,
					 void			*val,
					 void			*context);

void
_addRegexWatchersBySession		(const void		*key,
					 void			*val,
					 void			*context);

void
_removeWatcher				(CFNumberRef		sessionNum,
					 CFStringRef		watchedKey);

void
_removeRegexWatcherByKey		(const void		*key,
					 void			*val,
					 void			*context);

void
_removeRegexWatchersBySession		(const void		*key,
					 void			*val,
					 void			*context);

__END_DECLS

#endif /* !_S_SCD_H */
