
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

- (int)resetOctagon:(OTControlArguments*)arguments timeout:(NSTimeInterval)timeout;

- (int)resetProtectedData:(OTControlArguments*)arguments appleID:(NSString * _Nullable)appleID dsid:(NSString *_Nullable)dsid;

- (int)status:(OTControlArguments*)arguments json:(bool)json;

- (int)recoverUsingBottleID:(NSString *)bottleID
                    entropy:(NSData*)entropy
                  arguments:(OTControlArguments*)arguments
                    control:(OTControl*)control;

- (int)fetchAllBottles:(OTControlArguments*)arguments
               control:(OTControl *)control;

- (int)fetchEscrowRecords:(OTControlArguments*)arguments json:(bool)json;
- (int)fetchAllEscrowRecords:(OTControlArguments*)arguments json:(bool)json;

- (int)healthCheck:(OTControlArguments*)arguments skipRateLimitingCheck:(BOOL)skipRateLimitingCheck;
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

- (int)resetAccountCDPContentsWithArguments:(OTControlArguments*)argumentsName;

- (int)createCustodianRecoveryKeyWithArguments:(OTControlArguments*)argumentsName
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

- (int)createInheritanceKeyWithArguments:(OTControlArguments*)argumentsName
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

- (int)setMachineIDOverride:(OTControlArguments*)arguments
                  machineID:(NSString*)machineID
                       json:(bool)json;


@end

NS_ASSUME_NONNULL_END
