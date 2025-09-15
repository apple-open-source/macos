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
#include "keychain/SecureObjectSync/SOSCircle.h"
#include "keychain/securityd/SecDbQuery.h"
#include "utilities/SecDb.h"
#include <TargetConditionals.h>
#include "sec/ipc/securityd_client.h"


__BEGIN_DECLS

bool SecServerItemAdd(CFDictionaryRef attributes, SecurityClient *client, CFTypeRef *result, CFErrorRef *error);
bool SecServerItemCopyMatching(CFDictionaryRef query, SecurityClient *client, CFTypeRef *result, CFErrorRef *error);
bool SecServerItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate, SecurityClient *client, CFErrorRef *error);
bool SecServerItemDelete(CFDictionaryRef query, SecurityClient *client, CFErrorRef *error);
bool SecServerItemDeleteAll(CFErrorRef *error);
bool SecServerItemDeleteAllWithAccessGroups(CFArrayRef accessGroups, SecurityClient *client, CFErrorRef *error);
CFTypeRef SecServerItemShareWithGroup(CFDictionaryRef query, CFStringRef sharingGroup, SecurityClient *client, CFErrorRef *error) CF_RETURNS_RETAINED;
bool SecServerDeleteItemsOnSignOut(SecurityClient *client, CFErrorRef *error);

bool SecServerRestoreKeychain(CFErrorRef *error);
bool SecServerMigrateKeychain(int32_t handle_in, CFDataRef data_in, int32_t *handle_out, CFDataRef *data_out, CFErrorRef *error);
CFDataRef SecServerKeychainCreateBackup(SecurityClient *client, CFDataRef keybag, CFDataRef passcode, bool emcs, CFErrorRef *error);
bool SecServerKeychainRestore(CFDataRef backup, SecurityClient *client, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error);
CFStringRef SecServerBackupCopyUUID(CFDataRef backup, CFErrorRef *error);

bool SecServerBackupKeybagAdd(SecurityClient *client, CFDataRef passcode, CFDataRef *identifier, CFDataRef *pathinfo, CFErrorRef *error);
bool SecServerBackupKeybagDelete(CFDictionaryRef attributes, bool deleteAll, CFErrorRef *error);

bool SecItemServerUpdateTokenItemsForAccessGroups(CFStringRef tokenID, CFArrayRef accessGroups, CFArrayRef items, SecurityClient *client, CFErrorRef *error);
bool SecItemServerUpdateTokenItemsForSystemKeychain(CFStringRef tokenID, CFArrayRef accessGroups, CFArrayRef items, SecurityClient *client, CFErrorRef *error);

CF_RETURNS_RETAINED CFArrayRef SecServerKeychainSyncUpdateMessage(CFDictionaryRef updates, CFErrorRef *error);
CF_RETURNS_RETAINED CFDictionaryRef SecServerBackupSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error);

int SecServerKeychainTakeOverBackupFD(CFStringRef backupName, CFErrorRef *error);

bool SecServerRestoreSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error);

#if TARGET_OS_IOS
bool SecServerTransmogrifyToSystemKeychain(SecurityClient *client, CFErrorRef *error);
bool SecServerTranscryptToSystemKeychainKeybag(SecurityClient *client, CFErrorRef *error);
bool SecServerTransmogrifyToSyncBubble(CFArrayRef services, uid_t uid, SecurityClient *client, CFErrorRef *error);
bool SecServerDeleteMUSERViews(SecurityClient *client, uid_t uid, CFErrorRef *error);
#endif

#if SHAREDWEBCREDENTIALS
bool SecServerAddSharedWebCredential(CFDictionaryRef attributes, SecurityClient *client, const audit_token_t *clientAuditToken, CFStringRef appID, CFArrayRef domains, CFTypeRef *result, CFErrorRef *error);
#endif /* SHAREDWEBCREDENTIALS */

// Hack to log objects from inside SOS code
void SecItemServerAppendItemDescription(CFMutableStringRef desc, CFDictionaryRef object);

SecDbRef SecServerKeychainDbCreate(CFStringRef path, CFErrorRef* error);
SecDbRef SecServerKeychainDbInitialize(SecDbRef db);

bool kc_with_dbt(bool writeAndRead, SecDbRef customDB, CFErrorRef *error, bool (^perform)(SecDbConnectionRef dbt));
bool kc_with_dbt_non_item_tables(bool writeAndRead, SecDbRef customDB, CFErrorRef* error, bool (^perform)(SecDbConnectionRef dbt)); // can be used when only tables which don't store 'items' are accessed - avoids invoking SecItemServerDataSourceFactoryGetDefault()
bool kc_with_custom_db(bool writeAndRead, bool usesItemTables, SecDbRef db, CFErrorRef *error, bool (^perform)(SecDbConnectionRef dbt));

bool SecServerUpgradeItemPhase3(SecDbConnectionRef inDbt, bool *inProgress, CFErrorRef *error);

// returns whether or not it succeeeded
// if the inProgress bool is set, then an attempt to reinvoke this routine will occur sometime in the near future
// error to be filled in if any upgrade attempt resulted in an error
// this will always return true because upgrade phase3 always returns true
bool SecServerKeychainUpgradePersistentReferences(bool *inProgress, CFErrorRef *error);

/* For open box testing only */
SecDbRef SecServerKeychainDbGetDb(CFErrorRef* error);
void SecServerKeychainDbForceClose(void);
void SecServerKeychainDelayAsyncBlocks(bool);
void SecServerKeychainDbWaitForAsyncBlocks(void);
void SecServerKeychainDbReset(dispatch_block_t inbetween);

/* V V test routines V V */
void SecServerClearLastRowIDHandledForTests(void);
CFNumberRef SecServerLastRowIDHandledForTests(void);
void SecServerSetExpectedErrorForTests(CFErrorRef error);
void SecServerClearTestError(void);
void SecServerSetRowIDToErrorDictionary(CFDictionaryRef rowIDToErrorDictionary);
void SecServerClearRowIDAndErrorDictionary(void);
/* ^ ^ test routines ^ ^*/

SOSDataSourceFactoryRef SecItemServerDataSourceFactoryGetDefault(void);

/* FIXME: there is a specific type for keybag handle (keybag_handle_t)
   but it's not defined for simulator so we just use an int32_t */
void SecItemServerSetKeychainKeybag(int32_t keybag);
void SecItemServerSetKeychainKeybagToDefault(void);

void SecItemServerSetKeychainChangedNotification(const char *notification_name);
/// Overrides the notification center to use for the "shared items changed"
/// notification. Defaults to the distributed notification center if `NULL`.
void SecServerSetSharedItemNotifier(CFNotificationCenterRef notifier);

CFStringRef SecServerKeychainCopyPath(void);

bool SecServerRollKeys(bool force, SecurityClient *client, CFErrorRef *error);
bool SecServerRollKeysGlue(bool force, CFErrorRef *error);


/* initial sync */
#define SecServerInitialSyncCredentialFlagTLK                (1 << 0)
#define SecServerInitialSyncCredentialFlagPCS                (1 << 1)
#define SecServerInitialSyncCredentialFlagPCSNonCurrent      (1 << 2)
#define SecServerInitialSyncCredentialFlagBluetoothMigration (1 << 3)

#define PERSISTENT_REF_UUID_BYTES_LENGTH (sizeof(uuid_t))

CFArrayRef SecServerCopyInitialSyncCredentials(uint32_t flags, uint64_t* tlks, uint64_t* pcs, uint64_t* bluetooth, CFErrorRef *error);
bool SecServerImportInitialSyncCredentials(CFArrayRef array, CFErrorRef *error);

CF_RETURNS_RETAINED CFArrayRef SecItemServerCopyParentCertificates(CFDataRef normalizedIssuer, CFArrayRef accessGroups, CFErrorRef *error);
bool SecItemServerCertificateExists(CFDataRef normalizedIssuer, CFDataRef serialNumber, CFArrayRef accessGroups, CFErrorRef *error);

bool SecServerKeychainDbGetVersion(SecDbConnectionRef dbt, int *version, CFErrorRef *error);


// Should all be blocks called from SecItemDb
bool SecServerMatch_Item(SecDbConnectionRef dbt, Query *q, CFArrayRef accessGroups, CFDictionaryRef item);
bool SecServerAccessGroupsAllows(CFArrayRef accessGroups, CFStringRef accessGroup, SecurityClient* client);
bool SecServerItemInAccessGroup(CFDictionaryRef item, CFArrayRef accessGroups);
void SecServerKeychainChanged(void);
void SecServerSharedItemsChanged(void);

void SecServerDeleteCorruptedItemAsync(SecDbConnectionRef dbt, CFStringRef tablename, sqlite_int64 rowid);

CFDataRef SecServerUUIDDataCreate(void);

// Allows to interact with custom db
bool SecServerItemAddWithCustomDb(CFDictionaryRef attributes, SecDbRef db, SecurityClient *client, CFTypeRef *result, CFErrorRef *error);
bool SecServerItemCopyMatchingWithCustomDb(CFDictionaryRef query, SecDbRef db, CFTypeRef *result, SecurityClient *client, CFErrorRef *error);
bool SecServerItemUpdateWithCustomDb(CFDictionaryRef query, SecDbRef db, CFDictionaryRef attributesToUpdate, SecurityClient *client, CFErrorRef *error);
bool SecServerItemDeleteWithCustomDb(CFDictionaryRef query, SecDbRef db, SecurityClient *client, CFErrorRef *error);
__END_DECLS

#endif /* _SECURITYD_SECITEMSERVER_H_ */
