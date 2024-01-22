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

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection.h>
#import <objc/runtime.h>
#import <Security/SecXPCHelper.h>
#include <utilities/debugging.h>

#import "keychain/SecureObjectSync/SOSTypes.h"
#import "keychain/SecureObjectSync/SOSControlHelper.h"

void
_SOSControlSetupInterface(NSXPCInterface *interface)
{
    NSSet<Class> *errClasses = [SecXPCHelper safeErrorClasses];

    @try {
        [interface setClasses:errClasses forSelector:@selector(userPublicKey:) argumentIndex:2 ofReply:YES];

        [interface setClasses:errClasses forSelector:@selector(stashedCredentialPublicKey:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(assertStashedAccountCredential:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(validatedStashedAccountCredential:flowID:deviceSessionID:canSendMetrics:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(stashAccountCredential:altDSID:flowID:deviceSessionID:canSendMetrics:complete:) argumentIndex:1 ofReply:YES];

        [interface setClasses:errClasses forSelector:@selector(ghostBust:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(ghostBustPeriodic:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(ghostBustTriggerTimed:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(ghostBustInfo:) argumentIndex:0 ofReply:YES];

        [interface setClasses:errClasses forSelector:@selector(iCloudIdentityStatus:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(accountStatus:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(keyStatusFor:complete:) argumentIndex:1 ofReply:YES];

        [interface setClasses:errClasses forSelector:@selector(myPeerInfo:flowID:deviceSessionID:canSendMetrics:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(circleHash:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(circleJoiningBlob:flowID:deviceSessionID:canSendMetrics:applicant:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(joinCircleWithBlob:altDSID:flowID:deviceSessionID:canSendMetrics:version:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(initialSyncCredentials:altDSID:flowID:deviceSessionID:canSendMetrics:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(importInitialSyncCredentials:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcTriggerSync:complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(getWatchdogParameters:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(setWatchdogParmeters:complete:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcTriggerBackup:complete:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcTriggerRingUpdate:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(removeV0Peers:) argumentIndex:1 ofReply:YES];
    }
    @catch(NSException* e) {
        secerror("Could not configure SOSControlHelper: %@", e);
        @throw e;
    }
}
