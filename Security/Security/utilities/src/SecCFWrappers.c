/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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


#include <utilities/SecCFWrappers.h>

//
// Global sigleton Zulu time.
//
CFGiblisGetSingleton(CFCalendarRef, SecCFCalendarGetZulu, zuluCalendar, ^{
    *zuluCalendar = CFCalendarCreateWithIdentifier(kCFAllocatorDefault, kCFGregorianCalendar);
    CFTimeZoneRef tz = CFTimeZoneCreateWithTimeIntervalFromGMT(kCFAllocatorDefault, 0.0);
    CFCalendarSetTimeZone(*zuluCalendar, tz);
    CFReleaseSafe(tz);
})



void CFStringPerformWithCStringAndLength(CFStringRef inStr, void(^operation)(const char *utf8String, size_t utf8Length)) {
    const char *cstr = CFStringGetCStringPtr(inStr, kCFStringEncodingUTF8);
    if (cstr) {
        operation(cstr, strlen(cstr));
    } else {
        CFIndex neededLen = 0;
        CFRange range = { 0, CFStringGetLength(inStr) };
        CFStringGetBytes(inStr, range, kCFStringEncodingUTF8,
                         0, FALSE, NULL, 0, &neededLen);
        CFIndex usedLen = 0;
        if (neededLen < 4096) {
            char buf[neededLen + 1];
            CFStringGetBytes(inStr, range, kCFStringEncodingUTF8,
                             0, FALSE, (UInt8 *)buf, neededLen, &usedLen);
            assert(usedLen == neededLen);
            buf[usedLen] = 0;
            operation(buf, (size_t)usedLen);
            //cc_zero(neededLen, buf);
        } else {
            char *buf = malloc(neededLen + 1);
            CFStringGetBytes(inStr, range, kCFStringEncodingUTF8,
                             0, FALSE, (UInt8 *)buf, neededLen, &usedLen);
            assert(usedLen == neededLen);
            buf[usedLen] = 0;
            operation(buf, (size_t)usedLen);
            //cc_zero(neededLen, buf);
            free(buf);
        }
    }
}

void CFStringPerformWithCString(CFStringRef inStr, void(^operation)(const char *utf8String)) {
    const char *cstr = CFStringGetCStringPtr(inStr, kCFStringEncodingUTF8);
    if (cstr) {
        operation(cstr);
    } else {
        CFIndex neededLen = 0;
        CFRange range = { 0, CFStringGetLength(inStr) };
        CFStringGetBytes(inStr, range, kCFStringEncodingUTF8,
                         0, FALSE, NULL, 0, &neededLen);
        CFIndex usedLen = 0;
        if (neededLen < 4096) {
            char buf[neededLen + 1];
            CFStringGetBytes(inStr, range, kCFStringEncodingUTF8,
                             0, FALSE, (UInt8 *)buf, neededLen, &usedLen);
            assert(usedLen == neededLen);
            buf[usedLen] = 0;
            operation(buf);
            //cc_zero(neededLen, buf);
        } else {
            char *buf = malloc(neededLen + 1);
            CFStringGetBytes(inStr, range, kCFStringEncodingUTF8,
                             0, FALSE, (UInt8 *)buf, neededLen, &usedLen);
            assert(usedLen == neededLen);
            buf[usedLen] = 0;
            operation(buf);
            //cc_zero(neededLen, buf);
            free(buf);
        }
    }
}
