#ifndef FakeSOSControl_h
#define FakeSOSControl_h

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecKeyPriv.h>
#import <Security/SecItemPriv.h>
#import "keychain/SecureObjectSync/SOSAccount.h"
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSCircle.h"
#import <KeychainCircle/KeychainCircle.h>
#import "utilities/SecCFWrappers.h"

NS_ASSUME_NONNULL_BEGIN

@interface FCPairingFakeSOSControl : NSObject <SOSControlProtocol>
@property (assign) SecKeyRef accountPrivateKey;
@property (assign) SecKeyRef accountPublicKey;
@property (assign) SecKeyRef deviceKey;
@property (assign) SecKeyRef octagonSigningKey;
@property (assign) SecKeyRef octagonEncryptionKey;
@property (assign) SOSCircleRef circle;
@property (assign) SOSFullPeerInfoRef fullPeerInfo;
@property (assign) bool application;
- (instancetype)initWithRandomAccountKey:(bool)randomAccountKey circle:(SOSCircleRef)circle;
- (void)dealloc;
- (SOSPeerInfoRef)peerInfo;
- (void)signApplicationIfNeeded;
@end

@interface FakeNSXPCConnection : NSObject
- (instancetype) initWithControl:(id<SOSControlProtocol>)control;
- (id)remoteObjectProxyWithErrorHandler:(void(^)(NSError * _Nonnull error))failureHandler;
@end

@interface FakeNSXPCConnection ()
@property id<SOSControlProtocol> control;
@end

NS_ASSUME_NONNULL_END
#endif /* FakeSOSControl_h */


