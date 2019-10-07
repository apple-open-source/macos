
#import <Foundation/Foundation.h>
#import "keychain/escrowrequest/Framework/SecEscrowRequest.h"

NS_ASSUME_NONNULL_BEGIN

@interface EscrowRequestCLI : NSObject
@property SecEscrowRequest* escrowRequest;

- (instancetype)initWithEscrowRequest:(SecEscrowRequest*)er;

- (long)trigger;
- (long)status;
- (long)reset;
- (long)storePrerecordsInEscrow;

@end

NS_ASSUME_NONNULL_END
