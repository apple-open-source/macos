/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
#include <CoreFoundation/CoreFoundation.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {kSecKeychainEnteredBatchModeEvent = 14,
	  kSecKeychainLeftBatchModeEvent = 15};
enum {kSecKeychainEnteredBatchModeEventMask = 1 << kSecKeychainEnteredBatchModeEvent,
	  kSecKeychainLeftBatchModeEventMask = 1 << kSecKeychainLeftBatchModeEvent};


/* Keychain management */
OSStatus SecKeychainCreateNew(SecKeychainRef keychainRef, UInt32 passwordLength, const char* inPassword);
OSStatus SecKeychainMakeFromFullPath(const char *fullPathName, SecKeychainRef *keychainRef);
OSStatus SecKeychainIsValid(SecKeychainRef keychainRef, Boolean* isValid);
OSStatus SecKeychainChangePassword(SecKeychainRef keychainRef, UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword);
OSStatus SecKeychainOpenWithGuid(const CSSM_GUID *guid, uint32 subserviceId, uint32 subserviceType, const char* dbName, const CSSM_NET_ADDRESS *dbLocation, SecKeychainRef *keychain);
OSStatus SecKeychainSetBatchMode (SecKeychainRef kcRef, Boolean mode, Boolean rollback);

/* Keychain list management */
UInt16 SecKeychainListGetCount(void);
OSStatus SecKeychainListCopyKeychainAtIndex(UInt16 index, SecKeychainRef *keychainRef);
OSStatus SecKeychainListRemoveKeychain(SecKeychainRef *keychainRef);
OSStatus SecKeychainRemoveFromSearchList(SecKeychainRef keychainRef);

/* Login keychain support */
OSStatus SecKeychainLogin(UInt32 nameLength, const void* name, UInt32 passwordLength, const void* password);
OSStatus SecKeychainLogout();
OSStatus SecKeychainCopyLogin(SecKeychainRef *keychainRef);
OSStatus SecKeychainResetLogin(UInt32 passwordLength, const void* password, Boolean resetSearchList);

/* Keychain synchronization */
enum {
  kSecKeychainNotSynchronized = 0,
  kSecKeychainSynchronizedWithDotMac = 1
};
typedef UInt32 SecKeychainSyncState;

OSStatus SecKeychainCopySignature(SecKeychainRef keychainRef, CFDataRef *keychainSignature);
OSStatus SecKeychainCopyBlob(SecKeychainRef keychainRef, CFDataRef *dbBlob);
OSStatus SecKeychainRecodeKeychain(SecKeychainRef keychainRef, CFArrayRef dbBlobArray, CFDataRef extraData);
OSStatus SecKeychainCreateWithBlob(const char* fullPathName, CFDataRef dbBlob, SecKeychainRef *kcRef);

/* Keychain list manipulation */
OSStatus SecKeychainAddDBToKeychainList (SecPreferencesDomain domain, const char* dbName, const CSSM_GUID *guid, uint32 subServiceType);
OSStatus SecKeychainDBIsInKeychainList (SecPreferencesDomain domain, const char* dbName, const CSSM_GUID *guid, uint32 subServiceType);
OSStatus SecKeychainRemoveDBFromKeychainList (SecPreferencesDomain domain, const char* dbName, const CSSM_GUID *guid, uint32 subServiceType);

/* server operation (keychain inhibit) */
void SecKeychainSetServerMode();

/* special calls */
OSStatus SecKeychainCleanupHandles();

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECKEYCHAINPRIV_H_ */
