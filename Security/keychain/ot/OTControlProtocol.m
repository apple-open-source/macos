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
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTJoiningConfiguration.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"
#import <Security/SecXPCHelper.h>
#include <utilities/debugging.h>

NSXPCInterface* OTSetupControlProtocol(NSXPCInterface* interface) {
#if OCTAGON
    NSSet<Class> *errorClasses = [SecXPCHelper safeErrorClasses];

    @try {
        [interface setClasses:errorClasses forSelector:@selector(appleAccountSignedIn:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(appleAccountSignedOut:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(notifyIDMSTrustLevelChangeForAltDSID:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(rpcEpochWithArguments:configuration:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(rpcPrepareIdentityAsApplicantWithArguments:configuration:reply:) argumentIndex:5 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(rpcVoucherWithArguments:configuration:peerID:permanentInfo:permanentInfoSig:stableInfo:stableInfoSig:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(rpcJoinWithArguments:configuration:vouchData:vouchSig:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(status:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(status:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchEgoPeerID:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchCliqueStatus:configuration:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchTrustStatus:configuration:reply:) argumentIndex:4 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(startOctagonStateMachine:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(resetAndEstablish:resetReason:idmsTargetContext:idmsCuttlefishPassword:notifyIdMS:accountSettings:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(establish:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(leaveClique:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(removeFriendsInClique:peerIDs:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(peerDeviceNamesByPeerID:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchAllViableBottles:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(restoreFromBottle:entropy:bottleID:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchEscrowContents:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(createRecoveryKey:recoveryKey:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(joinWithRecoveryKey:recoveryKey:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(healthCheck:skipRateLimitingCheck:repair:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(waitForOctagonUpgrade:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(postCDPFollowupResult:success:type:error:reply:) argumentIndex:3 ofReply:NO];
        [interface setClasses:errorClasses forSelector:@selector(postCDPFollowupResult:success:type:error:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(tapToRadar:description:radar:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(refetchCKKSPolicy:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(setCDPEnabled:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(getCDPStatus:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchEscrowRecords:source:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(setUserControllableViewsSyncStatus:enabled:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchUserControllableViewsSyncStatus:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(resetAccountCDPContents:idmsTargetContext:idmsCuttlefishPassword:notifyIdMS:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(setLocalSecureElementIdentity:secureElementIdentity:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(removeLocalSecureElementIdentityPeerID:secureElementIdentityPeerID:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchTrustedSecureElementIdentities:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(setAccountSetting:setting:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchAccountSettings:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(fetchAccountWideSettingsWithForceFetch:arguments:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(waitForPriorityViewKeychainDataRecovery:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(createCustodianRecoveryKey:uuid:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(joinWithCustodianRecoveryKey:custodianRecoveryKey:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(preflightJoinWithCustodianRecoveryKey:custodianRecoveryKey:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(removeCustodianRecoveryKey:uuid:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(createInheritanceKey:uuid:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(generateInheritanceKey:uuid:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(storeInheritanceKey:ik:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(joinWithInheritanceKey:inheritanceKey:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(preflightJoinWithInheritanceKey:inheritanceKey:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(removeInheritanceKey:uuid:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(tlkRecoverabilityForEscrowRecordData:recordData:source:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(deliverAKDeviceListDelta:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(setMachineIDOverride:machineID:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(isRecoveryKeySet:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(recoverWithRecoveryKey:recoveryKey:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(removeRecoveryKey:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(preflightRecoverOctagonUsingRecoveryKey:recoveryKey:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(resetAcountData:resetReason:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(totalTrustedPeers:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errorClasses forSelector:@selector(areRecoveryKeysDistrusted:reply:) argumentIndex:1 ofReply:YES];
    }
    @catch(NSException* e) {
        secerror("OTSetupControlProtocol failed, continuing, but you might crash later: %@", e);
        @throw e;
    }
#endif
    
    return interface;
}
