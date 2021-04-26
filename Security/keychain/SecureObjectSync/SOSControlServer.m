#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/SecEntitlements.h>
#import <ipc/securityd_client.h>
#import "SOSAccount.h"
#import "SOSControlHelper.h"
#import "SOSControlServer.h"

@interface SOSControlServer : NSObject <NSXPCListenerDelegate>
@end

@interface SOSClient ()
@property (strong) SOSAccount * account;
- (instancetype)initSOSClientWithAccount:(SOSAccount *)account;
- (bool)checkEntitlement:(NSString *)entitlement;
@end

@interface SOSClientRemote : SOSClient
@property (weak) NSXPCConnection * connection;
- (instancetype)initSOSConnectionWithConnection:(NSXPCConnection *)connection account:(SOSAccount *)account;
@end

@implementation SOSControlServer

- (BOOL)listener:(__unused NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    NSNumber *num = [newConnection valueForEntitlement:(__bridge NSString *)kSecEntitlementKeychainCloudCircle];
    if (![num isKindOfClass:[NSNumber class]] || ![num boolValue]) {
        secerror("sos: Client pid: %d doesn't have entitlement: %@",
                [newConnection processIdentifier], kSecEntitlementKeychainCloudCircle);
        return NO;
    }

    SOSAccount *account = (__bridge SOSAccount *)SOSKeychainAccountGetSharedAccount();
    if (account == nil) {
        secerror("sos: SOS have not launched yet, come later, pid: %d",
                [newConnection processIdentifier]);
        return NO;
    }

    SOSClientRemote *sosClient = [[SOSClientRemote alloc] initSOSConnectionWithConnection:newConnection account:account];

    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(SOSControlProtocol)];
    _SOSControlSetupInterface(newConnection.exportedInterface);
    newConnection.exportedObject = sosClient;

    [newConnection resume];

    return YES;
}

- (SOSClient *)internalSOSClient
{
    return [[SOSClient alloc] initSOSClientWithAccount:(__bridge SOSAccount *)SOSKeychainAccountGetSharedAccount()];
}

@end

@implementation SOSClient

@synthesize account = _account;

- (instancetype)initSOSClientWithAccount:(SOSAccount *)account
{
    if ((self = [super init])) {
        _account = account;
    }
    return self;
}

- (bool)checkEntitlement:(NSString *)entitlement
{
    return true;
}

- (void)userPublicKey:(void ((^))(BOOL trusted, NSData *spki, NSError *error))reply
{
    [self.account userPublicKey:reply];
}

- (void)kvsPerformanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *))reply
{
    [self.account kvsPerformanceCounters:reply];
}

- (void)rateLimitingPerformanceCounters:(void(^)(NSDictionary <NSString *, NSString *> *))reply
{
    [self.account rateLimitingPerformanceCounters:reply];
}

- (void)stashedCredentialPublicKey:(void(^)(NSData *, NSError *error))reply
{
    [self.account stashedCredentialPublicKey:reply];
}

- (void)assertStashedAccountCredential:(void(^)(BOOL result, NSError *error))reply
{
    [self.account assertStashedAccountCredential:reply];
}

- (void)validatedStashedAccountCredential:(void(^)(NSData *credential, NSError *error))complete
{
    [self.account validatedStashedAccountCredential:complete];
}

- (void)stashAccountCredential:(NSData *)credential complete:(void(^)(bool success, NSError *error))complete
{
    [self.account stashAccountCredential:credential complete:complete];
}

- (void)myPeerInfo:(void (^)(NSData *, NSError *))complete
{
    [self.account myPeerInfo:complete];
}

- (void)circleHash:(void (^)(NSString *, NSError *))complete
{
    [self.account circleHash:complete];
}


- (void)circleJoiningBlob:(NSData *)applicant complete:(void (^)(NSData *blob, NSError *))complete
{
    [self.account circleJoiningBlob:applicant complete:complete];
}

- (void)joinCircleWithBlob:(NSData *)blob version:(PiggyBackProtocolVersion)version complete:(void (^)(bool success, NSError *))complete
{
    [self.account joinCircleWithBlob:blob version:version complete:complete];
}

- (void)initialSyncCredentials:(uint32_t)flags complete:(void (^)(NSArray *, NSError *))complete
{
    if (![self checkEntitlement:(__bridge NSString *)kSecEntitlementKeychainInitialSync]) {
        complete(@[], [NSError errorWithDomain:(__bridge NSString *)kSOSErrorDomain code:kSOSEntitlementMissing userInfo:NULL]);
        return;
    }

    [self.account initialSyncCredentials:flags complete:complete];
}

- (void)importInitialSyncCredentials:(NSArray *)items complete:(void (^)(bool success, NSError *))complete
{
    if (![self checkEntitlement:(__bridge NSString *)kSecEntitlementKeychainInitialSync]) {
        complete(false, [NSError errorWithDomain:(__bridge NSString *)kSOSErrorDomain code:kSOSEntitlementMissing userInfo:NULL]);
        return;
    }

    [self.account importInitialSyncCredentials:items complete:complete];
}

- (void)rpcTriggerSync:(NSArray <NSString *> *)peers complete:(void(^)(bool success, NSError *))complete
{
    [self.account rpcTriggerSync:peers complete:complete];
}

- (void)getWatchdogParameters:(void (^)(NSDictionary* parameters, NSError* error))complete
{
    [self.account getWatchdogParameters:complete];
}

- (void)setWatchdogParmeters:(NSDictionary*)parameters complete:(void (^)(NSError* error))complete
{
    [self.account setWatchdogParmeters:parameters complete:complete];
}

- (void) ghostBust:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool ghostBusted, NSError *error))complete {
    [self.account ghostBust:options complete:complete];
}

- (void)ghostBustTriggerTimed:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool ghostBusted, NSError *error))complete {
    [self.account ghostBustTriggerTimed:options complete:complete];
}

- (void) ghostBustPeriodic:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool busted, NSError *error))complete {
    [self.account ghostBustPeriodic:options complete:complete];
}

- (void) ghostBustInfo: (void(^)(NSData *json, NSError *error))complete {
    [self.account ghostBustInfo:complete];
}

- (void)iCloudIdentityStatus: (void (^)(NSData *json, NSError *error))complete {
    [self.account iCloudIdentityStatus: complete];
}

- (void)rpcTriggerBackup:(NSArray<NSString *>* _Nullable)backupPeers complete:(void (^)(NSError *error))complete
{
    [self.account rpcTriggerBackup:backupPeers complete:complete];
}

- (void)rpcTriggerRingUpdate:(void (^)(NSError *))complete {
    [self.account rpcTriggerRingUpdate:complete];
}

- (void)iCloudIdentityStatus_internal:(void (^)(NSDictionary *, NSError *))complete {
    [self.account iCloudIdentityStatus_internal:complete];
}

- (void)removeV0Peers:(void (^)(bool, NSError *))reply { 
    [self.account removeV0Peers:reply];
}



@end

@implementation SOSClientRemote

- (instancetype)initSOSConnectionWithConnection:(NSXPCConnection *)connection account:(SOSAccount *)account
{
    if ((self = [super initSOSClientWithAccount:account])) {
        self.connection = connection;
    }
    return self;
}

- (bool)checkEntitlement:(NSString *)entitlement
{
    NSXPCConnection *strongConnection = _connection;

    NSNumber *num = [strongConnection valueForEntitlement:entitlement];
    if (![num isKindOfClass:[NSNumber class]] || ![num boolValue]) {
        secerror("sos: Client pid: %d doesn't have entitlement: %@",
                 [strongConnection processIdentifier], entitlement);
        return false;
    }
    return true;
}
@end

static SOSControlServer *sosServer;

void
SOSControlServerInitialize(void)
{
    static dispatch_once_t once;
    static NSXPCListener *listener;

    dispatch_once(&once, ^{
        @autoreleasepool {
            sosServer = [SOSControlServer new];

            listener = [[NSXPCListener alloc] initWithMachServiceName:@(kSecuritydSOSServiceName)];
            listener.delegate = sosServer;
            [listener resume];
        }
    });
}

SOSClient *
SOSControlServerInternalClient(void)
{
    SOSControlServerInitialize();
    return [sosServer internalSOSClient];
}
