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
#import "keychain/ot/OTClique.h"
#import "keychain/ot/OTControlProtocol.h"
#import "keychain/ot/OTJoiningConfiguration.h"
#import <Security/SecXPCHelper.h>

#if OCTAGON
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import <utilities/debugging.h>
#include <dlfcn.h>
#import <KeychainCircle/KeychainCircle.h>
#import <SecurityFoundation/SFKey.h>
#endif // OCTAGON


NSXPCInterface* OTSetupControlProtocol(NSXPCInterface* interface) {
#if OCTAGON
    NSSet<Class> *errorClasses = [SecXPCHelper safeErrorClasses];

    @try {
        [interface setClasses:errorClasses forSelector:@selector(restore:dsid:secret:escrowRecordID:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(octagonEncryptionPublicKey:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(octagonSigningPublicKey:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(listOfEligibleBottledPeerRecords:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(signOut:context:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(signIn:container:context:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(reset:) argumentIndex:0 ofReply:YES];

        [interface setClasses:errorClasses forSelector:@selector(preflightBottledPeer:dsid:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(launchBottledPeer:bottleID:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(scrubBottledPeer:bottleID:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(handleIdentityChangeForSigningKey:ForEncryptionKey:ForPeerID:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchCliqueStatus:context:configuration:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchTrustStatus:context:configuration:reply:) argumentIndex:4 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchEscrowContents:contextID:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchAllViableBottles:context:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(createRecoveryKey:contextID:recoveryKey:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(joinWithRecoveryKey:contextID:recoveryKey:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(healthCheck:context:skipRateLimitingCheck:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(attemptSosUpgrade:context:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(waitForOctagonUpgrade:context:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(postCDPFollowupResult:type:error:containerName:contextName:reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:errorClasses forSelector:@selector(postCDPFollowupResult:type:error:containerName:contextName:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(tapToRadar:description:radar:reply:) argumentIndex:0 ofReply:YES];

#if __OBJC2__

        [interface setClasses:errorClasses
                  forSelector:@selector(rpcEpochWithConfiguration:reply:)
                argumentIndex:1
                      ofReply:YES];
        [interface setClasses:errorClasses
                  forSelector:@selector(rpcPrepareIdentityAsApplicantWithConfiguration:reply:)
                argumentIndex:5
                      ofReply:YES];
        [interface setClasses:errorClasses
                  forSelector:@selector(rpcVoucherWithConfiguration:peerID:permanentInfo:permanentInfoSig:stableInfo:stableInfoSig:reply:)
                argumentIndex:2
                      ofReply:YES];
        [interface setClasses:errorClasses
                  forSelector:@selector(rpcJoinWithConfiguration:vouchData:vouchSig:preapprovedKeys:reply:)
                argumentIndex:0
                      ofReply:YES];
#endif /* __OBJC2__ */
    }
    @catch(NSException* e) {
        secerror("OTSetupControlProtocol failed, continuing, but you might crash later: %@", e);
#if DEBUG
        @throw e;
#endif
    }
#endif
    
    return interface;
}


