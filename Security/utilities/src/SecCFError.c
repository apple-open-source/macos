//
//  SecCFError.c
//  utilities
//
//  Created by Mitch Adler on 7/18/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include <Security/SecBase.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/debugging.h>

bool SecKernError(kern_return_t result, CFErrorRef *error, CFStringRef format, ...) {
    if (!result) return true;
    if (error) {
        va_list args;
        CFIndex code = result;
        CFErrorRef previousError = *error;

        *error = NULL;
        va_start(args, format);
        SecCFCreateErrorWithFormatAndArguments(code, kSecKernDomain, previousError, error, NULL, format, args);
        va_end(args);
    }
    return false;
}

bool SecCheckErrno(int result, CFErrorRef *error, CFStringRef format, ...) {
    if (result == 0) return true;
    if (error) {
        va_list args;
        int errnum = errno;
        CFIndex code = errnum;
        CFErrorRef previousError = *error;

        *error = NULL;
        va_start(args, format);
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
        va_end(args);
        SecCFCreateErrorWithFormat(code, kSecErrnoDomain, previousError, error, NULL, CFSTR("%@: [%d] %s"), message, errnum, strerror(errnum));
        CFReleaseSafe(message);
    }
    return false;
}

bool SecError(OSStatus status, CFErrorRef *error, CFStringRef format, ...) {
    if (status == errSecSuccess) return true;
    if (error) {
        va_list args;
        CFIndex code = status;
        CFErrorRef previousError = *error;

        *error = NULL;
        va_start(args, format);
        SecCFCreateErrorWithFormatAndArguments(code, kSecErrorDomain, previousError, error, NULL, format, args);
        va_end(args);
    }
    return false;
}


void SecCFCreateError(CFIndex errorCode, CFStringRef domain, CFStringRef descriptionString,
                      CFErrorRef previousError, CFErrorRef *newError)
{
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    SecCFCreateErrorWithFormat(errorCode, domain, previousError, newError, NULL, descriptionString);
#pragma clang diagnostic pop
}

void SecCFCreateErrorWithFormat(CFIndex errorCode, CFStringRef domain, CFErrorRef previousError, CFErrorRef *newError,
                                CFDictionaryRef formatoptions, CFStringRef format, ...)
{
    va_list args;
    va_start(args, format);

    SecCFCreateErrorWithFormatAndArguments(errorCode, domain, previousError, newError, formatoptions, format, args);

    va_end(args);
}

void SecCFCreateErrorWithFormatAndArguments(CFIndex errorCode, CFStringRef domain,
                                            CFErrorRef previousError, CFErrorRef *newError,
                                            CFDictionaryRef formatoptions, CFStringRef format, va_list args)
{
    if (newError && !(*newError)) {
        CFStringRef formattedString = CFStringCreateWithFormatAndArguments(NULL, formatoptions, format, args);
        
        const void* keys[2] =   { kCFErrorDescriptionKey,   kCFErrorUnderlyingErrorKey};
        const void* values[2] = { formattedString,          previousError };
        const CFIndex numEntriesToUse = (previousError != NULL) ? 2 : 1;

        *newError = CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault, domain, errorCode,
                                                           keys, values, numEntriesToUse);

        CFReleaseNull(formattedString);
        if (previousError)
            secnotice("error", "encapsulated %@ with new error: %@", previousError, *newError);
    } else {
        if (previousError && newError && (previousError != *newError)) {
            secnotice("error", "dropping %@", previousError);
            CFRelease(previousError);
        }
    }
}
