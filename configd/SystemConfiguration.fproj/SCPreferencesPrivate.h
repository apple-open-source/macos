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

#ifndef _SCPREFERENCESPRIVATE_H
#define _SCPREFERENCESPRIVATE_H

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPreferences.h>


/*!
	@enum SCPreferencesKeyType
	@discussion Used with the SCDynamicStoreKeyCreatePreferences() function
		to describe the resulting CFStringRef argument.
	@constant kSCPreferencesKeyLock Key used when exclusive access to the
		stored preferences is obtained or released.
	@constant kSCPreferencesKeyCommit Key used when new preferences are
		committed to the store
	@constant kSCPreferencesKeyApply Key used when new preferences are
		to be applied to the active system configuration.
 */
typedef enum {
	kSCPreferencesKeyLock	= 1,
	kSCPreferencesKeyCommit	= 2,
	kSCPreferencesKeyApply	= 3,
} SCPreferencesKeyType;


__BEGIN_DECLS

/*!
	@function SCDynamicStoreKeyCreatePreferences
	@discussion Creates a key that can be used by the SCDynamicStoreSetNotificationKeys()
		function to receive notifications of changes to the saved
		preferences.
	@param allocator ...
	@param prefsID A string that identifies the name of the
		group of preferences to be accessed/updated.
	@param keyType A kSCPreferencesKeyType indicating the type a notification
		key to be returned.
	@result A notification string for the specified preference identifier.
 */
CFStringRef
SCDynamicStoreKeyCreatePreferences	(CFAllocatorRef		allocator,
					 CFStringRef		prefsID,
					 int			keyType);

SCPreferencesRef
SCUserPreferencesCreate			(CFAllocatorRef		allocator,
					 CFStringRef		name,
					 CFStringRef		prefsID,
					 CFStringRef		user);

CFStringRef
SCDynamicStoreKeyCreateUserPreferences	(CFAllocatorRef		allocator,
					 CFStringRef		prefsID,
					 CFStringRef		user,
					 int			keyType);

__END_DECLS

#endif /* _SCPREFERENCESPRIVATE_H */
