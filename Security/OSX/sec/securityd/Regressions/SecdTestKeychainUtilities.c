/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include "SecdTestKeychainUtilities.h"

#include <test/testmore.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecCFWrappers.h>
#include <securityd/SecItemServer.h>
#include <Security/SecureObjectSync/SOSViews.h>


#include <CoreFoundation/CoreFoundation.h>

//#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

void secd_test_setup_temp_keychain(const char* test_prefix, dispatch_block_t do_in_reset)
{
    CFStringRef tmp_dir = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("/tmp/%s.%X/"), test_prefix, arc4random());
    CFStringRef keychain_dir = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@Library/Keychains"), tmp_dir);
    
    CFStringPerformWithCString(keychain_dir, ^(const char *keychain_dir_string) {
        ok_unix(mkpath_np(keychain_dir_string, 0755), "Create temp dir %s", keychain_dir_string);
    });
    
    
    /* set custom keychain dir, reset db */
    SetCustomHomeURLString(tmp_dir);

    SecKeychainDbReset(do_in_reset);

    CFReleaseNull(tmp_dir);
    CFReleaseNull(keychain_dir);
}

CFStringRef kTestView1 = CFSTR("TestView1");
CFStringRef kTestView2 = CFSTR("TestView2");

void secd_test_setup_testviews(void) {
    static dispatch_once_t onceToken = 0;
    
    dispatch_once(&onceToken, ^{
        CFMutableSetRef testViews = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        CFSetAddValue(testViews, kTestView1);
        CFSetAddValue(testViews, kTestView2);
        
        SOSViewsSetTestViewsSet(testViews);
        CFReleaseNull(testViews);
    });
}



