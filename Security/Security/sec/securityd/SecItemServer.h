/*
 * Copyright (c) 2007-2009,2012-2014 Apple Inc. All Rights Reserved.
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
    the backend for SecItem APIs in the server.
*/

#ifndef _SECURITYD_SECITEMSERVER_H_
#define _SECURITYD_SECITEMSERVER_H_

#include <CoreFoundation/CoreFoundation.h>
#include <SecureObjectSync/SOSCircle.h>
#include <securityd/SecDbQuery.h>
#include <utilities/SecDb.h>

__BEGIN_DECLS

bool _SecItemAdd(CFDictionaryRef attributes, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error);
bool _SecItemCopyMatching(CFDictionaryRef query, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error);
bool _SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups, CFErrorRef *error);
bool _SecItemDelete(CFDictionaryRef query, CFArrayRef accessGroups, CFErrorRef *error);
bool _SecItemDeleteAll(CFErrorRef *error);
bool _SecServerRestoreKeychain(CFErrorRef *error);
bool _SecServerMigrateKeychain(int32_t handle_in, CFDataRef data_in, int32_t *handle_out, CFDataRef *data_out, CFErrorRef *error);
CF_RETURNS_RETAINED CFDataRef _SecServerKeychainBackup(CFDataRef keybag, CFDataRef passcode, CFErrorRef *error);
bool _SecServerKeychainRestore(CFDataRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error);
CF_RETURNS_RETAINED CFArrayRef _SecServerKeychainSyncUpdateKeyParameter(CFDictionaryRef updates, CFErrorRef *error);
CF_RETURNS_RETAINED CFArrayRef _SecServerKeychainSyncUpdateCircle(CFDictionaryRef updates, CFErrorRef *error);
CF_RETURNS_RETAINED CFArrayRef _SecServerKeychainSyncUpdateMessage(CFDictionaryRef updates, CFErrorRef *error);
CF_RETURNS_RETAINED CFDictionaryRef _SecServerBackupSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error);

bool _SecServerRestoreSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error);

bool _SecAddSharedWebCredential(CFDictionaryRef attributes, const audit_token_t *clientAuditToken, CFStringRef appID, CFArrayRef domains, CFTypeRef *result, CFErrorRef *error);
bool _SecCopySharedWebCredential(CFDictionaryRef query, const audit_token_t *clientAuditToken, CFStringRef appID, CFArrayRef domains, CFTypeRef *result, CFErrorRef *error);

// Hack to log objects from inside SOS code
void SecItemServerAppendItemDescription(CFMutableStringRef desc, CFDictionaryRef object);

SecDbRef SecKeychainDbCreate(CFStringRef path);

SOSDataSourceFactoryRef SecItemDataSourceFactoryGetDefault(void);

/* FIXME: there is a specific type for keybag handle (keybag_handle_t)
   but it's not defined for simulator so we just use an int32_t */
void SecItemServerSetKeychainKeybag(int32_t keybag);
void SecItemServerResetKeychainKeybag(void);

void SecItemServerSetKeychainChangedNotification(const char *notification_name);

CFStringRef __SecKeychainCopyPath(void);

bool _SecServerRollKeys(bool force, CFErrorRef *error);


// Should all be blocks called from SecItemDb
bool match_item(SecDbConnectionRef dbt, Query *q, CFArrayRef accessGroups, CFDictionaryRef item);
bool itemInAccessGroup(CFDictionaryRef item, CFArrayRef accessGroups);
void SecKeychainChanged(bool syncWithPeers);

__END_DECLS

#endif /* _SECURITYD_SECITEMSERVER_H_ */
