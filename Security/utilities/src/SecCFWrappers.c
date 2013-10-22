//
//  SecCFWrappers.c
//  utilities
//
//  Created by Michael Brouwer on 11/15/12.
//  Copyright 2012 Apple Inc. All rights reserved.
//

#include <utilities/SecCFWrappers.h>

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
