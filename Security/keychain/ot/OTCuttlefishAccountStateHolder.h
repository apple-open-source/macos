
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/ot/proto/generated_source/OTAccountMetadataClassCAccountSettings.h"
#import "keychain/ot/OTPersonaAdapter.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"

@class OTAccountSettings;

extern NSString* _Nonnull OTCuttlefishContextErrorDomain;
typedef NS_ERROR_ENUM(OTCuttlefishContextErrorDomain, OTCuttlefishContextErrors) {
    OTCCNoExistingPeerID = 0,
    OTCCNoAccountSettings = 1,
};

NS_ASSUME_NONNULL_BEGIN

@protocol OTCuttlefishAccountStateHolderNotifier
- (void)accountStateUpdated:(OTAccountMetadataClassC*)newState from:(OTAccountMetadataClassC*)oldState;
@end

@interface OTCuttlefishAccountStateHolder : NSObject

// If you already know you're on this queue, call the _onqueue versions below.
- (instancetype)initWithQueue:(dispatch_queue_t)queue
                    container:(NSString*)containerName
                      context:(NSString*)contextID
               personaAdapter:(id<OTPersonaAdapter>)personaAdapter
                activeAccount:(TPSpecificUser* _Nullable)activeAccount;

- (void)changeActiveAccount:(TPSpecificUser*)newActiveAccount;

- (OTAccountMetadataClassC* _Nullable)loadOrCreateAccountMetadata:(NSError**)error;
- (OTAccountMetadataClassC* _Nullable)_onqueueLoadOrCreateAccountMetadata:(NSError**)error;

- (void)registerNotification:(id<OTCuttlefishAccountStateHolderNotifier>)notifier;

- (BOOL)persistNewEgoPeerID:(NSString*)peerID error:(NSError**)error;
- (NSString * _Nullable)getEgoPeerID:(NSError **)error;

- (BOOL)persistNewTrustState:(OTAccountMetadataClassC_TrustState)newState
                       error:(NSError**)error;

- (BOOL)persistAccountChanges:(OTAccountMetadataClassC* _Nullable (^)(OTAccountMetadataClassC* metadata))makeChanges
                        error:(NSError**)error;

- (BOOL)_onqueuePersistAccountChanges:(OTAccountMetadataClassC* _Nullable (^)(OTAccountMetadataClassC* metadata))makeChanges
                                error:(NSError**)error;

- (NSDate* _Nullable)lastHealthCheckupDate:(NSError * _Nullable *)error;
- (BOOL)persistLastHealthCheck:(NSDate*)lastCheck error:(NSError**)error;

- (BOOL)persistOctagonJoinAttempt:(OTAccountMetadataClassC_AttemptedAJoinState)attempt error:(NSError**)error;

- (OTAccountMetadataClassC_MetricsState)fetchSendingMetricsPermitted:(NSError**)error;
- (BOOL)persistSendingMetricsPermitted:(OTAccountMetadataClassC_MetricsState)sendingMetricsPermitted
                                 error:(NSError**)error;
@end

NS_ASSUME_NONNULL_END
