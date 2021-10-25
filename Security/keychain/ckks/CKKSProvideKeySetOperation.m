
#if OCTAGON

#import "CKKSProvideKeySetOperation.h"

@interface CKKSProvideKeySetOperation ()
@property (nullable) NSDictionary<CKRecordZoneID*, CKKSCurrentKeySet*>* keysets;
@property dispatch_queue_t queue;

@property (nullable) NSOperation* startDependency;
@end

@implementation CKKSProvideKeySetOperation
@synthesize keysets = _keysets;
@synthesize intendedZoneIDs = _intendedZoneIDs;

- (instancetype)initWithIntendedZoneIDs:(NSSet<CKRecordZoneID*>*)intendedZoneIDs
{
    if((self = [super init])) {
        _intendedZoneIDs = intendedZoneIDs;
        _keysets = nil;
        _startDependency = [NSBlockOperation blockOperationWithBlock:^{}];
        _startDependency.name = @"key-set-provided";

        _queue = dispatch_queue_create("key-set-queue", DISPATCH_QUEUE_SERIAL);

        [self addDependency:_startDependency];
    }
    return self;
}

- (void)provideKeySets:(NSDictionary<CKRecordZoneID*, CKKSCurrentKeySet*>*)keysets
{
    // Ensure that only one keyset groupt is provided through each operation
    dispatch_sync(self.queue, ^{
        if(!self.keysets) {
            self.keysets = keysets;
            if(self.startDependency) {
                // Create a new queue here, just to be safe in case someone is waiting
                NSOperationQueue* queue = [[NSOperationQueue alloc] init];
                [queue addOperation:self.startDependency];
                self.startDependency = nil;
            }
        }
    });
}

@end

#endif // OCTAGON
