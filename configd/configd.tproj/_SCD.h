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


#ifndef _S_SCD_H
#define _S_SCD_H

#include <sys/cdefs.h>


/*
 * keys in the "cacheData" dictionary
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


extern CFMutableDictionaryRef	cacheData;
extern CFMutableDictionaryRef	sessionData;
extern CFMutableSetRef		changedKeys;
extern CFMutableSetRef		deferredRemovals;
extern CFMutableSetRef		removedSessionKeys;
extern CFMutableSetRef		needsNotification;

extern CFMutableDictionaryRef	cacheData_s;
extern CFMutableSetRef		changedKeys_s;
extern CFMutableSetRef		deferredRemovals_s;
extern CFMutableSetRef		removedSessionKeys_s;

/*
 * "context" argument for CFDictionaryArrayApplier _addRegexWatcherByKey(),
 * _addRegexWatcherBySession(), and _removeRegexWatcherByKey() functions.
 */
typedef struct {
	SCDSessionPrivateRef	session;
	regex_t			*preg;
} addContext, *addContextRef;


typedef struct {
	SCDSessionPrivateRef	session;
	regex_t			*preg;
} removeContext, *removeContextRef;


__BEGIN_DECLS

SCDStatus	_SCDOpen			__P((SCDSessionRef	*session,
						     CFStringRef	name));

SCDStatus	_SCDClose			__P((SCDSessionRef	*session));

SCDStatus	_SCDLock			__P((SCDSessionRef	session));

SCDStatus	_SCDUnlock			__P((SCDSessionRef	session));

SCDStatus	_SCDList			__P((SCDSessionRef	session,
						     CFStringRef	key,
						     int		regexOptions,
						     CFArrayRef		*subKeys));

SCDStatus	_SCDAdd				__P((SCDSessionRef	session,
						     CFStringRef	key,
						     SCDHandleRef	handle));

SCDStatus	_SCDAddSession			__P((SCDSessionRef	session,
						     CFStringRef	key,
						     SCDHandleRef	handle));

SCDStatus	_SCDGet				__P((SCDSessionRef	session,
						     CFStringRef	key,
						     SCDHandleRef	*handle));

SCDStatus	_SCDSet				__P((SCDSessionRef	session,
						     CFStringRef	key,
						     SCDHandleRef	handle));

SCDStatus	_SCDRemove			__P((SCDSessionRef	session,
						     CFStringRef	key));

SCDStatus	_SCDTouch			__P((SCDSessionRef	session,
						     CFStringRef	key));

SCDStatus	_SCDSnapshot			__P((SCDSessionRef	session));

SCDStatus	_SCDNotifierList		__P((SCDSessionRef	session,
						     int		regexOptions,
						     CFArrayRef		*notifierKeys));

SCDStatus	_SCDNotifierAdd			__P((SCDSessionRef	session,
						     CFStringRef	key,
						     int		regexOptions));

SCDStatus	_SCDNotifierRemove		__P((SCDSessionRef	session,
						     CFStringRef	key,
						     int		regexOptions));

SCDStatus	_SCDNotifierGetChanges		__P((SCDSessionRef	session,
						     CFArrayRef		*notifierKeys));

SCDStatus	_SCDNotifierInformViaMachPort	__P((SCDSessionRef	session,
						     mach_msg_id_t	msgid,
						     mach_port_t	*port));

SCDStatus	_SCDNotifierInformViaFD		__P((SCDSessionRef	session,
						     int32_t		identifier,
						     int		*fd));

SCDStatus	_SCDNotifierInformViaSignal	__P((SCDSessionRef	session,
						     pid_t		pid,
						     int		sig));

SCDStatus	_SCDNotifierCancel		__P((SCDSessionRef	session));

void		_swapLockedCacheData		__P(());

void		_addWatcher			__P((CFNumberRef	sessionNum,
						     CFStringRef	watchedKey));

void		_addRegexWatcherByKey		__P((const void		*key,
						     void		*val,
						     void		*context));

void		_addRegexWatchersBySession	__P((const void		*key,
						     void		*val,
						     void		*context));

void		_removeWatcher			__P((CFNumberRef	sessionNum,
						     CFStringRef	watchedKey));

void		_removeRegexWatcherByKey	__P((const void		*key,
						     void		*val,
						     void		*context));

void		_removeRegexWatchersBySession	__P((const void		*key,
						     void		*val,
						     void		*context));

__END_DECLS

#endif /* !_S_SCD_H */
