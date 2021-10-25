
#if OCTAGON

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#import "keychain/ckks/CKKSSQLDatabaseObject.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSSecDbAdapter : NSObject <CKKSDatabaseProviderProtocol>

- (instancetype)initWithQueue:(dispatch_queue_t)queue;

// If you really know what you're doing and already have a SecDbConnectionRef, use this.
- (bool)dispatchSyncWithConnection:(SecDbConnectionRef _Nonnull)dbconn
                    readWriteTxion:(BOOL)readWriteTxion
                             block:(CKKSDatabaseTransactionResult (^)(void))block;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
