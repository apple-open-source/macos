
#import <Foundation/Foundation.h>

#import "keychain/ot/OTControl.h"

#import <AppleFeatures/AppleFeatures.h>

NS_ASSUME_NONNULL_BEGIN


@interface OTControlCLI : NSObject
@property OTControl* control;

- (instancetype)initWithOTControl:(OTControl*)control;

- (int)startOctagonStateMachine:(OTControlArguments*)arguments;

- (int)signIn:(OTControlArguments*)arguments;

- (int)signOut:(OTControlArguments*)arguments;

- (int)depart:(OTControlArguments*)arguments;

- (int)resetOctagon:(OTControlArguments*)arguments
  idmsTargetContext:(NSString*_Nullable)idmsTargetContext
idmsCuttlefishPassword:(NSString*_Nullable)idmsCuttlefishPassword
         notifyIdMS:(bool)notifyIdMS
            timeout:(NSTimeInterval)timeout;


- (int)resetProtectedData:(OTControlArguments*)arguments
                  appleID:(NSString *_Nullable)appleID
                     dsid:(NSString *_Nullable)dsid
        idmsTargetContext:(NSString *_Nullable)idmsTargetContext
   idmsCuttlefishPassword:(NSString *_Nullable)idmsCuttlefishPassword
               notifyIdMS:(bool)notifyIdMS;

- (int)reset:(OTControlArguments*)arguments
     appleID:(NSString * _Nullable)appleID
        dsid:(NSString *_Nullable)dsid;

- (int)status:(OTControlArguments*)arguments json:(bool)json;

- (int)recoverUsingBottleID:(NSString *)bottleID
                    entropy:(NSData*)entropy
                  arguments:(OTControlArguments*)arguments
                    control:(OTControl*)control;

- (int)fetchAllBottles:(OTControlArguments*)arguments
               control:(OTControl *)control
   overrideEscrowCache:(BOOL)overrideEscrowCache;

- (int)fetchEscrowRecords:(OTControlArguments*)arguments
                     json:(bool)json
      overrideEscrowCache:(BOOL)overrideEscrowCache;

- (int)fetchAllEscrowRecords:(OTControlArguments*)arguments
                        json:(bool)json
         overrideEscrowCache:(BOOL)overrideEscrowCache;

- (int)healthCheck:(OTControlArguments*)arguments
       skipRateLimitingCheck:(BOOL)skipRateLimitingCheck
                      repair:(BOOL)repair
                        json:(BOOL)json;

- (int)simulateReceivePush:(OTControlArguments*)arguments
                      json:(BOOL)json;

- (int)refetchCKKSPolicy:(OTControlArguments*)arguments;

- (int)tapToRadar:(NSString *)action description:(NSString *)description radar:(NSString *)radar;

- (int)performEscrowRecovery:(OTControlArguments*)arguments
                    recordID:(NSString *)recordID
                     appleID:(NSString *)appleID
                      secret:(NSString *)secret
    overrideForAccountScript:(BOOL)overrideForAccountScript
         overrideEscrowCache:(BOOL)overrideEscrowCache;

- (int)performSilentEscrowRecovery:(OTControlArguments*)arguments
                           appleID:(NSString *)appleID secret:(NSString *)secret;

- (int)tlkRecoverability:(OTControlArguments*)arguments;

- (int)setUserControllableViewsSyncStatus:(OTControlArguments*)arguments
                                  enabled:(BOOL)enabled;

- (int)fetchUserControllableViewsSyncStatus:(OTControlArguments*)arguments;

- (int)resetAccountCDPContentsWithArguments:(OTControlArguments*)argumentsName
idmsTargetContext:(NSString*_Nullable)idmsTargetContextString idmsCuttlefishPassword:(NSString*_Nullable)idmsCuttlefishPassword notifyIdMS:(bool)notifyIdMS ;

- (int)createCustodianRecoveryKeyWithArguments:(OTControlArguments*)argumentsName
                                    uuidString:(NSString*_Nullable)uuidString
                                          json:(bool)json
                                       timeout:(NSTimeInterval)timeout;

- (int)joinWithCustodianRecoveryKeyWithArguments:(OTControlArguments*)argumentsName
                                     wrappingKey:(NSString*)wrappingKey
                                      wrappedKey:(NSString*)wrappedKey
                                      uuidString:(NSString*)uuidString
                                         timeout:(NSTimeInterval)timeout;

- (int)preflightJoinWithCustodianRecoveryKeyWithArguments:(OTControlArguments*)argumentsName
                                              wrappingKey:(NSString*)wrappingKey
                                               wrappedKey:(NSString*)wrappedKey
                                               uuidString:(NSString*)uuidString
                                                  timeout:(NSTimeInterval)timeout;

- (int)removeCustodianRecoveryKeyWithArguments:(OTControlArguments*)argumentsName
                                    uuidString:(NSString*)uuidString
                                       timeout:(NSTimeInterval)timeout;

- (int)checkCustodianRecoveryKeyWithArguments:(OTControlArguments*)argumentsName
                                   uuidString:(NSString*)uuidString
                                      timeout:(NSTimeInterval)timeout;

- (int)removeRecoveryKeyWithArguments:(OTControlArguments*)arguments;

- (int)setRecoveryKeyWithArguments:(OTControlArguments*)arguments;

- (int)joinWithRecoveryKeyWithArguments:(OTControlArguments*)arguments recoveryKey:(NSString*)recoveryKey;

- (int)checkRecoveryKeyWithArguments:(OTControlArguments*)arguments;

- (int)preflightJoinWithRecoveryKeyWithArguments:(OTControlArguments*)arguments recoveryKey:(NSString*)recoveryKey;

- (int)createInheritanceKeyWithArguments:(OTControlArguments*)argumentsName
                              uuidString:(NSString*_Nullable)uuidString
                                    json:(bool)json
                                 timeout:(NSTimeInterval)timeout;

- (int)generateInheritanceKeyWithArguments:(OTControlArguments*)argumentsName
                                      json:(bool)json
                                   timeout:(NSTimeInterval)timeout;

- (int)storeInheritanceKeyWithArguments:(OTControlArguments*)argumentsName
                            wrappingKey:(NSString*)wrappingKey
                             wrappedKey:(NSString*)wrappedKey
                             uuidString:(NSString*)uuidString
                                timeout:(NSTimeInterval)timeout;

- (int)joinWithInheritanceKeyWithArguments:(OTControlArguments*)argumentsName
                               wrappingKey:(NSString*)wrappingKey
                                wrappedKey:(NSString*)wrappedKey
                                uuidString:(NSString*)uuidString
                                   timeout:(NSTimeInterval)timeout;

- (int)preflightJoinWithInheritanceKeyWithArguments:(OTControlArguments*)argumentsName
                                        wrappingKey:(NSString*)wrappingKey
                                         wrappedKey:(NSString*)wrappedKey
                                         uuidString:(NSString*)uuidString
                                            timeout:(NSTimeInterval)timeout;

- (int)removeInheritanceKeyWithArguments:(OTControlArguments*)argumentsName
                              uuidString:(NSString*)uuidString
                                 timeout:(NSTimeInterval)timeout;

- (int)checkInheritanceKeyWithArguments:(OTControlArguments*)argumentsName
                             uuidString:(NSString*)uuidString
                                timeout:(NSTimeInterval)timeout;

- (int)recreateInheritanceKeyWithArguments:(OTControlArguments*)argumentsName
                                uuidString:(NSString*_Nullable)uuidString
                               wrappingKey:(NSString*)wrappingKey
                                wrappedKey:(NSString*)wrappedKey
                                claimToken:(NSString*)claimToken
                                      json:(bool)json
                                   timeout:(NSTimeInterval)timeout;

- (int)createInheritanceKeyWithClaimTokenAndWrappingKey:(OTControlArguments*)argumentsName
                                             uuidString:(NSString*_Nullable)uuidString
                                             claimToken:(NSString*)claimToken
                                            wrappingKey:(NSString*)wrappingKey
                                                   json:(bool)json
                                                timeout:(NSTimeInterval)timeout;

- (int)setMachineIDOverride:(OTControlArguments*)arguments
                  machineID:(NSString* _Nullable)machineID
                       json:(bool)json;

- (int)fetchAccountSettingsWithArguments:(OTControlArguments*)argumentsName
                                    json:(bool)json;
- (int)fetchAccountWideSettingsWithArguments:(OTControlArguments*)arguments
                                  useDefault:(bool)useDefault
                                  forceFetch:(bool)forceFetch
                                        json:(bool)json;

- (int)disableWalrusWithArguments:(OTControlArguments*)argumentsName
                          timeout:(NSTimeInterval)timeout;

- (int)enableWalrusWithArguments:(OTControlArguments*)argumentsName
                         timeout:(NSTimeInterval)timeout;

- (int)disableWebAccessWithArguments:(OTControlArguments*)argumentsName
                             timeout:(NSTimeInterval)timeout;

- (int)enableWebAccessWithArguments:(OTControlArguments*)argumentsName
                            timeout:(NSTimeInterval)timeout;

- (int)printAccountMetadataWithArguments:(OTControlArguments*)argumentsName
                                    json:(bool)json;

- (int)rerollWithArguments:(OTControlArguments*)argumentsName
                      json:(bool)json;

@end

NS_ASSUME_NONNULL_END
