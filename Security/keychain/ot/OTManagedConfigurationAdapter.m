/*
 * Copyright (c) 2023 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/OTManagedConfigurationAdapter.h"

#if TARGET_OS_OSX
#define KEYCHAINSYNCDISABLE "DisableKeychainCloudSync"
#define ICLOUDMANAGEDENVIRONMENT "com.apple.icloud.managed"
#else
#import <ManagedConfiguration/ManagedConfiguration.h>
#import <SoftLinking/SoftLinking.h>

SOFT_LINK_FRAMEWORK(PrivateFrameworks, ManagedConfiguration)
SOFT_LINK_CLASS(ManagedConfiguration, MCProfileConnection)
#endif

@implementation OTManagedConfigurationActualAdapter

- (BOOL)isCloudKeychainSyncAllowed
{
#if TARGET_OS_OSX
    return !CFPreferencesGetAppBooleanValue(CFSTR(KEYCHAINSYNCDISABLE), CFSTR(ICLOUDMANAGEDENVIRONMENT), NULL);
#else
    Class mpc = getMCProfileConnectionClass();
    MCProfileConnection *sharedConnection = [mpc sharedConnection];
    return [sharedConnection isCloudKeychainSyncAllowed];
#endif
}

@end
