/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

#import <TargetConditionals.h>
#import "ipc/securityd_client.h"
#import "keychain/ot/tests/OTMockPersonaAdapter.h"

// This mock layer will fail for persona OctagonTests once we adopt UserManagement test infrastructure (when it becomes available)

@implementation OTMockPersonaAdapter

- (instancetype)init
{
    if((self = [super init])) {
        _isDefaultPersona = YES;
        _currentPersonaString = [OTMockPersonaAdapter defaultMockPersonaString];
    }
    return self;
}

- (NSString*)currentThreadPersonaUniqueString
{
    return self.currentPersonaString;
}

- (BOOL)currentThreadIsForPrimaryiCloudAccount {
    return self.isDefaultPersona;
}

+ (NSString*)defaultMockPersonaString
{
    return @"MOCK_PERSONA_IDENTIFIER";
}

- (void)prepareThreadForKeychainAPIUseForPersonaIdentifier:(NSString* _Nullable)personaUniqueString
{
    
#if KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER
    // Note that this is a global override, and so is not thread-safe at all.
    // I can't find a way to simulate persona attachment to threads in the face of dispatch_async.
    // If you get strange test behavior with the keychain, suspect simultaneous access from different threads with expected persona musrs.
    if(personaUniqueString == nil || [personaUniqueString isEqualToString:[OTMockPersonaAdapter defaultMockPersonaString]]) {
        SecSecuritySetPersonaMusr(NULL);
    } else {
        SecSecuritySetPersonaMusr((__bridge CFStringRef)personaUniqueString);
    }
#endif
}

- (void)performBlockWithPersonaIdentifier:(NSString* _Nullable)personaUniqueString
                                     block:(void (^) (void)) block
{
#if KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER
    [self prepareThreadForKeychainAPIUseForPersonaIdentifier: personaUniqueString];
    block();
    // once UserManagement supplies some testing infrastructure that actually changes the current thread's persona in xctests, we should change this routine to
    // mimick the performBlockWithPersonaIdentifier from OTPersonaAdapter (where we look up the current thread's persona and restore it after the block executes.)
    [self prepareThreadForKeychainAPIUseForPersonaIdentifier: personaUniqueString];
#else
    block();
#endif
}


@end
