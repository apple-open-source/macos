/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/OTLAContextAdapter.h"
#import "utilities/debugging.h"
#import <LocalAuthentication/LocalAuthentication_Private.h>
#import <LocalAuthentication/LAContext.h>
#import <libaks.h>

@implementation OTLAContextActualAdapter

- (BOOL)setCredential:(NSData *)credential type:(LACredentialType)type laContext:(LAContext**)laContext error:(NSError **)error
{
    LAContext* localLAContext = [[LAContext alloc] init];
    if (laContext){
        *laContext = localLAContext;
    }
    return [localLAContext setCredential:credential type:type error:error];
}

- (void)discardPasscodeStashSecret:(cache_flow_enabled_context_t)contextType
{
    LAContext* laContext = nil;
    NSError* error = nil;

    BOOL success = [self setCredential:[NSData data] type:LACredentialTypePasscodeStashSecret laContext:&laContext error:&error];
    if (!success || error) {
        secnotice("octagon", "failed to discard passcode stash: %@", error);
    }

    // Explicitly discard the context.
    laContext = nil;

    switch (contextType) {
    case cache_flow_enabled_passcode_validated:
    case cache_flow_enabled_passcode_unlocked:
        secnotice("octagon", "enabling cache flow after discarding passcode stash");
        kern_return_t kr = aks_enable_cache_flow(session_keybag_handle);
        if (kr != kAKSReturnSuccess) {
            secnotice("octagon", "aks_enable_cache_flow failed: %x", kr);
        }
        break;
    default:
        break;
    }
}

@end
