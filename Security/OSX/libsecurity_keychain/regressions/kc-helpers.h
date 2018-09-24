/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#ifndef kc_helpers_h
#define kc_helpers_h

#include <stdlib.h>
#include <unistd.h>

#include <Security/Security.h>
#include <Security/SecKeychainPriv.h>
#include "utilities/SecCFRelease.h"

#include "kc-keychain-file-helpers.h"

extern char keychainFile[1000];
extern char keychainDbFile[1000];
extern char keychainTempFile[1000];
extern char keychainName[1000];
extern char testName[1000];

/* redefine this since the headers are mixed up */
static inline bool CFEqualSafe(CFTypeRef left, CFTypeRef right)
{
    if (left == NULL || right == NULL)
        return left == right;
    else
        return CFEqual(left, right);
}

void startTest(const char* thisTestName);

void initializeKeychainTests(const char* thisTestName);

// Use this at the bottom of every test to make sure everything is gone
void deleteTestFiles(void);

void addToSearchList(SecKeychainRef keychain);

/* Checks to be sure there are N elements in this search, and returns the first
 * if it exists. */
SecKeychainItemRef checkNCopyFirst(char* testName, const CFDictionaryRef CF_CONSUMED query, uint32_t n);

void checkN(char* testName, const CFDictionaryRef CF_CONSUMED query, uint32_t n);
#define checkNTests 3

void readPasswordContentsWithResult(SecKeychainItemRef item, OSStatus expectedResult, CFStringRef expectedContents);
#define readPasswordContentsWithResultTests 3

void readPasswordContents(SecKeychainItemRef item, CFStringRef expectedContents);
#define readPasswordContentsTests readPasswordContentsWithResultTests

void changePasswordContents(SecKeychainItemRef item, CFStringRef newPassword);
#define changePasswordContentsTests 1

void deleteItem(SecKeychainItemRef item);
#define deleteItemTests 1

void deleteItems(CFArrayRef items);
#define deleteItemsTests 1

/* Checks in with securityd to see how many prompts were generated since the last call to this function, and tests against the number expected.
 Returns the number generated since the last call. */
uint32_t checkPrompts(uint32_t expectedSinceLastCall, char* explanation);
#define checkPromptsTests 2

#endif /* kc_helpers_h */
