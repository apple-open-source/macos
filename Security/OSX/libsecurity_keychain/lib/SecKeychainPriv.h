/*
 * Copyright (c) 2003-2004,2011-2014 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_SECKEYCHAINPRIV_H_
#define _SECURITY_SECKEYCHAINPRIV_H_

#include <Security/Security.h>
#include <Security/SecBasePriv.h>
#include <Security/SecKeychain.h>
#include <CoreFoundation/CoreFoundation.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {kSecKeychainEnteredBatchModeEvent = 14,
	  kSecKeychainLeftBatchModeEvent = 15};
enum {kSecKeychainEnteredBatchModeEventMask = 1 << kSecKeychainEnteredBatchModeEvent,
	  kSecKeychainLeftBatchModeEventMask = 1 << kSecKeychainLeftBatchModeEvent};


/* Keychain management */
OSStatus SecKeychainCreateNew(SecKeychainRef keychainRef, UInt32 passwordLength, const char* inPassword)
API_DEPRECATED("SecKeychain is deprecated", macos(10.4, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainMakeFromFullPath(const char *fullPathName, SecKeychainRef *keychainRef)
API_DEPRECATED("SecKeychain is deprecated", macos(10.4, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainIsValid(SecKeychainRef keychainRef, Boolean* isValid)
API_DEPRECATED("SecKeychain is deprecated", macos(10.4, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainChangePassword(SecKeychainRef keychainRef, UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword)
API_DEPRECATED("SecKeychain is deprecated", macos(10.2, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainSetBatchMode (SecKeychainRef kcRef, Boolean mode, Boolean rollback)
API_DEPRECATED("SecKeychain is deprecated", macos(10.5, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/* Keychain list management */
UInt16 SecKeychainListGetCount(void)
API_DEPRECATED("SecKeychain is deprecated", macos(10.2, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainListCopyKeychainAtIndex(UInt16 index, SecKeychainRef *keychainRef)
API_DEPRECATED("SecKeychain is deprecated", macos(10.2, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainListRemoveKeychain(SecKeychainRef *keychainRef)
API_DEPRECATED("SecKeychain is deprecated", macos(10.2, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainRemoveFromSearchList(SecKeychainRef keychainRef)
API_DEPRECATED("SecKeychain is deprecated", macos(10.4, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/* Login keychain support */
OSStatus SecKeychainLogin(UInt32 nameLength, const void* name, UInt32 passwordLength, const void* password)
API_DEPRECATED("SecKeychain is deprecated", macos(10.2, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainStash(void)
API_DEPRECATED("SecKeychain is deprecated", macos(10.9, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainLogout(void)
API_DEPRECATED("SecKeychain is deprecated", macos(10.2, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainCopyLogin(SecKeychainRef *keychainRef)
API_DEPRECATED("SecKeychain is deprecated", macos(10.2, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainResetLogin(UInt32 passwordLength, const void* password, Boolean resetSearchList)
API_DEPRECATED("SecKeychain is deprecated", macos(10.3, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

OSStatus SecKeychainVerifyKeyStorePassphrase(uint32_t retries)
API_DEPRECATED("SecKeychain is deprecated", macos(10.9, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainChangeKeyStorePassphrase(void)
API_DEPRECATED("SecKeychain is deprecated", macos(10.9, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/* Keychain synchronization */
enum {
  kSecKeychainNotSynchronized = 0,
  kSecKeychainSynchronizedWithDotMac = 1
};
typedef UInt32 SecKeychainSyncState;

OSStatus SecKeychainCopySignature(SecKeychainRef keychainRef, CFDataRef *keychainSignature)
API_DEPRECATED("SecKeychain is deprecated", macos(10.4, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainCopyBlob(SecKeychainRef keychainRef, CFDataRef *dbBlob)
API_DEPRECATED("SecKeychain is deprecated", macos(10.4, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainRecodeKeychain(SecKeychainRef keychainRef, CFArrayRef dbBlobArray, CFDataRef extraData)
API_DEPRECATED("SecKeychain is deprecated", macos(10.6, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
OSStatus SecKeychainCreateWithBlob(const char* fullPathName, CFDataRef dbBlob, SecKeychainRef *kcRef)
API_DEPRECATED("SecKeychain is deprecated", macos(10.4, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/* Keychain list manipulation */
OSStatus SecKeychainAddDBToKeychainList (SecPreferencesDomain domain, const char* dbName, const CSSM_GUID *guid, uint32 subServiceType)
API_DEPRECATED("SecKeychain is deprecated", macos(10.4, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

OSStatus SecKeychainDBIsInKeychainList (SecPreferencesDomain domain, const char* dbName, const CSSM_GUID *guid, uint32 subServiceType)
API_DEPRECATED("SecKeychain is deprecated", macos(10.4, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

OSStatus SecKeychainRemoveDBFromKeychainList (SecPreferencesDomain domain, const char* dbName, const CSSM_GUID *guid, uint32 subServiceType)
API_DEPRECATED("SecKeychain is deprecated", macos(10.4, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/* server operation (keychain inhibit) */
void SecKeychainSetServerMode(void)
API_DEPRECATED("SecKeychain is deprecated", macos(10.5, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/* special calls */
OSStatus SecKeychainCleanupHandles(void)
API_DEPRECATED("SecKeychain is deprecated", macos(10.5, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

OSStatus SecKeychainSystemKeychainCheckWouldDeadlock(void)
API_DEPRECATED("SecKeychain is deprecated", macos(10.7, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

OSStatus SecKeychainStoreUnlockKey(SecKeychainRef userKeychainRef, SecKeychainRef systemKeychainRef, CFStringRef username, CFStringRef password)
API_DEPRECATED("SecKeychain is deprecated", macos(10.10, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/* Token login support */
OSStatus SecKeychainStoreUnlockKeyWithPubKeyHash(CFDataRef pubKeyHash, CFStringRef tokenID, CFDataRef wrapPubKeyHash, SecKeychainRef userKeychain, CFStringRef password)
API_DEPRECATED("SecKeychain is deprecated", macos(10.12, 10.12))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

OSStatus SecKeychainStoreUnlockKeyWithPubKeyHashAndPassword(CFDataRef pubKeyHash, CFStringRef tokenID, CFDataRef wrapPubKeyHash, SecKeychainRef userKeychain, CFStringRef keychainPassword, CFStringRef userPassword)
API_DEPRECATED("SecKeychain is deprecated", macos(10.12, 10.12))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

OSStatus SecKeychainEraseUnlockKeyWithPubKeyHash(CFDataRef pubKeyHash)
API_DEPRECATED("SecKeychain is deprecated", macos(10.12, 10.12))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/* calls to interact with keychain versions */
OSStatus SecKeychainGetKeychainVersion(SecKeychainRef keychain, UInt32* version)
API_DEPRECATED("SecKeychain is deprecated", macos(10.11, 10.11))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

OSStatus SecKeychainAttemptMigrationWithMasterKey(SecKeychainRef keychain, UInt32 version, const char* masterKeyFilename)
API_DEPRECATED("SecKeychain is deprecated", macos(10.11, 10.11))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/* calls for testing only */
OSStatus SecKeychainGetUserPromptAttempts(uint32_t* attempts)
API_DEPRECATED("SecKeychain is deprecated", macos(10.12, 10.12))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/*!
 @function SecKeychainMDSInstall
 Set up MDS.
 */
OSStatus SecKeychainMDSInstall(void)
API_DEPRECATED("SecKeychain is deprecated", macos(10.2, 10.10))
API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECKEYCHAINPRIV_H_ */
