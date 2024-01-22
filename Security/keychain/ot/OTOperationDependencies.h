
#import <Foundation/Foundation.h>

#import "keychain/ot/CuttlefishXPCWrapper.h"
#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OTAuthKitAdapter.h"
#import "keychain/ot/OTAccountsAdapter.h"
#import "keychain/ot/OTPersonaAdapter.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTDeviceInformationAdapter.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import <Security/SecEscrowRequest.h>

NS_ASSUME_NONNULL_BEGIN

// Used for dependency injection into most OctagonStateTransition operations
@interface OTOperationDependencies : NSObject

@property NSString* containerName;
@property NSString* contextID;

@property (nullable) TPSpecificUser* activeAccount;

@property OTCuttlefishAccountStateHolder* stateHolder;

@property id<OctagonStateFlagHandler> flagHandler;
@property id<OTSOSAdapter> sosAdapter;
@property (nullable) id<CKKSPeerProvider> octagonAdapter;
@property id<OTAccountsAdapter> accountsAdapter;
@property id<OTAuthKitAdapter> authKitAdapter;
@property id<OTPersonaAdapter> personaAdapter;
@property id<OTDeviceInformationAdapter> deviceInformationAdapter;
@property (readonly) CuttlefishXPCWrapper* cuttlefishXPCWrapper;
@property (readonly, weak) CKKSKeychainView* ckks;

@property CKKSLockStateTracker* lockStateTracker;
@property Class<SecEscrowRequestable> escrowRequestClass;
@property Class<CKKSNotifier> notifierClass;

@property (nullable, strong) NSString* flowID;
@property (nullable, strong) NSString* deviceSessionID;
@property (nonatomic) BOOL permittedToSendMetrics;

- (instancetype)initForContainer:(NSString*)containerName
                       contextID:(NSString*)contextID
                   activeAccount:(TPSpecificUser* _Nullable)activeAccount
                     stateHolder:(OTCuttlefishAccountStateHolder*)stateHolder
                     flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                      sosAdapter:(id<OTSOSAdapter>)sosAdapter
                  octagonAdapter:(id<CKKSPeerProvider> _Nullable)octagonAdapter
                 accountsAdapter:(id<OTAccountsAdapter>)accountsAdapter
                  authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                  personaAdapter:(id<OTPersonaAdapter>)personaAdapter
               deviceInfoAdapter:(id<OTDeviceInformationAdapter>)deviceInfoAdapter
                 ckksAccountSync:(CKKSKeychainView* _Nullable)ckks
                lockStateTracker:(CKKSLockStateTracker *)lockStateTracker
            cuttlefishXPCWrapper:(CuttlefishXPCWrapper *)cuttlefishXPCWrapper
              escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                   notifierClass:(Class<CKKSNotifier>)notifierClass
                          flowID:(NSString* _Nullable)flowID
                 deviceSessionID:(NSString* _Nullable)deviceSessionID
          permittedToSendMetrics:(BOOL)permittedToSendMetrics;

@end

NS_ASSUME_NONNULL_END
