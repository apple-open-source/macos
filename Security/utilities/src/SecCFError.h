//
//  SecCFError.h
//  utilities
//
//  Created by Mitch Adler on 7/18/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#ifndef _SECCFERROR_H_
#define _SECCFERROR_H_

#include <CoreFoundation/CoreFoundation.h>

//
// Leaf error creation from other systems
//

// kern_return_t errors
#define kSecKernDomain  kCFErrorDomainMach
bool SecKernError(kern_return_t result, CFErrorRef *error, CFStringRef format, ...);

// Unix errno errors
#define kSecErrnoDomain  kCFErrorDomainPOSIX
bool SecCheckErrno(int result, CFErrorRef *error, CFStringRef format, ...);

// OSStatus errors
#define kSecErrorDomain  kCFErrorDomainOSStatus
bool SecError(OSStatus status, CFErrorRef *error, CFStringRef format, ...);

//
// Create and chain
//
void SecCFCreateError(CFIndex errorCode, CFStringRef domain, CFStringRef descriptionString,
                      CFErrorRef previousError, CFErrorRef *newError);

void SecCFCreateErrorWithFormat(CFIndex errorCode, CFStringRef domain, CFErrorRef previousError, CFErrorRef *newError,
                                CFDictionaryRef formatoptions, CFStringRef descriptionString, ...)
                        CF_FORMAT_FUNCTION(6,7);


void SecCFCreateErrorWithFormatAndArguments(CFIndex errorCode, CFStringRef domain,
                                            CFErrorRef previousError, CFErrorRef *newError,
                                            CFDictionaryRef formatoptions, CFStringRef descriptionString, va_list args)
                                CF_FORMAT_FUNCTION(6, 0);

#endif
