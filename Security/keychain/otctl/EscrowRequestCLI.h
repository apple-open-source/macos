
#import <Foundation/Foundation.h>
#import "keychain/escrowrequest/Framework/SecEscrowRequest.h"

NS_ASSUME_NONNULL_BEGIN

@interface EscrowRequestCLI : NSObject
@property SecEscrowRequest* escrowRequest;

- (instancetype)initWithEscrowRequest:(SecEscrowRequest*)er;

- (int)trigger;
- (int)status;
- (int)reset;
- (int)storePrerecordsInEscrow;

@end

NS_ASSUME_NONNULL_END
