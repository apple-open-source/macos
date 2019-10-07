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
#define KCPairingErrorOctagonMessageMissing     4
#define KCPairingErrorTypeConfusion             5
#define KCPairingErrorNoTrustAvailable          6
#define KCPairingErrorNoStashedCredential       7

typedef void(^KCPairingChannelCompletion)(BOOL complete, NSData *packet, NSError *error);

typedef NSString* KCPairingIntent_Type NS_STRING_ENUM;
extern KCPairingIntent_Type KCPairingIntent_Type_None;
extern KCPairingIntent_Type KCPairingIntent_Type_SilentRepair;
extern KCPairingIntent_Type KCPairingIntent_Type_UserDriven;

@interface KCPairingChannelContext : NSObject <NSSecureCoding>
@property (strong) NSString *model;
@property (strong) NSString *modelVersion;
@property (strong) NSString *modelClass;
@property (strong) NSString *osVersion;
@property (strong) NSString *uniqueDeviceID;
@property (strong) NSString *uniqueClientID;
@property (strong) KCPairingIntent_Type intent;
@end

/**
 * Pairing channel provides the channel used in OOBE / D2D and HomePod/AppleTV (TapToFix) setup.
 *
 * The initiator is the device that wants to get into the circle, the acceptor is the device that is already in.
 * The interface require the caller to hold a lock assertion over the whole transaction, both on the initiator and acceptor.
*/

@interface KCPairingChannel : NSObject

@property (assign,readonly) BOOL needInitialSync;

+ (instancetype)pairingChannelInitiator:(KCPairingChannelContext *)peerVersionContext;
+ (instancetype)pairingChannelAcceptor:(KCPairingChannelContext *)peerVersionContext;

- (instancetype)initAsInitiator:(bool)initiator version:(KCPairingChannelContext *)peerVersionContext;
- (void)validateStart:(void(^)(bool result, NSError *error))complete;

- (NSData *)exchangePacket:(NSData *)data complete:(bool *)complete error:(NSError **)error;
- (void)exchangePacket:(NSData *)inputCompressedData complete:(KCPairingChannelCompletion)complete; /* async version of above */

/* for tests cases only */
- (void)setXPCConnectionObject:(NSXPCConnection *)connection;
- (void)setControlObject:(id)control;
- (void)setConfiguration:(id)config;
- (void)setSOSMessageFailForTesting:(BOOL)value;
- (void)setOctagonMessageFailForTesting:(BOOL)value;
+ (bool)isSupportedPlatform;
- (void)setSessionSupportsOctagonForTesting:(bool)value;
@end

