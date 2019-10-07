
#import <Foundation/Foundation.h>
#import "keychain/escrowrequest/generated_source/SecEscrowPendingRecord.h"

NS_ASSUME_NONNULL_BEGIN

@interface SecEscrowPendingRecord (KeychainSupport)

- (BOOL)saveToKeychain:(NSError**)error;
- (BOOL)deleteFromKeychain:(NSError**)error;

+ (SecEscrowPendingRecord* _Nullable)loadFromKeychain:(NSString*)uuid error:(NSError**)error;
+ (NSArray<SecEscrowPendingRecord*>* _Nullable)loadAllFromKeychain:(NSError**)error;
@end

@interface SecEscrowPendingRecord (EscrowAttemptTimeout)
- (BOOL)escrowAttemptedWithinLastSeconds:(NSTimeInterval)timeInterval;
@end

NS_ASSUME_NONNULL_END

