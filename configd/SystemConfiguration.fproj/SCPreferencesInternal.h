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

#ifndef _SCPREFERENCESINTERNAL_H
#define _SCPREFERENCESINTERNAL_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SCPreferences.h>
#include <SystemConfiguration/SCDynamicStore.h>


#define	PREFS_DEFAULT_DIR	CFSTR("/var/db/SystemConfiguration")
#define	PREFS_DEFAULT_CONFIG	CFSTR("preferences.xml")

#define	PREFS_DEFAULT_USER_DIR	CFSTR("Library/Preferences")


/* Define the per-preference-handle structure */
typedef struct {

	/* base CFType information */
	CFRuntimeBase		cfBase;

	/* session name */
	CFStringRef		name;

	/* preferences ID */
	CFStringRef		prefsID;

	/* per-user preference info */
	Boolean			perUser;
	CFStringRef		user;

	/* configuration file path */
	char			*path;

	/* configuration file signature */
	CFDataRef		signature;

	/* configd session */
	SCDynamicStoreRef	session;

	/* configd session keys */
	CFStringRef		sessionKeyLock;
	CFStringRef		sessionKeyCommit;
	CFStringRef		sessionKeyApply;

	/* preferences */
	CFMutableDictionaryRef	prefs;

	/* flags */
	Boolean			accessed;
	Boolean			changed;
	Boolean			locked;
	Boolean			isRoot;

} SCPreferencesPrivate, *SCPreferencesPrivateRef;


/* Define signature data */
typedef struct {
	dev_t			st_dev;		/* inode's device */
	ino_t			st_ino;		/* inode's number */
	struct  timespec	st_mtimespec;	/* time of last data modification */
	off_t			st_size;	/* file size, in bytes */
} SCPSignatureData, *SCPSignatureDataRef;


__BEGIN_DECLS

SCPreferencesRef
__SCPreferencesCreate			(CFAllocatorRef		allocator,
					 CFStringRef		name,
					 CFStringRef		prefsID,
					 Boolean		perUser,
					 CFStringRef		user);

CFDataRef
__SCPSignatureFromStatbuf		(const struct stat	*statBuf);

char *
__SCPreferencesPath			(CFAllocatorRef		allocator,
					 CFStringRef		prefsID,
					 Boolean		perUser,
					 CFStringRef		user);

CFStringRef
_SCPNotificationKey			(CFAllocatorRef		allocator,
					 CFStringRef		prefsID,
					 Boolean		perUser,
					 CFStringRef		user,
					 int			keyType);

__END_DECLS

#endif /* _SCPREFERENCESINTERNAL_H */
