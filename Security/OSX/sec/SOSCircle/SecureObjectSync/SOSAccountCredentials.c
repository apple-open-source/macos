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
#include "SOSAccountPriv.h"
#include "SOSPeerInfoCollections.h"
#include "SOSTransport.h"

#define kPublicKeyNotAvailable "com.apple.security.publickeynotavailable"

//
// MARK: User Credential management
//

void SOSAccountSetPreviousPublic(SOSAccountRef account) {
    CFReleaseNull(account->previous_public);
    account->previous_public = account->user_public;
    CFRetain(account->previous_public);
}

static void SOSAccountRemoveInvalidApplications(SOSAccountRef account, SOSCircleRef circle)
{
    CFMutableSetRef peersToRemove = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    SOSCircleForEachApplicant(circle, ^(SOSPeerInfoRef peer) {
        if (!SOSPeerInfoApplicationVerify(peer, account->user_public, NULL))
            CFSetAddValue(peersToRemove, peer);
    });

    CFSetForEach(peersToRemove, ^(const void *value) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef) value;

        SOSCircleWithdrawRequest(circle, peer, NULL);
    });
}

// List of things to do
// Update myFullPeerInfo in circle if needed
// Fix iCloud Identity if needed
// Gen sign if private key changed

static bool sosAccountUpgradeiCloudIdentity(SOSCircleRef circle, SecKeyRef privKey) {
    bool retval = false;
    SOSFullPeerInfoRef cloud_fpi = SOSCircleCopyiCloudFullPeerInfoRef(circle, NULL);
    require_quiet(cloud_fpi != NULL, errOut);
    require_quiet(SOSFullPeerInfoUpgradeSignatures(cloud_fpi, privKey, NULL), errOut);
    retval = SOSCircleUpdatePeerInfo(circle, SOSFullPeerInfoGetPeerInfo(cloud_fpi));
errOut:
    return retval;
}

static void SOSAccountGenerationSignatureUpdateWith(SOSAccountRef account, SecKeyRef privKey) {
    if (account->trusted_circle && account->my_identity) {
        SOSAccountModifyCircle(account, NULL, ^(SOSCircleRef circle) {
            SOSPeerInfoRef myPI = SOSAccountGetMyPeerInfo(account);
            bool iAmPeer = SOSCircleHasPeer(account->trusted_circle, myPI, NULL);
            bool change = SOSCircleUpdatePeerInfo(circle, myPI);
            if(iAmPeer && !SOSCircleVerify(circle, account->user_public, NULL)) {
                change |= sosAccountUpgradeiCloudIdentity(circle, privKey);
                SOSAccountRemoveInvalidApplications(account, circle);
                change |= SOSCircleGenerationSign(circle, privKey, account->my_identity, NULL);
                account->departure_code = kSOSNeverLeftCircle;
            } else if(iAmPeer) {
                SOSFullPeerInfoRef icfpi = SOSCircleCopyiCloudFullPeerInfoRef(circle, NULL);
                if(!icfpi) {
                    SOSAccountRemoveIncompleteiCloudIdentities(account, circle, privKey, NULL);
                    change |= SOSAccountAddiCloudIdentity(account, circle, privKey, NULL);
                } else {
                    CFReleaseNull(icfpi);
                }
            }
            secnotice("updatingGenSignature", "we changed the circle? %@", change ? CFSTR("YES") : CFSTR("NO"));
            return change;
        });
    }
}

bool SOSAccountGenerationSignatureUpdate(SOSAccountRef account, CFErrorRef *error) {
    bool result = false;
    SecKeyRef priv_key = SOSAccountGetPrivateCredential(account, error);
    require_quiet(priv_key, bail);

    SOSAccountGenerationSignatureUpdateWith(account, priv_key);
    
    result = true;
bail:
    return result;
}

/* this one is meant to be local - not published over KVS. */
static bool SOSAccountPeerSignatureUpdate(SOSAccountRef account, SecKeyRef privKey, CFErrorRef *error) {
    return account->my_identity && SOSFullPeerInfoUpgradeSignatures(account->my_identity, privKey, error);
}


void SOSAccountPurgePrivateCredential(SOSAccountRef account)
{
    CFReleaseNull(account->_user_private);
    CFReleaseNull(account->_password_tmp);
    if (account->user_private_timer) {
        dispatch_source_cancel(account->user_private_timer);
        dispatch_release(account->user_private_timer);
        account->user_private_timer = NULL;
        xpc_transaction_end();
    }
    if (account->lock_notification_token != NOTIFY_TOKEN_INVALID) {
        notify_cancel(account->lock_notification_token);
        account->lock_notification_token = NOTIFY_TOKEN_INVALID;
    }
}


static void SOSAccountSetTrustedUserPublicKey(SOSAccountRef account, bool public_was_trusted, SecKeyRef privKey)
{
    if (!privKey) return;
    SecKeyRef publicKey = SecKeyCreatePublicFromPrivate(privKey);
    
    if (account->user_public && account->user_public_trusted && CFEqual(publicKey, account->user_public)) {
        CFReleaseNull(publicKey);
        return;
    }
    
    if(public_was_trusted && account->user_public) {
        CFReleaseNull(account->previous_public);
        account->previous_public = account->user_public;
        CFRetain(account->previous_public);
    }
    
    CFReleaseNull(account->user_public);
    account->user_public = publicKey;
    account->user_public_trusted = true;
    
    if(!account->previous_public) {
        account->previous_public = account->user_public;
        CFRetain(account->previous_public);
    }
    
	secnotice("keygen", "trusting new public key: %@", account->user_public);
}

void SOSAccountSetUnTrustedUserPublicKey(SOSAccountRef account, SecKeyRef publicKey) {
    if(account->user_public_trusted && account->user_public) {
        secnotice("keygen", "Moving : %@ to previous_public", account->user_public);
        CFRetainAssign(account->previous_public, account->user_public);
    }
    
    CFReleaseNull(account->user_public);
    account->user_public = publicKey;
    account->user_public_trusted = false;
    
    if(!account->previous_public) {
        CFRetainAssign(account->previous_public, account->user_public);
    }
    
    secnotice("keygen", "not trusting new public key: %@", account->user_public);
    notify_post(kPublicKeyNotAvailable);
}


static void SOSAccountSetPrivateCredential(SOSAccountRef account, SecKeyRef private, CFDataRef password) {
    if (!private)
        return SOSAccountPurgePrivateCredential(account);
    
    CFRetain(private);
    CFReleaseSafe(account->_user_private);
    account->_user_private = private;
    CFReleaseSafe(account->_password_tmp);
    account->_password_tmp = CFDataCreateCopy(kCFAllocatorDefault, password);
    
    bool resume_timer = false;
    if (!account->user_private_timer) {
        xpc_transaction_begin();
        resume_timer = true;
        account->user_private_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, account->queue);
        dispatch_source_set_event_handler(account->user_private_timer, ^{
            SOSAccountPurgePrivateCredential(account);
        });
        
        notify_register_dispatch(kUserKeybagStateChangeNotification, &account->lock_notification_token, account->queue, ^(int token) {
            bool locked = false;
            CFErrorRef lockCheckError = NULL;
            
            if (!SecAKSGetIsLocked(&locked, &lockCheckError)) {
                secerror("Checking for locked after change failed: %@", lockCheckError);
            }
            
            if (locked) {
                SOSAccountPurgePrivateCredential(account);
            }
        });
    }
    
    // (Re)set the timer's fire time to now + 120 seconds with a 5 second fuzz factor.
    dispatch_time_t purgeTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(10 * 60 * NSEC_PER_SEC));
    dispatch_source_set_timer(account->user_private_timer, purgeTime, DISPATCH_TIME_FOREVER, (int64_t)(5 * NSEC_PER_SEC));
    if (resume_timer)
        dispatch_resume(account->user_private_timer);
}

SecKeyRef SOSAccountGetPrivateCredential(SOSAccountRef account, CFErrorRef* error)
{
    if (account->_user_private == NULL) {
        SOSCreateError(kSOSErrorPrivateKeyAbsent, CFSTR("Private Key not available - failed to prompt user recently"), NULL, error);
    }
    return account->_user_private;
}

CFDataRef SOSAccountGetCachedPassword(SOSAccountRef account, CFErrorRef* error)
{
    if (account->_password_tmp == NULL) {
        secnotice("keygen", "Password cache expired");
    }
    return account->_password_tmp;
}

SecKeyRef SOSAccountGetTrustedPublicCredential(SOSAccountRef account, CFErrorRef* error)
{
    if (account->user_public == NULL || account->user_public_trusted == false) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Public Key not available - failed to register before call"), NULL, error);
        return NULL;
    }
    return account->user_public;
}



bool SOSAccountHasPublicKey(SOSAccountRef account, CFErrorRef* error)
{
    return SOSAccountGetTrustedPublicCredential(account, error);
}

static void sosAccountSetTrustedCredentials(SOSAccountRef account, CFDataRef user_password, SecKeyRef user_private, bool public_was_trusted) {
    if(!SOSAccountFullPeerInfoVerify(account, user_private, NULL))  {
        (void) SOSAccountPeerSignatureUpdate(account, user_private, NULL);
    }
    SOSAccountSetTrustedUserPublicKey(account, public_was_trusted, user_private);
    SOSAccountSetPrivateCredential(account, user_private, user_password);
    SOSAccountCheckForAlwaysOnViews(account);
}

static SecKeyRef sosAccountCreateKeyIfPasswordIsCorrect(SOSAccountRef account, CFDataRef user_password, CFErrorRef *error) {
    SecKeyRef user_private = NULL;
    require_quiet(account->user_public && account->user_key_parameters, errOut);
    user_private = SOSUserKeygen(user_password, account->user_key_parameters, error);
    require_quiet(user_private, errOut);
    SecKeyRef public_candidate = SecKeyCreatePublicFromPrivate(user_private);
    if(!CFEqualSafe(account->user_public, public_candidate)) {
        secnotice("keygen", "Public keys don't match:  expected: %@, calculated: %@", account->user_public, public_candidate);
        CFReleaseNull(user_private);
    }
    CFReleaseSafe(public_candidate);
errOut:
    return user_private;
}

static bool sosAccountValidatePasswordOrFail(SOSAccountRef account, CFDataRef user_password, CFErrorRef *error) {
    SecKeyRef privKey = sosAccountCreateKeyIfPasswordIsCorrect(account, user_password, error);
    if(!privKey) {
        if(account->user_key_parameters) debugDumpUserParameters(CFSTR("sosAccountValidatePasswordOrFail"), account->user_key_parameters);
        SOSCreateError(kSOSErrorWrongPassword, CFSTR("Could not create correct key with password."), NULL, error);
        return false;
    }
    sosAccountSetTrustedCredentials(account, user_password, privKey, account->user_public_trusted);
    CFReleaseNull(privKey);
    return true;
}

void SOSAccountSetParameters(SOSAccountRef account, CFDataRef parameters) {
    CFRetainAssign(account->user_key_parameters, parameters);
}

bool SOSAccountAssertUserCredentials(SOSAccountRef account, CFStringRef user_account __unused, CFDataRef user_password, CFErrorRef *error)
{
    bool public_was_trusted = account->user_public_trusted;
    account->user_public_trusted = false;
    SecKeyRef user_private = NULL;
    CFDataRef parameters = NULL;
    
    // if this succeeds, skip to the end.  Success will update account->user_public_trusted by side-effect.
    require_quiet(!sosAccountValidatePasswordOrFail(account, user_password, error), errOut);
    
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
    
    CFReleaseSafe(publishError);
    
errOut:
    CFReleaseSafe(parameters);
    CFReleaseSafe(user_private);
    account->key_interests_need_updating = true;
    return account->user_public_trusted;
}


bool SOSAccountTryUserCredentials(SOSAccountRef account, CFStringRef user_account __unused, CFDataRef user_password, CFErrorRef *error) {
    bool success = sosAccountValidatePasswordOrFail(account, user_password, error);
    account->key_interests_need_updating = true;
    return success;
}

bool SOSAccountRetryUserCredentials(SOSAccountRef account) {
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



