/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

//
//  SecItemBackup.h
//  SecItem backup restore SPIs
//

#ifndef _SECURITY_ITEMBACKUP_H_
#define _SECURITY_ITEMBACKUP_H_

#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CFString.h>

__BEGIN_DECLS

// Keys in a backup item dictionary
#define kSecItemBackupHashKey  CFSTR("hash")
#define kSecItemBackupClassKey CFSTR("class")
#define kSecItemBackupDataKey  CFSTR("data")


/* View aware backup/restore SPIs. */

#define kSecItemBackupNotification "com.apple.security.itembackup"

typedef enum SecBackupEventType {
    kSecBackupEventReset = 0,           // key is keybag
    kSecBackupEventAdd,                 // key, item are added in backup (replaces existing item with key)
    kSecBackupEventRemove,              // key gets removed from backup
    kSecBackupEventComplete             // key and value are unused
} SecBackupEventType;

bool SecItemBackupWithRegisteredBackups(CFErrorRef *error, void(^backup)(CFStringRef backupName));

/*!
 @function SecItemBackupWithChanges
 @abstract Tell securityd which keybag (via a persistent ref) to use to backup
 items for each of the built in dataSources to.
 @param backupName Name of this backup set.
 @param error Returned if there is a failure.
 @result bool standard CFError contract.
 @discussion CloudServices is expected to call this SPI to stream out changes already spooled into a backup file by securityd.  */
bool SecItemBackupWithChanges(CFStringRef backupName, CFErrorRef *error, void (^event)(SecBackupEventType et, CFTypeRef key, CFTypeRef item));

/*!
 @function SecItemBackupSetConfirmedManifest
 @abstract Tell securityd what we have in the backup for a particular backupName
 @param backupName Name of this backup set.
 @param keybagDigest The SHA1 hash of the last received keybag.
 @param manifest Manifest of the backup.
 @result bool standard CFError contract.
 @discussion cloudsvc is expected to call this SPI to whenever it thinks securityd might not be in sync with backupd of whenever it reads a backup from or writes a backup to kvs.  */
bool SecItemBackupSetConfirmedManifest(CFStringRef backupName, CFDataRef keybagDigest, CFDataRef manifest, CFErrorRef *error);

/*!
 @function SecItemBackupRestore
 @abstract Restore data from a cloudsvc backup.
 @param backupName Name of this backup set (corresponds to the view).
 @param peerID hash of the public key of the peer info matching the chosen device. For single iCSC recovery, this is the public key hash returned from SOSRegisterSingleRecoverySecret().
 @param secret Credential to unlock keybag
 @param keybag keybag for this backup
 @param backup backup to be restored
 @discussion CloudServices iterates over all the backups, calling this for each backup with peer infos matching the chosen device. */
void SecItemBackupRestore(CFStringRef backupName, CFStringRef peerID, CFDataRef keybag, CFDataRef secret, CFTypeRef backup, void (^completion)(CFErrorRef error));

/*!
 @function SecItemBackupCopyMatching
 @abstract Query the contents of a backup dictionary.
 @param keybag The bag protecting the backup data.
 @param secret Credential to unlock keybag.
 @param backup Dictionary returned from SecItemBackupDataSource.
 @param query A dictionary containing an item class specification and
 optional attributes for controlling the search. See the "Keychain
 Search Attributes" section of SecItemCopyMatching for a description of
 currently defined search attributes.
 @result CFTypeRef reference to the found item(s). The
 exact type of the result is based on the search attributes supplied
 in the query.  Returns NULL and sets *error if there is a failure.
 @discussion This allows clients to "restore" a backup and fetch an item from
 it without restoring the backup to the keychain, and in particular without
 even having a writable keychain around, such as when running in the restore OS. */
CFDictionaryRef SecItemBackupCopyMatching(CFDataRef keybag, CFDataRef secret, CFDictionaryRef backup, CFDictionaryRef query, CFErrorRef *error);

// Utility function to compute a confirmed manifest from a v0 backup dictionary.
CFDataRef SecItemBackupCreateManifest(CFDictionaryRef backup, CFErrorRef *error);

__END_DECLS

#endif /* _SECURITY_ITEMBACKUP_H_ */
