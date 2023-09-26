/*
 * Copyright (c) 2000-2023 Apple Inc. All rights reserved.
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
 * May 21, 2021			Allan Nathanson <ajn@apple.com>
 * - add access controls
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * June 2, 2000			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#ifndef _S_SCD_H
#define _S_SCD_H

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>

#define APP_CLIP_ENTITLEMENT				\
	"com.apple.security.on-demand-install-capable"
#define DEVICE_NAME_PUBLIC_ENTITLEMENT			\
	"com.apple.developer.device-information.user-assigned-device-name"

/*
 * keys in the "storeData" dictionary
 */

/*
 * data associated with a key
 */
#define	kSCDData		CFSTR("data")

/*
 * access controls associated with the key
 */
#define kSCDAccessControls	CFSTR("access-controls")

/*
 * The kSCDAccessControls dictionary can include any of the
 * following keys :
 *
 *   read-deny			<array of entitlements>
 *     A process is denied read access if it has one of the specified
 *     entitlements.
 *
 *   read-deny-background
 *     A process is denied read access if it is a BackgroundAssets Extension.
 *
 *   read-allow			<array of entitlements>
 *     A process is allowed read access if it has one of the specified
 *     entitlements. A process without any of these entitlements is denied
 *     read access except if read-allow-platform or read-allow-system
 *     is applicable.
 *
 *   read-allow-platform
 *     If read-allow is specified, but the process is a platform binary,
 *     read is allowed but will generate a once-per-launch fault on the
 *     client side.
 *
 *   read-allow-system
 *     If read-allow is specified, but the process is a system process,
 *     read is allowed.
 *
 *   write-protect
 *     A process is only allowed write access if it has the SCDynamicStore
 *     write entitlement for the specific key. For example, to allow
 *     loginwindow to write the console users key, it must have this
 *     entitlement:
 *
 *	<key>com.apple.SystemConfiguration.SCDynamicStore-write-access</key>
 *	<dict>
 *		<key>keys</key>
 *		<array>
 *			<string>State:/Users/ConsoleUser</string>
 *		</array>
 *	</dict>
 */
#define kSCDAccessControls_readDeny		CFSTR("read-deny")
#define kSCDAccessControls_readDenyBackground	CFSTR("read-deny-background")
#define kSCDAccessControls_readAllow		CFSTR("read-allow")
#define kSCDAccessControls_readAllowPlatform	CFSTR("read-allow-platform")
#define kSCDAccessControls_readAllowSystem	CFSTR("read-allow-system")
#define kSCDAccessControls_writeProtect		CFSTR("write-protect")
/*
 * client session ids watching a key and, since we can possibly have
 * multiple regex keys which reference the key, a count of active
 * references
 */
#define	kSCDWatchers		CFSTR("watchers")
#define	kSCDWatcherRefs		CFSTR("watcherRefs")

/*
 * client session id for per-session keys.
 */
#define	kSCDSession		CFSTR("session")


extern CFMutableDictionaryRef	storeData;
extern CFMutableDictionaryRef	patternData;
extern CFMutableSetRef		changedKeys;
extern CFMutableSetRef		deferredRemovals;
extern CFMutableSetRef		removedSessionKeys;
extern CFMutableSetRef		needsNotification;


__BEGIN_DECLS

void
__SCDynamicStoreInit			(void);

int
__SCDynamicStoreOpen			(SCDynamicStoreRef	*store,
					 CFStringRef		name);
int
__SCDynamicStoreClose			(SCDynamicStoreRef	*store);

int
__SCDynamicStorePush			(void);

int
__SCDynamicStoreCopyKeyList		(SCDynamicStoreRef	store,
					 CFStringRef		prefix,
					 Boolean		isRegex,
					 CFArrayRef		*subKeys);

int
__SCDynamicStoreAddValue		(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 CFDataRef		value);

int
__SCDynamicStoreCopyValue		(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 CFDictionaryRef	*key_controls,
					 CFDataRef		*value,
					 Boolean		internal);

int
__SCDynamicStoreSetValue		(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 CFDataRef		value,
					 Boolean		internal);

int
__SCDynamicStoreSetMultiple		(SCDynamicStoreRef	store,
					 CFDictionaryRef	keysToSet,
					 CFArrayRef		keysToRemove,
					 CFArrayRef		keysToNotify);

int
__SCDynamicStoreRemoveValue		(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 Boolean		internal);

int
__SCDynamicStoreNotifyValue		(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 Boolean		internal);

int
__SCDynamicStoreSnapshot		(SCDynamicStoreRef	store);

int
__SCDynamicStoreAddWatchedKey		(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 Boolean		isRegex,
					 Boolean		internal);

int
__SCDynamicStoreRemoveWatchedKey	(SCDynamicStoreRef	store,
					 CFStringRef		key,
					 Boolean		isRegex,
					 Boolean		internal);

int
__SCDynamicStoreSetNotificationKeys	(SCDynamicStoreRef	store,
					 CFArrayRef		keys,
					 CFArrayRef		patterns);

int
__SCDynamicStoreCopyNotifiedKeys	(SCDynamicStoreRef	store,
					 CFArrayRef		*notifierKeys);

int
__SCDynamicStoreNotifyMachPort		(SCDynamicStoreRef	store,
					 mach_msg_id_t		msgid,
					 mach_port_t		port);

int
__SCDynamicStoreNotifyFileDescriptor	(SCDynamicStoreRef	store);

int
__SCDynamicStoreNotifyCancel		(SCDynamicStoreRef	store);

void
_storeAddWatcher			(CFNumberRef		sessionNum,
					 CFStringRef		watchedKey);

void
_storeRemoveWatcher			(CFNumberRef		sessionNum,
					 CFStringRef		watchedKey);

CFDictionaryRef
_storeKeyGetAccessControls		(CFStringRef		key);

void
_storeKeySetAccessControls		(CFStringRef		key,
					 CFDictionaryRef	controls);

void
pushNotifications			(void);

__END_DECLS

#endif	/* !_S_SCD_H */
