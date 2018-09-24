/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include "utilities/SecCFError.h"
#include "utilities/SecCFRelease.h"
#include "utilities/SecCFWrappers.h"
#include "utilities/iOSforOSX.h"

#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <securityd/SecDbItem.h> // For SecError
#include "utilities/iOSforOSX.h"

#include <Security/SecBase64.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>
#include <utilities/der_date.h>

#include <corecrypto/ccrng.h>
#include <corecrypto/ccrng_pbkdf2_prng.h>

#include <CommonCrypto/CommonRandomSPI.h>

#include <os/lock.h>

#include <AssertMacros.h>

const CFStringRef kSOSErrorDomain = CFSTR("com.apple.security.sos.error");
const CFStringRef kSOSDSIDKey = CFSTR("AccountDSID");
const CFStringRef SOSTransportMessageTypeIDSV2 = CFSTR("IDS2.0");
const CFStringRef SOSTransportMessageTypeKVS = CFSTR("KVS");

bool SOSErrorCreate(CFIndex errorCode, CFErrorRef *error, CFDictionaryRef formatOptions, CFStringRef format, ...)
    CF_FORMAT_FUNCTION(4, 5);


bool SOSErrorCreate(CFIndex errorCode, CFErrorRef *error, CFDictionaryRef formatOptions, CFStringRef format, ...) {
    if (!errorCode) return true;
    if (error && !*error) {
        va_list va;
        va_start(va, format);
        SecCFCreateErrorWithFormatAndArguments(errorCode, kSOSErrorDomain, NULL, error, formatOptions, format, va);
        va_end(va);
    }
    return false;
}

bool SOSCreateError(CFIndex errorCode, CFStringRef descriptionString, CFErrorRef previousError, CFErrorRef *newError) {
    SOSCreateErrorWithFormat(errorCode, previousError, newError, NULL, CFSTR("%@"), descriptionString);
    return false;
}

bool SOSCreateErrorWithFormat(CFIndex errorCode, CFErrorRef previousError, CFErrorRef *newError,
                              CFDictionaryRef formatOptions, CFStringRef format, ...) {
    va_list va;
    va_start(va, format);
    bool res = SOSCreateErrorWithFormatAndArguments(errorCode, previousError, newError, formatOptions, format, va);
    va_end(va);
    return res;
}

bool SOSCreateErrorWithFormatAndArguments(CFIndex errorCode, CFErrorRef previousError, CFErrorRef *newError,
                                          CFDictionaryRef formatOptions, CFStringRef format, va_list args) {
    return SecCFCreateErrorWithFormatAndArguments(errorCode, kSOSErrorDomain, previousError, newError, formatOptions, format, args);
}


//
// Utility Functions
//

static OSStatus GenerateECPairImp(int keySize, CFBooleanRef permanent, SecKeyRef* public, SecKeyRef *full)
{
    static const CFStringRef sTempNameToUse = CFSTR("GenerateECPair Temporary Key - Shouldn't be live");

    CFNumberRef signing_bitsize = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keySize);

    CFDictionaryRef keygen_parameters = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                     kSecAttrKeyType,       kSecAttrKeyTypeEC,
                                                                     kSecAttrKeySizeInBits, signing_bitsize,
                                                                     kSecAttrIsPermanent,   permanent,
                                                                     kSecAttrLabel,         sTempNameToUse,
                                                                     NULL);
    CFReleaseNull(signing_bitsize);
    OSStatus result = SecKeyGeneratePair(keygen_parameters, public, full);
    CFReleaseNull(keygen_parameters);

    return result;
}

OSStatus GenerateECPair(int keySize, SecKeyRef* public, SecKeyRef *full)
{
    return GenerateECPairImp(keySize, kCFBooleanFalse, public, full);
}

OSStatus GeneratePermanentECPair(int keySize, SecKeyRef* public, SecKeyRef *full)
{
    return GenerateECPairImp(keySize, kCFBooleanTrue, public, full);
}

static CFStringRef SOSCircleCopyDescriptionFromData(CFDataRef data)
{
    CFErrorRef error = NULL;
    CFStringRef result = NULL;

    SOSCircleRef circle = SOSCircleCreateFromData(kCFAllocatorDefault, data, &error);

    if (circle)
        result = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), circle);

    CFReleaseSafe(circle);

    return result;
}

CFStringRef SOSItemsChangedCopyDescription(CFDictionaryRef changes, bool is_sender)
{
    CFMutableStringRef string = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("<Changes: {\n"));

    CFDictionaryForEach(changes, ^(const void *key, const void *value) {
        CFStringRef value_description = NULL;
        if (isString(key) && isData(value)) {
            CFDataRef value_data = (CFDataRef) value;
            switch (SOSKVSKeyGetKeyType(key)) {
                case kCircleKey:
                    value_description = SOSCircleCopyDescriptionFromData(value_data);
                    break;
                case kMessageKey:
                    value_description = CFCopyDescription(value_data);
                    break;
                default:
                    break;
            }

        }
        CFStringAppendFormat(string, NULL, CFSTR("    '%@' %s %@\n"),
                             key,
                             is_sender ? "<=" : "=>",
                             value_description ? value_description : value);
        
        CFReleaseNull(value_description);
    });

    CFStringAppendFormat(string, NULL, CFSTR("}"));

    return string;
}

CFStringRef SOSCopyHashBufAsString(uint8_t *digest, size_t len) {
    char encoded[2 * len + 1]; // Big enough for base64 encoding.

    size_t length = SecBase64Encode(digest, len, encoded, sizeof(encoded));
    assert(length && length < sizeof(encoded));
    if (length > kSOSPeerIDLengthMax)
        length = kSOSPeerIDLengthMax;
    encoded[length] = 0;
    return CFStringCreateWithCString(kCFAllocatorDefault, encoded, kCFStringEncodingASCII);
}

CFStringRef SOSCopyIDOfDataBuffer(CFDataRef data, CFErrorRef *error) {
    const struct ccdigest_info * di = ccsha1_di();
    uint8_t digest[di->output_size];    
    ccdigest(di, CFDataGetLength(data), CFDataGetBytePtr(data), digest);
    return SOSCopyHashBufAsString(digest, sizeof(digest));
}

CFStringRef SOSCopyIDOfDataBufferWithLength(CFDataRef data, CFIndex len, CFErrorRef *error) {
    CFStringRef retval = NULL;
    CFStringRef tmp = SOSCopyIDOfDataBuffer(data, error);
    if(tmp) retval = CFStringCreateWithSubstring(kCFAllocatorDefault, tmp, CFRangeMake(0, len));
    CFReleaseNull(tmp);
    return retval;
}

CFStringRef SOSCopyIDOfKey(SecKeyRef key, CFErrorRef *error) {
    CFDataRef publicBytes = NULL;
    CFStringRef result = NULL;
    require_action_quiet(key, errOut, SOSErrorCreate(kSOSErrorNoKey, error, NULL, CFSTR("NULL key passed to SOSCopyIDOfKey")));
    require_quiet(SecError(SecKeyCopyPublicBytes(key, &publicBytes), error, CFSTR("Failed to export public bytes %@"), key), errOut);
    result = SOSCopyIDOfDataBuffer(publicBytes, error);
errOut:
    CFReleaseNull(publicBytes);
    return result;
}

CFStringRef SOSCopyIDOfKeyWithLength(SecKeyRef key, CFIndex len, CFErrorRef *error) {
    CFStringRef retval = NULL;
    CFStringRef tmp = SOSCopyIDOfKey(key, error);
    if(tmp) retval = CFStringCreateWithSubstring(kCFAllocatorDefault, tmp, CFRangeMake(0, len));
    CFReleaseNull(tmp);
    return retval;
}


CFGiblisGetSingleton(ccec_const_cp_t, SOSGetBackupKeyCurveParameters, sBackupKeyCurveParameters, ^{
    *sBackupKeyCurveParameters = ccec_cp_256();
});


//
// We're expecting full entropy here, so we just need to stretch
// via the PBKDF entropy rng. We'll choose a few iterations and no salt
// since we don't get sent any.
//
const int kBackupKeyIterations = 20;
const uint8_t sBackupKeySalt[] = { 0 };

bool SOSPerformWithDeviceBackupFullKey(ccec_const_cp_t cp, CFDataRef entropy, CFErrorRef *error, void (^operation)(ccec_full_ctx_t fullKey))
{
    bool result = false;
    ccec_full_ctx_decl_cp(cp, fullKey);

    require_quiet(SOSGenerateDeviceBackupFullKey(fullKey, cp, entropy, error), exit);

    operation(fullKey);

    result = true;
exit:
    ccec_full_ctx_clear_cp(cp, fullKey);

    return result;
}


bool SOSGenerateDeviceBackupFullKey(ccec_full_ctx_t generatedKey, ccec_const_cp_t cp, CFDataRef entropy, CFErrorRef* error)
{
    bool result = false;
    int cc_result = 0;
    struct ccrng_pbkdf2_prng_state pbkdf2_prng;
    const int kBackupKeyMaxBytes = 1024; // This may be a function of the cp but will be updated when we use a formally deterministic key generation.

    cc_result = ccrng_pbkdf2_prng_init(&pbkdf2_prng, kBackupKeyMaxBytes,
                                       CFDataGetLength(entropy), CFDataGetBytePtr(entropy),
                                       sizeof(sBackupKeySalt), sBackupKeySalt,
                                       kBackupKeyIterations);
    require_action_quiet(cc_result == 0, exit, SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("pbkdf rng init failed: %d"), cc_result));

    cc_result = ccec_compact_generate_key(cp, (struct ccrng_state *) &pbkdf2_prng, generatedKey);
    require_action_quiet(cc_result == 0, exit, SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("Generate key failed: %d"), cc_result));

    result = true;
exit:
    bzero(&pbkdf2_prng, sizeof(pbkdf2_prng));
    return result;

}

CFDataRef SOSCopyDeviceBackupPublicKey(CFDataRef entropy, CFErrorRef *error)
{
    CFDataRef result = NULL;
    CFMutableDataRef publicKeyData = NULL;

    ccec_full_ctx_decl_cp(SOSGetBackupKeyCurveParameters(), fullKey);

    require_quiet(SOSGenerateDeviceBackupFullKey(fullKey, SOSGetBackupKeyCurveParameters(), entropy, error), exit);

    size_t space = ccec_compact_export_size(false, ccec_ctx_pub(fullKey));
    publicKeyData = CFDataCreateMutableWithScratch(kCFAllocatorDefault, space);
    require_quiet(SecAllocationError(publicKeyData, error, CFSTR("Mutable data allocation")), exit);

    ccec_compact_export(false, CFDataGetMutableBytePtr(publicKeyData), fullKey);

    CFTransferRetained(result, publicKeyData);

exit:
    CFReleaseNull(publicKeyData);
    return result;
}


CFDataRef SOSDateCreate(void) {
    CFDateRef now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    size_t bufsiz = der_sizeof_date(now, NULL);
    uint8_t buf[bufsiz];
    der_encode_date(now, NULL, buf, buf+bufsiz);
    CFReleaseNull(now);
    return CFDataCreate(NULL, buf, bufsiz);
}


CFDataRef CFDataCreateWithDER(CFAllocatorRef allocator, CFIndex size, uint8_t*(^operation)(size_t size, uint8_t *buffer)) {
    __block CFMutableDataRef result = NULL;
    if(!size) return NULL;
    if((result = CFDataCreateMutableWithScratch(allocator, size)) == NULL) return NULL;
    uint8_t *ptr = CFDataGetMutableBytePtr(result);
    uint8_t *derptr = operation(size, ptr);
    if(derptr == ptr) return result; // most probable case
    if(!derptr || derptr < ptr) { // DER op failed  - or derptr ended up prior to allocated buffer
        CFReleaseNull(result);
    } else if(derptr > ptr) { // This is a possible case where we don't end up using the entire allocated buffer
        size_t diff = derptr - ptr; // The unused space ends up being the beginning of the allocation
        CFDataDeleteBytes(result, CFRangeMake(0, diff));
    }
    return result;
}

@implementation SOSCachedNotification
+ (NSString *)notificationName:(const char *)notificationString {
#if TARGET_OS_OSX
    return [NSString stringWithFormat:@"user.uid.%d.%s", getuid(), notificationString];
#else
    return @(notificationString);
#endif
}

@end

bool SOSCachedNotificationOperation(const char *notificationString, bool (^operation) (int token, bool gtg)) {
    static os_unfair_lock token_lock = OS_UNFAIR_LOCK_INIT;
    static NSMutableDictionary *tokenCache = NULL;
    int token = NOTIFY_TOKEN_INVALID;

    @autoreleasepool {
        os_unfair_lock_lock(&token_lock);
        if (tokenCache == NULL) {
            tokenCache = [NSMutableDictionary dictionary];
        }
        NSString *notification = [SOSCachedNotification notificationName:notificationString];
        if (notification == NULL) {
            os_unfair_lock_unlock(&token_lock);
            return false;
        }

        NSNumber *cachedToken = tokenCache[notification];
        if (cachedToken == NULL) {
            uint32_t status;

            status = notify_register_check([notification UTF8String], &token);
            if (status == NOTIFY_STATUS_OK) {
                tokenCache[notification] = @(token);
            } else {
                secnotice("cachedStatus", "Failed to retreive token for %@: error %d",
                          notification, status);
            }
        } else {
            token = [cachedToken intValue];
        }
        os_unfair_lock_unlock(&token_lock);
    }

    return operation(token, (token != NOTIFY_TOKEN_INVALID));
}

uint64_t SOSGetCachedCircleBitmask(void) {
    __block uint64_t retval = 0; // If the following call fails and we return 0 the caller, checking CC_STATISVALID will see we didn't get anything.
    SOSCachedNotificationOperation(kSOSCCCircleChangedNotification, ^bool(int token, bool gtg) {
        if(gtg) {
            notify_get_state(token, &retval);
        }
        return false;
    });
    return retval;
}

const SOSCCStatus kSOSNoCachedValue = -99;

SOSCCStatus SOSGetCachedCircleStatus(CFErrorRef *error) {
    uint64_t statusMask = SOSGetCachedCircleBitmask();
    SOSCCStatus retval = kSOSNoCachedValue;

    if(statusMask & CC_STATISVALID) {
        if(statusMask & CC_UKEY_TRUSTED) {
            retval = (SOSCCStatus) statusMask & CC_MASK;
        } else {
            retval = kSOSCCError;
            if(error) {
                CFReleaseNull(*error);
                if(statusMask & CC_PEER_IS_IN) {
                    SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Public Key isn't available, this peer is in the circle, but invalid. The iCloud Password must be provided to keychain syncing subsystem to repair this."), NULL, error);
                } else {
                    SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Public Key isn't available. The iCloud Password must be provided to keychain syncing subsystem to repair this."), NULL, error);
                }
            }
        }
    }
    return retval;
}

uint64_t SOSCachedViewBitmask(void) {
    __block uint64_t retval = 0;
    if(SOSGetCachedCircleStatus(NULL) == kSOSCCInCircle) {
        SOSCachedNotificationOperation(kSOSCCViewMembershipChangedNotification, ^bool(int token, bool gtg) {
            if(gtg) {
                notify_get_state(token, &retval);
                return true;
            }
            return false;
        });
    }
    return retval;
}

CFSetRef SOSCreateCachedViewStatus(void) {
    __block CFSetRef retval = NULL;
    uint64_t state = SOSCachedViewBitmask();
    if(state) {
        retval = SOSViewCreateSetFromBitmask(state);
    }
    return retval;
}

