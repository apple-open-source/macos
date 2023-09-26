/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

#import "SecurityTool/sharedTool/tool_errors.h"
#import "SecurityCommands.h"
#import "tool_auth_helpers.h"

int stuff_keychain(int argc, char * const *argv)
{
    int ch;
    long count = 25000;
    NSString* acctPrefix = @"account-";
    bool sync = false;
    bool delete = false;
    bool authSucceeded = false;

    srandomdev();

    while ((ch = getopt(argc, argv, "a:c:e:sDyY:")) != -1)
    {
        switch  (ch)
        {
            case 'a':
                acctPrefix = [NSString stringWithCString:optarg encoding:NSUTF8StringEncoding];
                break;
            case 'c':
                count = strtol(optarg, NULL, 10);
                if (count <= 0) {
                    fprintf(stderr, "Count must be >= 0\n");
                    return 1;
                }
                break;
            case 'e':
            {
                long seed = strtol(optarg, NULL, 10);
                if (seed < 0) {
                    fprintf(stderr, "Seed must be >= 0\n");
                    return 1;
                }
                srandom((unsigned)seed);
                break;
            }
            case 's':
                sync = true;
                break;
            case 'D':
                delete = true;
                break;
            case 'y':
                if (!promptForAndCheckPassphrase()) {
                    return 1;
                }
                authSucceeded = true;
                break;
            case 'Y':
                if (!checkPassphrase(optarg, 0) ) {
                    return 1;
                }
                authSucceeded = true;
                break;
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    if (!authSucceeded) {
        fprintf(stderr, "Authentication is required to interact with keychain items. Add -y after the subcommand for interactive authentication, or -Y <passcode> to authenticate directly, which is insecure.\n");
        return 1;
    }

    @autoreleasepool {
        NSMutableDictionary* query = [@{(id)kSecClass : (id)kSecClassGenericPassword,
                                        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
                                        (id)kSecAttrAccessGroup : @"com.apple.private.security.keychainstuffer",
                                        (id)kSecAttrLabel : @"KeychainStuffer",
                                        (id)kSecUseDataProtectionKeychain : @YES,
                                        } mutableCopy];

        if (delete) {
            NSLog(@"Unstuffing the keychain!");

            query[(id)kSecAttrSynchronizable] = (id)kSecAttrSynchronizableAny;
            OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
            if (status != errSecSuccess) {
                NSLog(@"KeychainStuffer failed to delete items: %d", (int)status);
                return 1;
            }
            return 0;
        }

        NSLog(@"Stuffing the keychain!");

        if (sync) {
            query[(id)kSecAttrSynchronizable] = @YES;
        }

        for (long idx = 1; idx <= count; ++idx) {
            query[(id)kSecAttrAccount] = [NSString stringWithFormat:@"%@%ld-%ld", acctPrefix, idx, random()];
            query[(id)kSecValueData] = [[NSString stringWithFormat:@"swordfish-%ld-%ld", idx, random()] dataUsingEncoding:NSUTF8StringEncoding];

            OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, NULL);
            if (status != errSecSuccess && status != errSecDuplicateItem) {
                NSLog(@"KeychainStuffer failed to add item %ld to keychain: %d", idx, (int)status);
                return 1;
            }
            if (!(idx % 1000)) {
                NSLog(@"Still stuffing! %ld items so far", idx);
            }
        }

        NSLog(@"Stuffed %ld items. So full right now...", count);

        return 0;
    }
}
