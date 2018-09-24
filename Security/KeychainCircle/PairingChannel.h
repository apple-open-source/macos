//
//  SecurityPairing.h
//  Security
//

#import <Foundation/Foundation.h>
#import <Security/SecureObjectSync/SOSTypes.h>

extern NSString *kKCPairingChannelErrorDomain;

#define KCPairingErrorNoControlChannel          1
#define KCPairingErrorTooManySteps              2
#define KCPairingErrorAccountCredentialMissing  3

typedef void(^KCPairingChannelCompletion)(BOOL complete, NSData *packet, NSError *error);

@interface KCPairingChannelContext : NSObject
@property (strong) NSString *model;
@property (strong) NSString *modelVersion;
@property (strong) NSString *modelClass;
@property (strong) NSString *osVersion;
@end

@interface KCPairingChannel : NSObject

@property (assign,readonly) BOOL needInitialSync;

+ (instancetype)pairingChannelInitiator:(KCPairingChannelContext *)peerVersionContext;
+ (instancetype)pairingChannelAcceptor:(KCPairingChannelContext *)peerVersionContext;

- (instancetype)initAsInitiator:(bool)initator version:(KCPairingChannelContext *)peerVersionContext;
- (void)validateStart:(void(^)(bool result, NSError *error))complete;

- (NSData *)exchangePacket:(NSData *)data complete:(bool *)complete error:(NSError **)error;

/* for tests cases only */
- (void)setXPCConnectionObject:(NSXPCConnection *)connection;
+ (bool)isSupportedPlatform;
@end

