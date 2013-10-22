/*
 * Copyright (c) 2007-2009,2012 Apple Inc. All Rights Reserved.
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

/*!
    @header SecItemServer
    The functions provided in SecItemServer.h provide an interface to 
    the backed for SecItem APIs in the server.
*/

#ifndef _SECURITYD_SECITEMSERVER_H_
#define _SECURITYD_SECITEMSERVER_H_

#include <CoreFoundation/CoreFoundation.h>
#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSCircle.h>
#include <utilities/SecDb.h>

__BEGIN_DECLS

bool _SecItemAdd(CFDictionaryRef attributes, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error);
bool _SecItemCopyMatching(CFDictionaryRef query, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error);
bool _SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups, CFErrorRef *error);
bool _SecItemDelete(CFDictionaryRef query, CFArrayRef accessGroups, CFErrorRef *error);
bool _SecItemDeleteAll(CFErrorRef *error);
bool _SecServerRestoreKeychain(CFErrorRef *error);
bool _SecServerMigrateKeychain(int32_t handle_in, CFDataRef data_in, int32_t *handle_out, CFDataRef *data_out, CFErrorRef *error);
CFDataRef _SecServerKeychainBackup(CFDataRef keybag, CFDataRef passcode, CFErrorRef *error);
bool _SecServerKeychainRestore(CFDataRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error);
bool _SecServerKeychainSyncUpdate(CFDictionaryRef updates, CFErrorRef *error);
CFDictionaryRef _SecServerBackupSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error);
bool _SecServerRestoreSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error);

// Hack to log objects from inside SOS code
void SecItemServerAppendItemDescription(CFMutableStringRef desc, CFDictionaryRef object);

// These are visible for testing.
SOSDataSourceFactoryRef SecItemDataSourceFactoryCreate(SecDbRef db);
SOSDataSourceFactoryRef SecItemDataSourceFactoryCreateDefault(void);

/* FIXME: there is a specific type for keybag handle (keybag_handle_t)
   but it's not defined for simulator so we just use an int32_t */
void SecItemServerSetKeychainKeybag(int32_t keybag);
void SecItemServerResetKeychainKeybag(void);

void SecItemServerSetKeychainChangedNotification(const char *notification_name);

CFStringRef __SecKeychainCopyPath(void);

__END_DECLS

#endif /* _SECURITYD_SECITEMSERVER_H_ */
