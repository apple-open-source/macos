/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include <stdio.h>
#include <AssertMacros.h>
#include "SOSAccountPriv.h"
#include "SOSPeerInfoCollections.h"
#include "SOSTransport.h"
#import <Security/SecureObjectSync/SOSAccountTrust.h>
#import <Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h>

#define kPublicKeyAvailable "com.apple.security.publickeyavailable"
//
// MARK: User Credential management
//


// List of things to do
// Update myFullPeerInfo in circle if needed
// Fix iCloud Identity if needed
// Gen sign if private key changed

bool SOSAccountGenerationSignatureUpdate(SOSAccount* account, CFErrorRef *error) {
    bool result = false;
    SecKeyRef priv_key = SOSAccountGetPrivateCredential(account, error);
    require_quiet(priv_key, bail);

    [account.trust generationSignatureUpdateWith:account key:priv_key];
    
    result = true;
bail:
    return result;
}

/* this one is meant to be local - not published over KVS. */
static bool SOSAccountPeerSignatureUpdate(SOSAccount* account, SecKeyRef privKey, CFErrorRef *error) {
    SOSFullPeerInfoRef identity = NULL;
    SOSAccountTrustClassic *trust = account.trust;
    identity = trust.fullPeerInfo;

    return identity && SOSFullPeerInfoUpgradeSignatures(identity, privKey, error);
}


void SOSAccountPurgePrivateCredential(SOSAccount* account)
{
    secnotice("circleOps", "Purging private account credential");

    if(account.accountPrivateKey)
    {
        account.accountPrivateKey = NULL;
    }
    if(account._password_tmp)
    {
        account._password_tmp = NULL;
    }
    if (account.user_private_timer) {
        dispatch_source_cancel(account.user_private_timer);
        account.user_private_timer = NULL;
        xpc_transaction_end();
    }
    if (account.lock_notification_token != NOTIFY_TOKEN_INVALID) {
        notify_cancel(account.lock_notification_token);
        account.lock_notification_token = NOTIFY_TOKEN_INVALID;
    }
}


static void SOSAccountSetTrustedUserPublicKey(SOSAccount* account, bool public_was_trusted, SecKeyRef privKey)
{
    if (!privKey) return;
    SecKeyRef publicKey = SecKeyCreatePublicFromPrivate(privKey);
    
    if (account.accountKey && account.accountKeyIsTrusted && CFEqual(publicKey, account.accountKey)) {
        CFReleaseNull(publicKey);
        return;
    }
    
    if(public_was_trusted && account.accountKey) {
        account.previousAccountKey = account.accountKey;
    }

    account.accountKey = publicKey;
    account.accountKeyIsTrusted = true;
    
    if(!account.previousAccountKey) {
        account.previousAccountKey = account.accountKey;
    }

    CFReleaseNull(publicKey);

	secnotice("circleOps", "trusting new public key: %@", account.accountKey);
    notify_post(kPublicKeyAvailable);
}

void SOSAccountSetUnTrustedUserPublicKey(SOSAccount* account, SecKeyRef publicKey) {
    if(account.accountKeyIsTrusted && account.accountKey) {
        secnotice("circleOps", "Moving : %@ to previousAccountKey", account.accountKey);
        account.previousAccountKey = account.accountKey;
    }

    account.accountKey = publicKey;
    account.accountKeyIsTrusted = false;
    
    if(!account.previousAccountKey) {
        account.previousAccountKey = account.accountKey;
    }
    
    secnotice("circleOps", "not trusting new public key: %@", account.accountKey);
}


static void SOSAccountSetPrivateCredential(SOSAccount* account, SecKeyRef private, CFDataRef password) {
    if (!private)
        return SOSAccountPurgePrivateCredential(account);
    
    secnotice("circleOps", "setting new private credential");

    account.accountPrivateKey = private;

    if (password) {
        account._password_tmp = [[NSData alloc] initWithData:(__bridge NSData * _Nonnull)(password)];
    } else {
        account._password_tmp = NULL;
    }

    bool resume_timer = false;
    if (!account.user_private_timer) {
        xpc_transaction_begin();
        resume_timer = true;
        account.user_private_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, account.queue);
        dispatch_source_set_event_handler(account.user_private_timer, ^{
            secnotice("keygen", "Timing out, purging private account credential");
            SOSAccountPurgePrivateCredential(account);
        });
        int lockNotification;

        notify_register_dispatch(kUserKeybagStateChangeNotification, &lockNotification, account.queue, ^(int token) {
            bool locked = false;
            CFErrorRef lockCheckError = NULL;
            
            if (!SecAKSGetIsLocked(&locked, &lockCheckError)) {
                secerror("Checking for locked after change failed: %@", lockCheckError);
            }
            
            if (locked) {
                SOSAccountPurgePrivateCredential(account);
            }
        });
        [account setLock_notification_token:lockNotification];
    }

    SOSAccountRestartPrivateCredentialTimer(account);
    
    if (resume_timer)
        dispatch_resume(account.user_private_timer);
}

void SOSAccountRestartPrivateCredentialTimer(SOSAccount*  account)
{
    if (account.user_private_timer) {
        // (Re)set the timer's fire time to now + 10 minutes with a 5 second fuzz factor.
        dispatch_time_t purgeTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(10 * 60 * NSEC_PER_SEC));
        dispatch_source_set_timer(account.user_private_timer, purgeTime, DISPATCH_TIME_FOREVER, (int64_t)(5 * NSEC_PER_SEC));
    }
}

SecKeyRef SOSAccountGetPrivateCredential(SOSAccount* account, CFErrorRef* error)
{
    if (account.accountPrivateKey == NULL) {
        SOSCreateError(kSOSErrorPrivateKeyAbsent, CFSTR("Private Key not available - failed to prompt user recently"), NULL, error);
    }
    return account.accountPrivateKey;
}

CFDataRef SOSAccountGetCachedPassword(SOSAccount* account, CFErrorRef* error)
{
    if (account._password_tmp == NULL) {
        secnotice("circleOps", "Password cache expired");
    }
    return (__bridge CFDataRef)(account._password_tmp);
}
static NSString *SOSUserCredentialAccount = @"SOSUserCredential";
static NSString *SOSUserCredentialAccessGroup = @"com.apple.security.sos-usercredential";

__unused static void SOSAccountDeleteStashedAccountKey(SOSAccount* account)
{
    NSString *dsid = (__bridge NSString *)SOSAccountGetValue(account, kSOSDSIDKey, NULL);
    if (dsid == NULL)
        return;

    NSDictionary * attributes = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassInternetPassword,
        (__bridge id)kSecAttrAccount : SOSUserCredentialAccount,
        (__bridge id)kSecAttrServer : dsid,
        (__bridge id)kSecAttrAccessGroup : SOSUserCredentialAccessGroup,
    };
    (void)SecItemDelete((__bridge CFDictionaryRef)attributes);
}

void SOSAccountStashAccountKey(SOSAccount* account)
{
    OSStatus status;
    SecKeyRef user_private = account.accountPrivateKey;
    NSData *data = CFBridgingRelease(SecKeyCopyExternalRepresentation(user_private, NULL));
    if (data == NULL){
        return;
    }

    NSDictionary *attributes = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassInternetPassword,
        (__bridge id)kSecAttrAccount : SOSUserCredentialAccount,
        (__bridge id)kSecAttrIsInvisible : @YES,
        (__bridge id)kSecAttrAccessible : (__bridge id)kSecAttrAccessibleWhenUnlocked,
        (__bridge id)kSecAttrAccessGroup : SOSUserCredentialAccessGroup,
        (__bridge id)kSecAttrSysBound : @(kSecSecAttrSysBoundPreserveDuringRestore),
        (__bridge id)kSecValueData : data,
    };

    status = SecItemAdd((__bridge CFDictionaryRef)attributes, NULL);

    if (status == errSecDuplicateItem) {
        attributes = @{
                       (__bridge id)kSecClass : (__bridge id)kSecClassInternetPassword,
                       (__bridge id)kSecAttrAccount : SOSUserCredentialAccount,
                       (__bridge id)kSecAttrAccessGroup : SOSUserCredentialAccessGroup,
                       (__bridge id)kSecAttrNoLegacy : @YES,
                       };

        status = SecItemUpdate((__bridge CFDictionaryRef)attributes, (__bridge CFDictionaryRef)@{
             (__bridge id)kSecValueData : data,
             (__bridge id)kSecAttrSysBound : @(kSecSecAttrSysBoundPreserveDuringRestore)
        });

        if (status) {
            secnotice("circleOps", "Failed to update user private key to keychain: %d", (int)status);
        }
    } else if (status != 0) {
        secnotice("circleOps", "Failed to add user private key to keychain: %d", (int)status);
    }
    
    if(status == 0) {
        secnotice("circleOps", "Stored user private key stashed local keychain");
    }
    
    return;
}

SecKeyRef SOSAccountCopyStashedUserPrivateKey(SOSAccount* account, CFErrorRef *error)
{
    SecKeyRef key = NULL;
    CFDataRef data = NULL;
    OSStatus status;

    NSDictionary *attributes = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassInternetPassword,
        (__bridge id)kSecAttrAccount : SOSUserCredentialAccount,
        (__bridge id)kSecAttrAccessGroup : SOSUserCredentialAccessGroup,
        (__bridge id)kSecReturnData : @YES,
        (__bridge id)kSecMatchLimit : (__bridge id)kSecMatchLimitOne,
    };

    status = SecItemCopyMatching((__bridge CFDictionaryRef)attributes, (CFTypeRef *)&data);
    if (status) {
        SecError(status, error, CFSTR("Failed fetching account credential: %d"), (int)status);
        return NULL;
    }

    NSDictionary *keyAttributes = @{
        (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
        (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeEC,
    };


    key = SecKeyCreateWithData(data, (__bridge CFDictionaryRef)keyAttributes, error);
    CFReleaseNull(data);

    return key;
}

SecKeyRef SOSAccountGetTrustedPublicCredential(SOSAccount* account, CFErrorRef* error)
{
    if (account.accountKey == NULL || account.accountKeyIsTrusted == false) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Public Key isn't available. The iCloud Password must be provided to the syncing subsystem to repair this."), NULL, error);
        return NULL;
    }
    return account.accountKey;
}

bool SOSAccountHasPublicKey(SOSAccount* account, CFErrorRef* error)
{
    return SOSAccountGetTrustedPublicCredential(account, error);
}

static void sosAccountSetTrustedCredentials(SOSAccount* account, CFDataRef user_password, SecKeyRef user_private, bool public_was_trusted) {
    if(!SOSAccountFullPeerInfoVerify(account, user_private, NULL)){
        (void) SOSAccountPeerSignatureUpdate(account, user_private, NULL);
    }
    SOSAccountSetTrustedUserPublicKey(account, public_was_trusted, user_private);
    SOSAccountSetPrivateCredential(account, user_private, user_password);
    SOSAccountCheckForAlwaysOnViews(account);
}

static SecKeyRef sosAccountCreateKeyIfPasswordIsCorrect(SOSAccount* account, CFDataRef user_password, CFErrorRef *error) {
    SecKeyRef user_private = NULL;
    require_quiet(account.accountKey && account.accountKeyDerivationParamters, errOut);
    user_private = SOSUserKeygen(user_password, (__bridge CFDataRef)(account.accountKeyDerivationParamters), error);
    require_quiet(user_private, errOut);

    require_action_quiet(SOSAccountValidateAccountCredential(account, user_private, error), errOut, CFReleaseNull(user_private));
errOut:
    return user_private;
}

static bool sosAccountValidatePasswordOrFail(SOSAccount* account, CFDataRef user_password, CFErrorRef *error) {
    SecKeyRef privKey = sosAccountCreateKeyIfPasswordIsCorrect(account, user_password, error);
    if(!privKey) {
        if(account.accountKeyDerivationParamters) debugDumpUserParameters(CFSTR("sosAccountValidatePasswordOrFail"), (__bridge CFDataRef)(account.accountKeyDerivationParamters));
        SOSCreateError(kSOSErrorWrongPassword, CFSTR("Could not create correct key with password."), NULL, error);
        return false;
    }
    sosAccountSetTrustedCredentials(account, user_password, privKey, account.accountKeyIsTrusted);
    CFReleaseNull(privKey);
    return true;
}

void SOSAccountSetParameters(SOSAccount* account, CFDataRef parameters) {
    account.accountKeyDerivationParamters = (__bridge NSData *) parameters;
}

bool SOSAccountValidateAccountCredential(SOSAccount* account, SecKeyRef accountPrivateKey, CFErrorRef *error)
{
    SecKeyRef publicCandidate = NULL;
    bool res = false;

    publicCandidate = SecKeyCreatePublicFromPrivate(accountPrivateKey);

    require_action_quiet(CFEqualSafe(account.accountKey, publicCandidate), fail,
                         SOSCreateErrorWithFormat(kSOSErrorWrongPassword, NULL, error, NULL, CFSTR("account private key no doesn't match validated public key: candidate: %@ accountKey: %@"), publicCandidate, account.accountKey));

    res = true;
fail:
    CFReleaseNull(publicCandidate);
    return res;

}

bool SOSAccountAssertStashedAccountCredential(SOSAccount* account, CFErrorRef *error)
{
    SecKeyRef accountPrivateKey = NULL, publicCandidate = NULL;
    bool result = false;

    require_action(account.accountKey, fail, SOSCreateError(kSOSErrorWrongPassword, CFSTR("account public key missing, can't check stashed copy"), NULL, error));
    require_action(account.accountKeyIsTrusted, fail, SOSCreateError(kSOSErrorWrongPassword, CFSTR("public key no not valid, can't check stashed copy"), NULL, error));


    accountPrivateKey = SOSAccountCopyStashedUserPrivateKey(account, error);
    require_action_quiet(accountPrivateKey, fail, secnotice("circleOps", "Looked for a stashed private key, didn't find one"));

    require(SOSAccountValidateAccountCredential(account, accountPrivateKey, error), fail);

    sosAccountSetTrustedCredentials(account, NULL, accountPrivateKey, true);

    result = true;
fail:
    CFReleaseSafe(publicCandidate);
    CFReleaseSafe(accountPrivateKey);

    return result;
}



bool SOSAccountAssertUserCredentials(SOSAccount* account, CFStringRef user_account __unused, CFDataRef user_password, CFErrorRef *error)
{
    bool public_was_trusted = account.accountKeyIsTrusted;
    account.accountKeyIsTrusted = false;
    SecKeyRef user_private = NULL;
    CFDataRef parameters = NULL;
    
    // if this succeeds, skip to the end.  Success will update account.accountKeyIsTrusted by side-effect.
    require_quiet(!sosAccountValidatePasswordOrFail(account, user_password, error), recordCred);
    
    // We may or may not have parameters here.
    // In any case we tried using them and they didn't match
    // So forget all that and start again, assume we're the first to push anything useful.

    if (CFDataGetLength(user_password) > 20) {
        secwarning("Long password (>20 byte utf8) being used to derive account key – this may be a PET by mistake!!");
    }
    
    parameters = SOSUserKeyCreateGenerateParameters(error);
    require_quiet(user_private = SOSUserKeygen(user_password, parameters, error), errOut);
    SOSAccountSetParameters(account, parameters);
    sosAccountSetTrustedCredentials(account, user_password, user_private, public_was_trusted);

    CFErrorRef publishError = NULL;
    if (!SOSAccountPublishCloudParameters(account, &publishError)) {
        secerror("Failed to publish new cloud parameters: %@", publishError);
    }
    
    CFReleaseNull(publishError);
recordCred:
    SOSAccountStashAccountKey(account);
errOut:
    CFReleaseNull(parameters);
    CFReleaseNull(user_private);
    secnotice("circleop", "Setting account.key_interests_need_updating to true in SOSAccountAssertUserCredentials");
    account.key_interests_need_updating = true;
    return account.accountKeyIsTrusted;
}


bool SOSAccountTryUserCredentials(SOSAccount* account, CFStringRef user_account __unused, CFDataRef user_password, CFErrorRef *error) {
    bool success = sosAccountValidatePasswordOrFail(account, user_password, error);
    if(success) {
        SOSAccountStashAccountKey(account);
    }
    secnotice("circleop", "Setting account.key_interests_need_updating to true in SOSAccountTryUserCredentials");
    account.key_interests_need_updating = true;
    return success;
}

bool SOSAccountTryUserPrivateKey(SOSAccount* account, SecKeyRef user_private, CFErrorRef *error) {
    bool retval = SOSAccountValidateAccountCredential(account, user_private, error);
    if(!retval) {
        secnotice("circleOps", "Failed to accept provided user_private as credential");
        return retval;
    }
    sosAccountSetTrustedCredentials(account, NULL, user_private, account.accountKeyIsTrusted);
    SOSAccountStashAccountKey(account);
    secnotice("circleop", "Setting account.key_interests_need_updating to true in SOSAccountTryUserPrivateKey");
    account.key_interests_need_updating = true;
    secnotice("circleOps", "Accepted provided user_private as credential");
    return retval;
}

bool SOSAccountRetryUserCredentials(SOSAccount* account) {
    CFDataRef cachedPassword = SOSAccountGetCachedPassword(account, NULL);
    if (cachedPassword == NULL)
        return false;
    /*
     * SOSAccountTryUserCredentials reset the cached password internally,
     * so we must have a retain of the password over SOSAccountTryUserCredentials().
     */
    CFRetain(cachedPassword);
    bool res = SOSAccountTryUserCredentials(account, NULL, cachedPassword, NULL);
    CFRelease(cachedPassword);
    return res;
}



