
#import <Foundation/Foundation.h>

#import "keychain/ot/OTControl.h"

#import <AppleFeatures/AppleFeatures.h>

NS_ASSUME_NONNULL_BEGIN


@interface OTControlCLI : NSObject
@property OTControl* control;

- (instancetype)initWithOTControl:(OTControl*)control;

- (int)startOctagonStateMachine:(NSString* _Nullable)container context:(NSString *)contextID;

- (int)signIn:(NSString *)altDSID container:(NSString * _Nullable)container context:(NSString *)contextID;

- (int)signOut:(NSString * _Nullable)container context:(NSString *)contextID;

- (int)depart:(NSString * _Nullable)container context:(NSString *)contextID;

- (int)resetOctagon:(NSString* _Nullable)container context:(NSString *)contextID altDSID:(NSString *)altDSID timeout:(NSTimeInterval)timeout;

- (int)resetProtectedData:(NSString* _Nullable)container context:(NSString *)contextID altDSID:(NSString *)altDSID appleID:(NSString * _Nullable)appleID dsid:(NSString *_Nullable)dsid;

- (int)status:(NSString * _Nullable)container context:(NSString *)contextID json:(bool)json;

- (int)recoverUsingBottleID:(NSString *)bottleID
                     entropy:(NSData *)entropy
                     altDSID:(NSString *)altDSID
               containerName:(NSString* _Nullable)containerName
                     context:(NSString *)context
                     control:(OTControl *)control;

- (int)fetchAllBottles:(NSString *)altDSID
          containerName:(NSString* _Nullable)containerName
                context:(NSString *)context
                control:(OTControl *)control;

- (int)fetchEscrowRecords:(NSString * _Nullable)container context:(NSString *)contextID;
- (int)fetchAllEscrowRecords:(NSString* _Nullable)container context:(NSString*)contextID;

- (int)healthCheck:(NSString * _Nullable)container context:(NSString *)contextID skipRateLimitingCheck:(BOOL)skipRateLimitingCheck;
- (int)refetchCKKSPolicy:(NSString* _Nullable)container context:(NSString *)contextID;

- (int)tapToRadar:(NSString *)action description:(NSString *)description radar:(NSString *)radar;

- (int)performEscrowRecovery:(NSString * _Nullable)container
                      context:(NSString *)contextID
                     recordID:(NSString *)recordID
                      appleID:(NSString *)appleID
                       secret:(NSString *)secret;

- (int)performSilentEscrowRecovery:(NSString * _Nullable)container context:(NSString *)contextID appleID:(NSString *)appleID secret:(NSString *)secret;

- (int)tlkRecoverability:(NSString * _Nullable)container context:(NSString *)contextID;

- (int)setUserControllableViewsSyncStatus:(NSString * _Nullable)containerName
                                 contextID:(NSString *)contextID
                                   enabled:(BOOL)enabled;

- (int)fetchUserControllableViewsSyncStatus:(NSString * _Nullable)containerName
                                   contextID:(NSString *)contextID;

- (int)resetAccountCDPContentsWithContainerName:(NSString* _Nullable)containerName
                                       contextID:(NSString *)contextID;

- (int)createCustodianRecoveryKeyWithContainerName:(NSString* _Nullable)containerName
                                         contextID:(NSString *)contextID
                                              json:(bool)json
                                           timeout:(NSTimeInterval)timeout;

- (int)joinWithCustodianRecoveryKeyWithContainerName:(NSString* _Nullable)containerName
                                           contextID:(NSString *)contextID
                                         wrappingKey:(NSString*)wrappingKey
                                          wrappedKey:(NSString*)wrappedKey
                                          uuidString:(NSString*)uuidString
                                             timeout:(NSTimeInterval)timeout;

- (int)preflightJoinWithCustodianRecoveryKeyWithContainerName:(NSString* _Nullable)containerName
                                                    contextID:(NSString *)contextID
                                                  wrappingKey:(NSString*)wrappingKey
                                                   wrappedKey:(NSString*)wrappedKey
                                                   uuidString:(NSString*)uuidString
                                                      timeout:(NSTimeInterval)timeout;

- (int)removeCustodianRecoveryKeyWithContainerName:(NSString* _Nullable)containerName
                                         contextID:(NSString *)contextID
                                        uuidString:(NSString*)uuidString
                                           timeout:(NSTimeInterval)timeout;

- (int)createInheritanceKeyWithContainerName:(NSString* _Nullable)containerName
                                   contextID:(NSString *)contextID
                                        json:(bool)json
                                     timeout:(NSTimeInterval)timeout;

- (int)generateInheritanceKeyWithContainerName:(NSString* _Nullable)containerName
                                     contextID:(NSString *)contextID
                                          json:(bool)json
                                       timeout:(NSTimeInterval)timeout;

- (int)storeInheritanceKeyWithContainerName:(NSString* _Nullable)containerName
                                  contextID:(NSString *)contextID
                                wrappingKey:(NSString*)wrappingKey
                                 wrappedKey:(NSString*)wrappedKey
                                 uuidString:(NSString*)uuidString
                                    timeout:(NSTimeInterval)timeout;

- (int)joinWithInheritanceKeyWithContainerName:(NSString* _Nullable)containerName
                                     contextID:(NSString *)contextID
                                   wrappingKey:(NSString*)wrappingKey
                                    wrappedKey:(NSString*)wrappedKey
                                    uuidString:(NSString*)uuidString
                                       timeout:(NSTimeInterval)timeout;

- (int)preflightJoinWithInheritanceKeyWithContainerName:(NSString* _Nullable)containerName
                                              contextID:(NSString *)contextID
                                            wrappingKey:(NSString*)wrappingKey
                                             wrappedKey:(NSString*)wrappedKey
                                             uuidString:(NSString*)uuidString
                                                timeout:(NSTimeInterval)timeout;

- (int)removeInheritanceKeyWithContainerName:(NSString* _Nullable)containerName
                                   contextID:(NSString *)contextID
                                  uuidString:(NSString*)uuidString
                                     timeout:(NSTimeInterval)timeout;


@end

NS_ASSUME_NONNULL_END
