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
// Create and chain, all return false to make the analyzer happy.
//
bool SecCFCreateError(CFIndex errorCode, CFStringRef domain, CFStringRef descriptionString,
                      CFErrorRef previousError, CFErrorRef *newError);

bool SecCFCreateErrorWithFormat(CFIndex errorCode, CFStringRef domain, CFErrorRef previousError, CFErrorRef *newError,
                                CFDictionaryRef formatoptions, CFStringRef descriptionString, ...)
                        CF_FORMAT_FUNCTION(6,7);


bool SecCFCreateErrorWithFormatAndArguments(CFIndex errorCode, CFStringRef domain,
                                            CFErrorRef previousError, CFErrorRef *newError,
                                            CFDictionaryRef formatoptions, CFStringRef descriptionString, va_list args)
                                CF_FORMAT_FUNCTION(6, 0);

#endif
