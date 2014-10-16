/*
 * Copyright (c) 2000-2012,2014 Apple Inc. All Rights Reserved.
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


#include "debugging_internal.h"
#include <stdarg.h>
#include <CoreFoundation/CoreFoundation.h>

void secdebug_internal(const char* scope, const char* format, ...)
{
    if (__builtin_expect(SECURITY_DEBUG_LOG_ENABLED(), 0))
    {
        va_list list;
        va_start(list, format);
        
        CFStringRef formatString = CFStringCreateWithCString(NULL, format, kCFStringEncodingUTF8);
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, formatString, list);
        CFRelease(formatString);
        CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(message), kCFStringEncodingUTF8) + 1;
        char buffer[maxLength];
        CFStringGetCString(message, buffer, sizeof(buffer), kCFStringEncodingUTF8);
        CFRelease(message);
        SECURITY_DEBUG_LOG((char *)(scope), (buffer));
        
        va_end(list);
    }
}
