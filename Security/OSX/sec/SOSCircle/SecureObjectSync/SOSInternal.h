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


#ifndef _SOSINTERNAL_H_
#define _SOSINTERNAL_H_

#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecKey.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>

#include <utilities/SecCFWrappers.h>

#include <corecrypto/ccec.h>

__BEGIN_DECLS

enum {
    // Public errors are first (See SOSCloudCircle)

    kSOSErrorFirstPrivateError = 1024,
    
    kSOSErrorAllocationFailure  = 1024,
    kSOSErrorEncodeFailure      = 1025,
    kSOSErrorNameMismatch       = 1026,
    kSOSErrorSendFailure        = 1027,
    kSOSErrorProcessingFailure  = 1028,
    kSOSErrorDecodeFailure      = 1029,

    kSOSErrorAlreadyPeer        = 1030,
    kSOSErrorNotApplicant       = 1031,
    kSOSErrorPeerNotFound       = 1032,

    kSOSErrorNoKey              = 1033,
    kSOSErrorBadKey             = 1034,
    kSOSErrorBadFormat          = 1035,
    kSOSErrorNoCircleName       = 1036,
    kSOSErrorNoCircle           = 1037,
    kSOSErrorBadSignature       = 1038,
    kSOSErrorReplay             = 1039,

    kSOSErrorUnexpectedType     = 1040,

    kSOSErrorUnsupported        = 1041,
    kSOSErrorInvalidMessage     = 1042,
    kSOSErrorNoRing             = 1043,

    kSOSErrorNoiCloudPeer       = 1044,
    kSOSErrorParam              = 1045,
};


// Returns false unless errorCode is 0.
bool SOSErrorCreate(CFIndex errorCode, CFErrorRef *error, CFDictionaryRef formatOptions, CFStringRef descriptionString, ...);

bool SOSCreateError(CFIndex errorCode, CFStringRef descriptionString, CFErrorRef previousError, CFErrorRef *newError);

bool SOSCreateErrorWithFormat(CFIndex errorCode, CFErrorRef previousError, CFErrorRef *newError,
                              CFDictionaryRef formatOptions, CFStringRef formatString, ...)
                        CF_FORMAT_FUNCTION(5,6);

bool SOSCreateErrorWithFormatAndArguments(CFIndex errorCode, CFErrorRef previousError, CFErrorRef *newError,
                                          CFDictionaryRef formatOptions, CFStringRef formatString, va_list args)
                                CF_FORMAT_FUNCTION(5,0);


static inline bool SOSClearErrorIfTrue(bool condition, CFErrorRef *error) {
    if(condition && error && *error) {
        secdebug("errorBug", "Got Success and Error (dropping error): %@", *error);
        CFReleaseNull(*error);
    }
    return true;
}

static inline bool isSOSErrorCoded(CFErrorRef error, CFIndex sosErrorCode) {
    return error && CFErrorGetCode(error) == sosErrorCode && CFEqualSafe(CFErrorGetDomain(error), kSOSErrorDomain);
}

//
// Backup Key handling
//
ccec_const_cp_t SOSGetBackupKeyCurveParameters(void);
bool SOSGenerateDeviceBackupFullKey(ccec_full_ctx_t generatedKey, ccec_const_cp_t cp, CFDataRef entropy, CFErrorRef* error);

bool SOSPerformWithDeviceBackupFullKey(ccec_const_cp_t cp, CFDataRef entropy, CFErrorRef *error, void (^operation)(ccec_full_ctx_t fullKey));
CFDataRef SOSCopyDeviceBackupPublicKey(CFDataRef entropy, CFErrorRef *error);

//
// Wrapping and Unwrapping
//

CFMutableDataRef SOSCopyECWrappedData(ccec_pub_ctx *ec_ctx, CFDataRef data, CFErrorRef *error);
bool             SOSPerformWithUnwrappedData(ccec_full_ctx_t ec_ctx, CFDataRef data, CFErrorRef *error,
                                             void (^operation)(size_t size, uint8_t *buffer));
CFMutableDataRef SOSCopyECUnwrappedData(ccec_full_ctx_t ec_ctx, CFDataRef data, CFErrorRef *error);
//
// Utility Functions
//
OSStatus GenerateECPair(int keySize, SecKeyRef* public, SecKeyRef *full);
OSStatus GeneratePermanentECPair(int keySize, SecKeyRef* public, SecKeyRef *full);

CFStringRef SOSItemsChangedCopyDescription(CFDictionaryRef changes, bool is_sender);

CFStringRef SOSCopyIDOfDataBuffer(CFDataRef data, CFErrorRef *error);
CFStringRef SOSCopyIDOfDataBufferWithLength(CFDataRef data, CFIndex len, CFErrorRef *error);

CFStringRef SOSCopyIDOfKey(SecKeyRef key, CFErrorRef *error);
CFStringRef SOSCopyIDOfKeyWithLength(SecKeyRef key, CFIndex len, CFErrorRef *error);

//
// Der encoding accumulation
//
static inline bool accumulate_size(size_t *accumulator, size_t size) {
    *accumulator += size;
    return size != 0;
}

// Used for simple timestamping that's DERable (not durable)
CFDataRef SOSDateCreate(void);

CFDataRef CFDataCreateWithDER(CFAllocatorRef allocator, CFIndex size, uint8_t*(^operation)(size_t size, uint8_t *buffer));

extern const CFStringRef kSecIDSErrorDomain;
extern const CFStringRef kIDSOperationType;
extern const CFStringRef kIDSMessageToSendKey;
extern const CFStringRef kIDSMessageUniqueID;
extern const CFStringRef kIDSMessageRecipientPeerID;
extern const CFStringRef kIDSMessageRecipientDeviceID;
extern const CFStringRef kIDSMessageUsesAckModel;



__END_DECLS

#endif
