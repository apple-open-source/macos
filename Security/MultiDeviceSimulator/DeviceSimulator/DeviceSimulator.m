//
//  DeviceSimulator.m
//  DeviceSimulator
//

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <securityd/SOSCloudCircleServer.h>
#import <Security/SecureObjectSync/SOSPeerInfo.h>
#import <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#import <Security/SecureObjectSync/SOSViews.h>
#import <Security/SecureObjectSync/SOSInternal.h>

#import <stdlib.h>
#import <unistd.h>
#import <libproc.h>

#import "keychain/ckks/CKKS.h"
#import "SOSCloudKeychainClient.h"

#import "DeviceSimulatorProtocol.h"
#import "DeviceSimulator.h"


@implementation DeviceSimulator

- (void)setDevice:(NSString *)name
           version:(NSString *)version
             model:(NSString *)model
     testInstance:(NSString *)testUUID
           network:(NSXPCListenerEndpoint *)network
          complete:(void(^)(BOOL success))complete
{
    self.name = name;

    SecCKKSDisable(); // for now
    SecCKKSContainerName = [NSString stringWithFormat:@"com.apple.test.p01.B.%@.com.apple.security.keychain", testUUID];

    SOSCCSetGestalt_Server((__bridge CFStringRef)name, (__bridge CFStringRef)version,
                           (__bridge CFStringRef)model, (__bridge CFStringRef)deviceInstance);

    boot_securityd(network);

    complete(TRUE);
}


- (void)secItemAdd:(NSDictionary *)input complete:(void (^)(OSStatus, NSDictionary *))reply
{
    NSMutableDictionary *attributes = [input mutableCopy];
    CFTypeRef data = NULL;

    attributes[(__bridge NSString *)kSecReturnAttributes] = @YES;
    attributes[(__bridge NSString *)kSecReturnPersistentRef] = @YES;
    attributes[(__bridge NSString *)kSecReturnData] = @YES;

    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)attributes, &data);
    NSDictionary *returnData = CFBridgingRelease(data);
    
    reply(status, returnData);
}

- (void)secItemCopyMatching:(NSDictionary *)input complete:(void (^)(OSStatus, NSArray<NSDictionary *>*))reply
{
    NSMutableDictionary *attributes = [input mutableCopy];
    CFTypeRef data = NULL;

    attributes[(__bridge NSString *)kSecReturnAttributes] = @YES;
    attributes[(__bridge NSString *)kSecReturnData] = @YES;
    attributes[(__bridge NSString *)kSecReturnPersistentRef] = @YES;
    attributes[(__bridge NSString *)kSecMatchLimit] = (__bridge id)kSecMatchLimitAll;

    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)attributes, &data);
    NSArray<NSDictionary *>* array = CFBridgingRelease(data);
    NSMutableArray *result = [NSMutableArray array];
    for (NSDictionary *d in array) {
        NSMutableDictionary *r = [d mutableCopy];
        r[@"accc"] = nil;
        [result addObject:r];
    }

    reply(status, result);
}

- (void)setupSOSCircle:(NSString *)username password:(NSString *)password complete:(void (^)(bool success, NSError *error))complete
{
    CFErrorRef cferror = NULL;
    bool result = SOSCCSetUserCredentialsAndDSID((__bridge CFStringRef)username,
                                                 (__bridge CFDataRef)[password dataUsingEncoding:NSUTF8StringEncoding],
                                                 CFSTR("1"), &cferror);
    if (result) {
        SOSCCStatus circleStat = SOSCCThisDeviceIsInCircle(&cferror);
        if (circleStat == kSOSCCCircleAbsent) {
            result = SOSCCResetToOffering(&cferror);
        }
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



//PRAGMA mark: - Diagnostics

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

@end
