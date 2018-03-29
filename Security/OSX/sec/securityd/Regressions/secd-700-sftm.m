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

//
//  secd-700-sftm.m
//

#import <Foundation/Foundation.h>
#import "secd_regressions.h"
#import "SecdTestKeychainUtilities.h"

#import "keychain/Signin Metrics/SFTransactionMetric.h"

static void test()
{
    SFTransactionMetric *metric = [[SFTransactionMetric alloc] initWithUUID:@"UUID" category:@"CoreCDP"];
    NSError *error = [[NSError alloc] initWithDomain:@"TestErrorDomain" code:42 userInfo:@{}];
    [metric logError:error];
    
    NSDictionary* eventAttributes = @{@"wait for initial sync time" : @"90 s", @"event result" : @"success"};
    [metric logEvent:@"event" eventAttributes:eventAttributes];
    
    NSDictionary *query = @{
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrLabel : @"TestLabel",
                            (id)kSecAttrAccessGroup : @"com.apple.security.wiiss",
                            };
    
    [metric timeEvent:@"Adding item to keychain" blockToTime:^{
        CFTypeRef result;
        SecItemAdd((__bridge CFDictionaryRef)query, &result);
    }];
    
    [metric signInCompleted];
}

int secd_700_sftm(int argc, char *const *argv)
{
    plan_tests(1);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    test();
    
    return 0;
}
