//
//  DeviceSimulatorProtocol.h
//  DeviceSimulator
//

#import <Foundation/Foundation.h>
#import <Security/SecureObjectSync/SOSCloudCircle.h>

@protocol DeviceSimulatorProtocol

- (void)setDevice:(NSString *)name
          version:(NSString *)version
            model:(NSString *)model
     testInstance:(NSString *)testUUID
          network:(NSXPCListenerEndpoint *)network
         complete:(void(^)(BOOL success))complete;

// Local Keychain
- (void)secItemAdd:(NSDictionary *)input complete:(void (^)(OSStatus, NSDictionary *))reply;
- (void)secItemCopyMatching:(NSDictionary *)input complete:(void (^)(OSStatus, NSArray<NSDictionary *>*))replyreply;

// SOS trust
- (void)setupSOSCircle:(NSString *)username password:(NSString *)password complete:(void (^)(bool success,  NSError *error))complete;
- (void)sosCircleStatus:(void(^)(SOSCCStatus status, NSError *error))complete;
- (void)sosCircleStatusNonCached:(void(^)(SOSCCStatus status, NSError *error))complete;
- (void)sosViewStatus:(NSString *) view withCompletion: (void(^)(SOSViewResultCode status, NSError *error))complete;
- (void)sosICKStatus: (void(^)(bool status))complete;
- (void)sosCachedViewBitmask: (void(^)(uint64_t bitmask))complete;
- (void)sosPeerID:(void(^)(NSString *peerID))complete;
- (void)sosRequestToJoin:(void(^)(bool success, NSString *peerID, NSError *error))complete;
- (void)sosLeaveCircle: (void(^)(bool success, NSError *error))complete;
- (void)sosApprovePeer:(NSString *)peerID complete:(void(^)(BOOL success, NSError *error))complete;

// SOS syncing
- (void)sosWaitForInitialSync:(void(^)(bool success, NSError *error))complete;
- (void)sosEnableAllViews:(void(^)(BOOL success, NSError *error))complete;

// Diagnostics
- (void)diagnosticsLeaks:(void(^)(bool success, NSString *outout, NSError *error))complete;
- (void)diagnosticsCPUUsage:(void(^)(bool success, uint64_t user_usec, uint64_t sys_usec, NSError *error))complete;
- (void)diagnosticsDiskUsage:(void(^)(bool success, uint64_t usage, NSError *error))complete;

@end

