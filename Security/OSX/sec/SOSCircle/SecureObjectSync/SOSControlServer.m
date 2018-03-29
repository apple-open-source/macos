#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/SecEntitlements.h>
#import <ipc/securityd_client.h>
#import "SOSAccount.h"
#import "SOSControlHelper.h"
#import "SOSControlServer.h"

@interface SOSControlServer : NSObject <NSXPCListenerDelegate>
@end

@interface SOSClient : NSObject <SOSControlProtocol>
@property (weak) NSXPCConnection * connection;
@property (strong) SOSAccount * account;

- (instancetype)initWithConnection:(NSXPCConnection *)connection account:(SOSAccount *)account;
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


    SOSClient *sosClient = [[SOSClient alloc] initWithConnection:newConnection account:(__bridge SOSAccount *)SOSKeychainAccountGetSharedAccount()];

    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(SOSControlProtocol)];
    _SOSControlSetupInterface(newConnection.exportedInterface);
    newConnection.exportedObject = sosClient;

    [newConnection resume];

    return YES;
}

@end

@implementation SOSClient

@synthesize account = _account;
@synthesize connection = _connection;

- (instancetype)initWithConnection:(NSXPCConnection *)connection account:(SOSAccount *)account
{
    if ((self = [super init])) {
        _connection = connection;
        _account = account;
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

- (void)userPublicKey:(void ((^))(BOOL trusted, NSData *spki, NSError *error))reply
{
    [self.account userPublicKey:reply];
}

- (void)kvsPerformanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *))reply
{
    [self.account kvsPerformanceCounters:reply];
}

- (void)idsPerformanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *))reply
{
    [self.account idsPerformanceCounters:reply];
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

- (void)triggerSync:(NSArray <NSString *> *)peers complete:(void(^)(bool success, NSError *))complete
{
    if (![self checkEntitlement:(__bridge NSString *)kSecEntitlementKeychainCloudCircle]) {
        complete(false, [NSError errorWithDomain:(__bridge NSString *)kSOSErrorDomain code:kSOSEntitlementMissing userInfo:NULL]);
        return;
    }

    [self.account triggerSync:peers complete:complete];
}

- (void)getWatchdogParameters:(void (^)(NSDictionary* parameters, NSError* error))complete
{
    [self.account getWatchdogParameters:complete];
}

- (void)setWatchdogParmeters:(NSDictionary*)parameters complete:(void (^)(NSError* error))complete
{
    [self.account setWatchdogParmeters:parameters complete:complete];
}

@end

void
SOSControlServerInitialize(void)
{
    static dispatch_once_t once;
    static SOSControlServer *server;
    static NSXPCListener *listener;

    dispatch_once(&once, ^{
        @autoreleasepool {
            server = [SOSControlServer new];

            listener = [[NSXPCListener alloc] initWithMachServiceName:@(kSecuritydSOSServiceName)];
            listener.delegate = server;
            [listener resume];
        }
    });
}
