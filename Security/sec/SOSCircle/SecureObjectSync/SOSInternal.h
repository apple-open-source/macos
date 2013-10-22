//
//  SOSInternal.h
//  sec
//
//  Created by Mitch Adler on 7/18/12.
//
//

#ifndef _SOSINTERNAL_H_
#define _SOSINTERNAL_H_

#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecKey.h>

#include <SecureObjectSync/SOSCloudCircle.h>

#include <utilities/SecCFWrappers.h>

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

    kSOSErrorUnsupported        = 1041
};

bool SOSCreateError(CFIndex errorCode, CFStringRef descriptionString, CFErrorRef previousError, CFErrorRef *newError);

bool SOSCreateErrorWithFormat(CFIndex errorCode, CFErrorRef previousError, CFErrorRef *newError,
                              CFDictionaryRef formatOptions, CFStringRef formatString, ...)
                        CF_FORMAT_FUNCTION(5,6);

bool SOSCreateErrorWithFormatAndArguments(CFIndex errorCode, CFErrorRef previousError, CFErrorRef *newError,
                                          CFDictionaryRef formatOptions, CFStringRef formatString, va_list args)
                                CF_FORMAT_FUNCTION(5,0);


static inline bool isSOSErrorCoded(CFErrorRef error, CFIndex sosErrorCode) {
    return CFErrorGetCode(error) == sosErrorCode && CFEqualSafe(CFErrorGetDomain(error), kSOSErrorDomain);
}


//
// Utility Functions
//
OSStatus GenerateECPair(int keySize, SecKeyRef* public, SecKeyRef *full);
OSStatus GeneratePermanentECPair(int keySize, SecKeyRef* public, SecKeyRef *full);

CFStringRef SOSChangesCopyDescription(CFDictionaryRef changes, bool is_sender);

CFStringRef SOSCopyIDOfKey(SecKeyRef key, CFErrorRef *error);

//
// Der encoding accumulation
//
static inline bool accumulate_size(size_t *accumulator, size_t size) {
    *accumulator += size;
    return size != 0;
}

__END_DECLS

#endif
