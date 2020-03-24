
#import <Foundation/Foundation.h>

#import "keychain/ot/OTControl.h"

NS_ASSUME_NONNULL_BEGIN


@interface OTControlCLI : NSObject
@property OTControl* control;

- (instancetype)initWithOTControl:(OTControl*)control;

- (long)startOctagonStateMachine:(NSString*)container context:(NSString*)contextID;

- (long)signIn:(NSString*)altDSID container:(NSString* _Nullable)container context:(NSString*)contextID;

- (long)signOut:(NSString* _Nullable)container context:(NSString*)contextID;

- (long)depart:(NSString* _Nullable)container context:(NSString*)contextID;

- (long)resetOctagon:(NSString*)container context:(NSString*)contextID altDSID:(NSString*)altDSID;

- (long)resetProtectedData:(NSString*)container context:(NSString*)contextID altDSID:(NSString*)altDSID appleID:(NSString*)appleID dsid:(NSString*)dsid;

- (long)status:(NSString* _Nullable)container context:(NSString*)contextID json:(bool)json;

- (long)recoverUsingBottleID:(NSString*)bottleID
                     entropy:(NSData*)entropy
                     altDSID:(NSString*)altDSID
               containerName:(NSString*)containerName
                     context:(NSString*)context
                     control:(OTControl*)control;

- (long)fetchAllBottles:(NSString*)altDSID
          containerName:(NSString*)containerName
                context:(NSString*)context
                control:(OTControl*)control;

- (long)healthCheck:(NSString* _Nullable)container context:(NSString*)contextID skipRateLimitingCheck:(BOOL)skipRateLimitingCheck;
- (long)refetchCKKSPolicy:(NSString*)container context:(NSString*)contextID;

- (long)tapToRadar:(NSString *)action description:(NSString *)description radar:(NSString *)radar;

@end

NS_ASSUME_NONNULL_END
