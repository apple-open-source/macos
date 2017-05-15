/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#include <Security/SecRecoveryPassword.h>

#include "keychain_regressions.h"

static void tests(void)
{
    const void *qs[] = {CFSTR("q1"), CFSTR("q2"), CFSTR("q3")};
    CFArrayRef questions = CFArrayCreate(kCFAllocatorDefault, qs, 3, NULL);
    
    const void *as[] = {CFSTR("a1"), CFSTR("a2"), CFSTR("a3")};
    CFArrayRef answers = CFArrayCreate(kCFAllocatorDefault, as, 3, NULL);
    
    CFStringRef password = CFSTR("AAAA-AAAA-AAAA-AAAA-AAAA-AAAA");
    
    CFDictionaryRef wrappedPassword = SecWrapRecoveryPasswordWithAnswers(password, questions, answers);
    isnt(wrappedPassword, NULL, "wrappedPassword NULL");
    
    CFStringRef recoveredPassword = SecUnwrapRecoveryPasswordWithAnswers(wrappedPassword, answers);
    isnt(recoveredPassword, NULL, "recoveredPassword NULL");

    is(CFStringCompare(password, recoveredPassword, 0), kCFCompareEqualTo, "SecRecoveryPassword");
    
    CFRelease(questions);
    CFRelease(answers);
    CFRelease(wrappedPassword);
    CFRelease(recoveredPassword);
}

int kc_44_secrecoverypassword(int argc, char *const *argv)
{
    plan_tests(3);
    tests();
    
    return 0;
}

