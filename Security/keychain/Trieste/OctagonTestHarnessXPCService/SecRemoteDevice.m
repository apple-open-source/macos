/*
 * Copyright (c) 2017 - 2018 Apple Inc. All Rights Reserved.
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
#import <Foundation/NSXPCConnection_Private.h>
#import "keychain/securityd/SOSCloudCircleServer.h"
#import <Security/SecureObjectSync/SOSPeerInfo.h>
#import <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#import <Security/SecureObjectSync/SOSViews.h>
#import "keychain/SecureObjectSync/SOSTypes.h"
#import "keychain/SecureObjectSync/SOSInternal.h"
#import "keychain/SecureObjectSync/SOSAuthKitHelpers.h"
#import "OSX/sec/Security/SecItemShim.h"

#import <stdlib.h>
#import <unistd.h>
#import <libproc.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ot/OTManager.h"
#import "keychain/ot/OT.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"
#import "keychain/SecureObjectSync/SOSControlServer.h"
#import "KeychainCircle/PairingChannel.h"

#import "SharedMocks/NSXPCConnectionMock.h"

#import "SecRemoteDeviceProtocol.h"
#import "SecRemoteDevice.h"

@interface  DevicePairingSimulator : NSObject <DevicePairingProtocol>
@property KCPairingChannelContext *remoteVersionContext;
@property KCPairingChannel *channel;
@property SecRemoteDevice *device;
@property (assign) bool initiator;
@property (assign) bool haveHandshakeCompleted;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initAsInitiator:(bool)initiator version:(KCPairingChannelContext *)peerVersionContext device:(SecRemoteDevice *)device;
@end

@implementation DevicePairingSimulator

- (instancetype)initAsInitiator:(bool)initiator version:(KCPairingChannelContext *)peerVersionContext device:(SecRemoteDevice *)device
{
    self = [super init];
    if (self) {
        self.remoteVersionContext = peerVersionContext;
        self.initiator = initiator;
        self.device = device;
        self.channel = [[KCPairingChannel alloc] initAsInitiator:initiator version:peerVersionContext];
#if SECD_SERVER
        [self.channel setXPCConnectionObject:(NSXPCConnection *)[[NSXPCConnectionMock alloc] initWithRealObject:SOSControlServerInternalClient()]];
#endif
    }

    return self;
}

- (void)exchangePacket:(NSData *)data complete:(void (^)(bool complete, NSData *result, NSError *error))complete
{
    os_log(NULL, "[%@] exchangePacket", self.device.name);

    if (self.haveHandshakeCompleted || self.channel == NULL) {
        abort();
    }
    [self.channel exchangePacket:data complete:^void(BOOL handshakeComplete, NSData *packet, NSError *error) {
        self.haveHandshakeCompleted = handshakeComplete;
        os_log(NULL, "[%@] exchangePacket:complete: %d", self.device.name, handshakeComplete);
        complete(handshakeComplete, packet, error);
    }];
}

- (void)validateStart:(void(^)(bool result, NSError *error))complete
{
    if (self.channel == NULL) {
        abort();
    }
    [self.channel validateStart:^(bool result, NSError *error) {
        complete(result, error);
    }];
}
@end


@implementation SecRemoteDevice


- (void)setUserCredentials:(NSString *)username password:(NSString *)password complete:(void (^)(bool success, NSError *error))complete
{
    CFErrorRef cferror = NULL;
    bool result = SOSCCSetUserCredentialsAndDSID((__bridge CFStringRef)username,
                                                 (__bridge CFDataRef)[password dataUsingEncoding:NSUTF8StringEncoding],
                                                 CFSTR("1"), &cferror);
    complete(result, (__bridge NSError *)cferror);
    CFReleaseNull(cferror);
}

- (void)setupSOSCircle:(NSString *)username password:(NSString *)password complete:(void (^)(bool success, NSError *error))complete
{
    CFErrorRef cferror = NULL;
    bool result = SOSCCSetUserCredentialsAndDSID((__bridge CFStringRef)username,
                                                 (__bridge CFDataRef)[password dataUsingEncoding:NSUTF8StringEncoding],
                                                 CFSTR("1"), &cferror);
    if (result) {
        result = SOSCCResetToOffering(&cferror);
    }
    complete(result, (__bridge NSError *)cferror);
    CFReleaseNull(cferror);
}

- (void)sosCircleStatus:(void(^)(SOSCCStatus status, NSError *error))complete
{
    SOSCloudKeychainFlush(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef __unused returnedValues, CFErrorRef __unused sync_error) {
        CFErrorRef cferror = NULL;
        SOSCCStatus status = SOSCCThisDeviceIsInCircle(&cferror);
        complete(status, (__bridge NSError *)cferror);
        CFReleaseNull(cferror);
    });
}

- (void)sosCircleStatusNonCached:(void(^)(SOSCCStatus status, NSError *error))complete
{
    SOSCloudKeychainFlush(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef __unused returnedValues, CFErrorRef __unused sync_error) {
        CFErrorRef cferror = NULL;
        SOSCCStatus status = SOSCCThisDeviceIsInCircleNonCached(&cferror);
        complete(status, (__bridge NSError *)cferror);
        CFReleaseNull(cferror);
    });
}


- (void)sosViewStatus:(NSString *) viewName withCompletion: (void(^)(SOSViewResultCode status, NSError *error))complete
{
    CFErrorRef cferror = NULL;
    SOSViewResultCode status = SOSCCView((__bridge CFStringRef)(viewName), kSOSCCViewQuery, &cferror);
    complete(status, (__bridge NSError *)cferror);
    CFReleaseNull(cferror);
}


- (void)sosICKStatus: (void(^)(bool status))complete
{
    CFErrorRef cferror = NULL;
    bool status = SOSCCIsIcloudKeychainSyncing();
    complete(status);
    CFReleaseNull(cferror);
}

- (void)sosPeerID:(void (^)(NSString *))complete
{
    CFErrorRef cferror = NULL;
    CFStringRef peerID = NULL;
    SOSPeerInfoRef peerInfo = SOSCCCopyMyPeerInfo(&cferror);
    if (peerInfo)
        peerID = SOSPeerInfoGetPeerID(peerInfo);

    complete((__bridge NSString *)peerID);
    CFReleaseNull(peerInfo);
}

- (void)sosPeerSerial:(void(^)(NSString * _Nullable peerSerial))complete {
    CFErrorRef cferror = NULL;
    CFStringRef peerSerial = NULL;
    SOSPeerInfoRef peerInfo = SOSCCCopyMyPeerInfo(&cferror);
    if (peerInfo)
        peerSerial = SOSPeerInfoCopySerialNumber(peerInfo);
    complete((__bridge NSString *)peerSerial);
    CFReleaseNull(peerSerial);
    CFReleaseNull(peerInfo);
}

- (void)sosCirclePeerIDs:(void (^)(NSArray<NSString *> *))complete
{
    CFErrorRef error = NULL;
    NSArray *array = CFBridgingRelease(SOSCCCopyConcurringPeerPeerInfo(&error));
    NSMutableArray<NSString *> *peerIDs = [NSMutableArray new];

    if (array) {
        [array enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            SOSPeerInfoRef peerInfo = (__bridge SOSPeerInfoRef)obj;
            NSString *peerID = (__bridge NSString *)SOSPeerInfoGetPeerID(peerInfo);
            [peerIDs addObject:peerID];
        }];
   }
    complete(peerIDs);
}

- (void)sosRequestToJoin:(void(^)(bool success, NSString *peerID, NSError *error))complete
{
    CFErrorRef cferror = NULL;

    os_log(NULL, "[%@] sosRequestToJoin", self.name);

    SOSCCStatus status = SOSCCThisDeviceIsInCircle(&cferror);
    if (status == kSOSCCCircleAbsent) {
        cferror = CFErrorCreate(NULL, CFSTR("MDCircleAbsent"), 1, NULL);
        complete(false, NULL, (__bridge NSError *)cferror);
        CFReleaseNull(cferror);
    } else if (status == kSOSCCNotInCircle) {
        CFReleaseNull(cferror);
        NSString *peerID = NULL;
        bool result = SOSCCRequestToJoinCircle(&cferror);
        if (result) {
            SOSPeerInfoRef peerInfo = SOSCCCopyMyPeerInfo(&cferror);
            if (peerInfo) {
                peerID = (__bridge NSString *)SOSPeerInfoGetPeerID(peerInfo);
            }
            CFReleaseNull(peerInfo);
            CFReleaseNull(cferror);
        }
        complete(result, peerID, (__bridge NSError *)cferror);
        CFReleaseNull(cferror);
    } else {
        if(!cferror) {
            cferror = CFErrorCreate(NULL, CFSTR("MDGeneralJoinError"), 1, NULL);
        }
        complete(false, NULL, (__bridge NSError *)cferror);
        CFReleaseNull(cferror);
    }
}

- (void)sosLeaveCircle: (void(^)(bool success, NSError *error))complete {
    CFErrorRef cferror = NULL;
    bool retval = false;

    os_log(NULL, "[%@] sosLeaveCircle", self.name);

    SOSCCStatus status = SOSCCThisDeviceIsInCircle(&cferror);
    if(status == kSOSCCInCircle || status == kSOSCCRequestPending) {
        retval = SOSCCRemoveThisDeviceFromCircle(&cferror);
    }
    complete(retval, (__bridge NSError *) cferror);
    CFReleaseNull(cferror);
}


- (void)sosApprovePeer:(NSString *)peerID complete:(void(^)(BOOL success, NSError *error))complete
{
    CFErrorRef cferror = NULL;
    os_log(NULL, "[%@] sosApprovePeer: %@", self.name, peerID);
    NSArray *applicants = CFBridgingRelease(SOSCCCopyApplicantPeerInfo(&cferror));
    if ([applicants count] == 0) {
        CFReleaseNull(cferror);
        cferror = CFErrorCreate(NULL, CFSTR("MDNoApplicant"), 1, NULL);
        complete(false, (__bridge NSError *)cferror);
        CFReleaseNull(cferror);
        return;
    }
    NSMutableArray *approvedApplicants = [NSMutableArray array];
    for (id peer in applicants) {
        SOSPeerInfoRef peerInfo = (__bridge SOSPeerInfoRef)peer;
        NSString *applicantPeerID = (__bridge NSString *)SOSPeerInfoGetPeerID(peerInfo);
        if (peerID == NULL || [peerID isEqualToString:applicantPeerID]){
            [approvedApplicants addObject:(__bridge id)peerInfo];
        }
    }
    bool result = false;
    if ([approvedApplicants count]) {
        result = SOSCCAcceptApplicants((__bridge CFArrayRef)approvedApplicants, &cferror);
    } else {
        cferror = CFErrorCreate(NULL, CFSTR("MDNoApplicant"), 1, NULL);
    }
    complete(result, (__bridge NSError *)cferror);
    CFReleaseNull(cferror);
}

- (void)sosGhostBust:(SOSAccountGhostBustingOptions)options complete:(void(^)(bool busted, NSError *error))complete {
    os_log(NULL, "[%@] sosGhostBust", self.name);
    SOSCCGhostBust(options, ^(bool busted, NSError *error) {
        os_log(NULL, "[%@] sosGhostBust: %sbusted error: %@", self.name, busted ? "" : "no ", error);
        complete(busted, error);
    });
}

- (void)sosCircleHash: (void(^)(NSString *data, NSError * _Nullable error))complete
{
    NSError *error = NULL;
    NSString *hash = SOSCCCircleHash(&error);
    complete(hash, error);
}


- (void)sosWaitForInitialSync:(void(^)(bool success, NSError *error))complete
{
    CFErrorRef cferror = NULL;
    bool success = SOSCCWaitForInitialSync(&cferror);
    complete(success, (__bridge NSError *)cferror);
    CFReleaseNull(cferror);
}

- (void)sosEnableAllViews:(void(^)(BOOL success, NSError *error))complete
{
    CFMutableSetRef viewsToEnable = SOSViewCopyViewSet(kViewSetAll);
    CFMutableSetRef viewsToDisable = CFSetCreateMutable(NULL, 0, NULL);

    bool success = SOSCCViewSet(viewsToEnable, viewsToDisable);
    CFRelease(viewsToEnable);
    CFRelease(viewsToDisable);
    complete(success, NULL);

}

- (void) sosCachedViewBitmask: (void(^)(uint64_t bitmask))complete {
    uint64_t result =  SOSCachedViewBitmask();
    complete(result);
}

- (void) deviceInfo:(nonnull void (^)(NSString * _Nullable, NSString * _Nullable, NSError * _Nullable))complete {
    complete(@"", @"", NULL);
}


// MARK: - Pairing

- (void)pairingChannelSetup:(bool)initiator pairingContext:(KCPairingChannelContext *)context complete:(void (^)(id<DevicePairingProtocol>, NSError *))complete {

    DevicePairingSimulator *pairingSim = [[DevicePairingSimulator alloc] initAsInitiator:initiator version:context device:self];
    complete(pairingSim, nil);
}

// MARK: - Diagnostics

- (void)diagnosticsLeaks:(void(^)(bool success, NSString *outout, NSError *error))complete
{
    complete(true, NULL, NULL);
}

- (void)diagnosticsCPUUsage:(void(^)(bool success, uint64_t user_usec, uint64_t sys_usec, NSError *error))complete
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    uint64_t user_usec = usage.ru_utime.tv_sec * USEC_PER_SEC + usage.ru_utime.tv_usec;
    uint64_t sys_usec  = usage.ru_stime.tv_sec * USEC_PER_SEC + usage.ru_stime.tv_usec;

    complete(true, user_usec, sys_usec, NULL);
}

- (void)diagnosticsDiskUsage:(void(^)(bool success, uint64_t usage, NSError *error))complete
{
    rusage_info_current rusage;

    if (proc_pid_rusage(getpid(), RUSAGE_INFO_CURRENT, (rusage_info_t *)&rusage) == 0) {
        complete(true, rusage.ri_logical_writes, NULL);
    } else {
        complete(false, 0, NULL);
    }
}

// MARK: - Octagon
- (void)otReset:(NSString *)altDSID complete:(void (^)(bool success, NSError *_Nullable error))complete
{
#if OCTAGON
    OTControl *ot = [self OTControl];

    [ot resetAndEstablish:nil context:OTDefaultContext altDSID:altDSID resetReason:CuttlefishResetReasonTestGenerated reply:^(NSError * _Nullable error) {
        complete(error == NULL, error);
    }];
#else
    complete(false, [self octagonNotAvailableError]);
#endif
}

- (void)otPeerID:(NSString *)altDSID complete:(void (^)(NSString *peerID,  NSError *_Nullable error))complete
{
#if OCTAGON
    OTControl *ot = [self OTControl];
    [ot fetchEgoPeerID:nil context:OTDefaultContext reply:^(NSString * _Nullable peerID, NSError * _Nullable error) {
        complete(peerID, error);
    }];
#else
    complete(false, [self octagonNotAvailableError]);
#endif

}

- (void)otInCircle:(NSString *)altDSID complete:(void (^)(bool inCircle,  NSError *_Nullable error))complete
{
#if OCTAGON
    OTControl *ot = [self OTControl];
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];
    [ot fetchCliqueStatus:nil context:OTDefaultContext configuration:configuration reply:^(CliqueStatus cliqueStatus, NSError * _Nullable error) {
        os_log(NULL, "[%@] otInCircle: clique: %d error: %@", self.name, (int)cliqueStatus, error);
        complete(cliqueStatus == CliqueStatusIn, error);
    }];
#else
    complete(false, [self octagonNotAvailableError]);
#endif
}


//MARK: - Misc helpers

#if OCTAGON

- (OTControl *)OTControl
{
#if SECD_SERVER
    return [[OTControl alloc] initWithConnection:(NSXPCConnection *)[[NSXPCConnectionMock alloc] initWithRealObject:[OTManager manager]] sync:true];
#else
    NSError *error = NULL;
    return [OTControl controlObject:true error:&error];
#endif
}

#else /* !OCTAGON */

- (NSError *)octagonNotAvailableError
{
    return [NSError errorWithDomain:@"DeviceSimulator" code:1 description:@"no octagon available"];
}
#endif

@end
