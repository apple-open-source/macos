/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#if OCTAGON

#import <AuthKit/AuthKit.h>

#import "keychain/ot/OTDefines.h"
#import "keychain/ot/ErrorUtils.h"
#import "keychain/ckks/CKKS.h"

/*
 * These are stolen from cloudkit to avoid having to link it (which we cannot do on darinwos).
 */

static NSString * const _CKErrorDomain = @"CKErrorDomain";
static NSString * const _CKInternalErrorDomain = @"CKInternalErrorDomain";
static NSString * const _CKErrorRetryAfterKey = @"CKRetryAfter";

enum {
    /*! Network not available */
    _CKErrorNetworkUnavailable             = 3,

    /*! Network error (available but CFNetwork gave us an error) */
    _CKErrorNetworkFailure                 = 4,

    /*! Client is being rate limited */
    _CKErrorRequestRateLimited             = 7,

    /*! The server rejected this request. This is a non-recoverable error */
    _CKErrorServerRejectedRequest          = 15,

    /* Request server errors */
    _CKErrorInternalServerInternalError = 2000,

    /* More Other */
    _CKErrorInternalPluginError = 6000,
};

@implementation NSError (Octagon)

- (BOOL)_isCKServerInternalError {
    NSError* underlyingError = self.userInfo[NSUnderlyingErrorKey];

    return [self.domain isEqualToString:_CKErrorDomain] &&
        self.code == _CKErrorServerRejectedRequest &&
        underlyingError &&
        [underlyingError.domain isEqualToString:_CKInternalErrorDomain] &&
        underlyingError.code == _CKErrorInternalServerInternalError;
}

- (BOOL)_isCuttlefishError:(CuttlefishErrorCode)cuttlefishErrorCode
{
    NSError *error = self;

    if ([error.domain isEqualToString:_CKErrorDomain] && error.code == _CKErrorServerRejectedRequest) {
        NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];

        if([underlyingError.domain isEqualToString:_CKInternalErrorDomain] && underlyingError.code == _CKErrorInternalPluginError) {
            NSError* cuttlefishError = underlyingError.userInfo[NSUnderlyingErrorKey];

            if([cuttlefishError.domain isEqualToString:CuttlefishErrorDomain] && cuttlefishError.code == cuttlefishErrorCode) {
                return YES;
            }
        }
    }
    return NO;
}

- (BOOL)_isRetryableNSURLError {
    NSError *error = self;

    if ([error.domain isEqualToString:NSURLErrorDomain]) {
        switch (error.code) {
        case NSURLErrorTimedOut:
            return true;
        default:
            return false;
        }
    }
    return false;
}

- (BOOL)_isRetryableAKError {
    NSError* underlyingError = self.userInfo[NSUnderlyingErrorKey];

    return [self.domain isEqualToString:AKAppleIDAuthenticationErrorDomain] &&
        underlyingError &&
        [underlyingError _isRetryableNSURLError];
}

- (bool)isRetryable {
    bool retry = false;
    // Specific errors that are transaction failed -- try them again
    if ([self _isCuttlefishError:CuttlefishErrorRetryableServerFailure] ||
        [self _isCuttlefishError:CuttlefishErrorTransactionalFailure]) {
        retry = true;
    // These are the CuttlefishError -> FunctionErrorType
    } else if ([self _isCuttlefishError:CuttlefishErrorJoinFailed] ||
               [self _isCuttlefishError:CuttlefishErrorUpdateTrustFailed] ||
               [self _isCuttlefishError:CuttlefishErrorEstablishPeerFailed] ||
               [self _isCuttlefishError:CuttlefishErrorEstablishBottleFailed] ||
               [self _isCuttlefishError:CuttlefishErrorEscrowProxyFailure]) {
        retry = true;
    } else if ([self.domain isEqualToString:TrustedPeersHelperErrorDomain]) {
        switch (self.code) {
        case TrustedPeersHelperErrorUnknownCloudKitError:
            retry = true;
            break;
        default:
            break;
        }
    } else if ([self _isRetryableNSURLError]) {
        retry = true;
    } else if ([self.domain isEqualToString:_CKErrorDomain]) {
        if (self.userInfo[_CKErrorRetryAfterKey] != nil) {
            retry = true;
        } else {
            switch (self.code) {
            case _CKErrorNetworkUnavailable:
            case _CKErrorNetworkFailure:
            case _CKErrorRequestRateLimited:
                retry = true;
                break;
            default:
                break;
            }
        }
    } else if ([self _isCKServerInternalError]) {
        retry = true;
    } else if ([self _isRetryableAKError]) {
        retry = true;
    }

    return retry;
}

@end

#endif // OCTAGON
