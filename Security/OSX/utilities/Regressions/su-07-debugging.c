/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


#include <stdio.h>

#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecIOFormat.h>

#include "utilities_regressions.h"
#include "utilities/debugging.h"
#include "utilities/debugging_test.h"

#if USINGOLDLOGGING
#define kTestCount (39)

static void
tests(void) {
    ok(IsScopeActive(SECLOG_LEVEL_ERR, NULL), "Errors are active by default");

    ok(!IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("testscope")), "scope is off");

    ApplyScopeListForIDC("-first", kScopeIDXPC);

    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("testscope")), "scope is on");
    ok(!IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("first")), "scope is off");

    ApplyScopeListForIDC("first", kScopeIDXPC);

    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("first")), "scope is on");
    ok(!IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("testscope")), "scope is off");

    ApplyScopeListForIDC("testscope, bar, baz,frog", kScopeIDXPC);

    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("testscope")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("bar")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("baz")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("frog")), "scope is on");
    ok(!IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("bonzo")), "scope is off");
    ok(!IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("nothing")), "scope is off");

    ApplyScopeListForID(CFSTR("-bonzo, boy"), kScopeIDDefaults);

    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("testscope")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("bar")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("baz")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("frog")), "scope is on");
    ok(!IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("bonzo")), "scope is off");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("nothing")), "scope is on");

    ApplyScopeListForID(CFSTR(""), kScopeIDDefaults);

    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("testscope")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("bar")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("baz")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("frog")), "scope is on");
    ok(!IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("bonzo")), "scope is off");
    ok(!IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("nothing")), "scope is on");

    int value = SECLOG_LEVEL_NOTICE;
    CFNumberRef noticeNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);

    value = SECLOG_LEVEL_INFO;
    CFNumberRef infoNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);

    CFDictionaryRef settings_dictionary = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                       CFSTR(ASL_STRING_DEBUG),     CFSTR("-baz"),
                                                                       CFSTR(ASL_STRING_WARNING),   CFSTR("baz,bar"),
                                                                       noticeNumber,                CFSTR("bar"),
                                                                       infoNumber,                  CFSTR("baz"),
                                                                       NULL);

    ApplyScopeDictionaryForID(settings_dictionary, kScopeIDXPC);

    ok(!IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("testscope")), "scope is off");
    ok(!IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("bar")), "scope is off");
    ok(IsScopeActive(SECLOG_LEVEL_INFO, CFSTR("baz")), "scope is on");

    ok(!IsScopeActive(SECLOG_LEVEL_NOTICE, CFSTR("testscope")), "scope is off");
    ok(IsScopeActive(SECLOG_LEVEL_NOTICE, CFSTR("bar")), "scope is on");
    ok(!IsScopeActive(SECLOG_LEVEL_NOTICE, CFSTR("baz")), "scope is off");

    ok(!IsScopeActive(SECLOG_LEVEL_WARNING, CFSTR("testscope")), "scope is off");
    ok(IsScopeActive(SECLOG_LEVEL_WARNING, CFSTR("bar")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_WARNING, CFSTR("baz")), "scope is on");

    ok(IsScopeActive(SECLOG_LEVEL_DEBUG, CFSTR("testscope")), "scope is on");
    ok(IsScopeActive(SECLOG_LEVEL_DEBUG, CFSTR("bar")), "scope is on");
    ok(!IsScopeActive(SECLOG_LEVEL_DEBUG, CFSTR("baz")), "scope is off");

    ok(!IsScopeActive(SECLOG_LEVEL_ALERT, CFSTR("testscope")), "scope is off");
    ok(!IsScopeActive(SECLOG_LEVEL_ALERT, CFSTR("bar")), "scope is off");
    ok(!IsScopeActive(SECLOG_LEVEL_ALERT, CFSTR("baz")), "scope is off");

    CFReleaseSafe(noticeNumber);
    CFReleaseSafe(infoNumber);
    CFReleaseSafe(settings_dictionary);

    ApplyScopeListForIDC("", kScopeIDXPC);
    ApplyScopeListForIDC("", kScopeIDDefaults);

}

#if !defined(NDEBUG)
#define kTestLogCount (6 + 5)
#else
#define kTestLogCount (6 + 1)
#endif

static void
testLog()
{
    int value = SECLOG_LEVEL_NOTICE;
    CFNumberRef noticeNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);

    value = SECLOG_LEVEL_INFO;
    CFNumberRef infoNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);

    CFDictionaryRef settings_dictionary = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                       CFSTR(ASL_STRING_DEBUG),     CFSTR("-baz"),
                                                                       CFSTR(ASL_STRING_WARNING),   CFSTR("baz,bar"),
                                                                       noticeNumber,                CFSTR("-bar"),
                                                                       infoNumber,                  CFSTR("baz"),
                                                                       NULL);


    ApplyScopeDictionaryForID(settings_dictionary, kScopeIDXPC);

    __block int level = -1;
    __block CFStringRef scope = NULL;
    __block CFStringRef message = NULL;
    __block CFStringRef file = NULL;
    __block CFStringRef function = NULL;
    __block int line = 0;

    __block bool called = false;

    security_log_handler verify = ^(int level_sent, CFStringRef scope_sent, const char *functionC,
                                     const char *fileC, int line_sent, CFStringRef message_sent) {
        called = true;

        level = level_sent;
        scope = CFRetainSafe(scope_sent);
        file = CFStringCreateWithCString(kCFAllocatorDefault, fileC, kCFStringEncodingUTF8);
        function = CFStringCreateWithCString(kCFAllocatorDefault, functionC, kCFStringEncodingUTF8);
        line = line_sent;
        message = CFRetainSafe(message_sent);
    };

    add_security_log_handler(verify);

    called = false; CFReleaseNull(scope); CFReleaseNull(message); CFReleaseNull(file); CFReleaseNull(function); level = -1; line = 0;

    secinfo("bar", "Get this!");

#if !defined(NDEBUG)
    is(called, true, "Handler called");
    is(level, SECLOG_LEVEL_DEBUG, "level");
    eq_cf(scope, CFSTR("bar"), "Scope");
    eq_cf(message, CFSTR("Get this!"), "message");
    eq_cf(function, CFSTR("testLog"), "function");
#else
    is(called, false, "Handler not called");
#endif

    called = false;
    CFReleaseNull(scope);
    CFReleaseNull(message);
    CFReleaseNull(file);
    CFReleaseNull(function);

    secnotice("bunz", "Get this, too!");

    is(called, true, "Handler called");
    is(level, SECLOG_LEVEL_NOTICE, "level");
    eq_cf(scope, CFSTR("bunz"), "Scope");
    eq_cf(message, CFSTR("Get this, too!"), "message");
    eq_cf(function, CFSTR("testLog"), "function");

    CFReleaseNull(scope);
    CFReleaseNull(message);
    CFReleaseNull(file);
    CFReleaseNull(function);

    remove_security_log_handler(verify);

    CFReleaseSafe(settings_dictionary);
    CFReleaseSafe(infoNumber);
    CFReleaseSafe(noticeNumber);

    CFPropertyListRef result = CopyCurrentScopePlist();

    ok(result, "exported");

    CFReleaseSafe(result);
}
#endif


int
su_07_debugging(int argc, char *const *argv) {
#if USINGOLDLOGGING
    plan_tests(kTestCount + kTestLogCount);
    tests();
    testLog();
#else
    plan_tests(1);
    ok(1, "Using os_log");
#endif

    return 0;
}
