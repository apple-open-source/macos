
#if OCTAGON

#import "CKKSProvideKeySetOperation.h"

@interface CKKSProvideKeySetOperation ()
@property (nullable) CKKSCurrentKeySet* keyset;
@property dispatch_queue_t queue;

@property (nullable) NSOperation* startDependency;
@end

@implementation CKKSProvideKeySetOperation
@synthesize zoneName = _zoneName;
@synthesize keyset = _keyset;

- (instancetype)initWithZoneName:(NSString*)zoneName
{
    if((self = [super init])) {
        _zoneName = zoneName;
        _keyset = nil;
        _startDependency = [NSBlockOperation blockOperationWithBlock:^{}];
        _startDependency.name = @"key-set-provided";

        _queue = dispatch_queue_create("key-set-queue", DISPATCH_QUEUE_SERIAL);

        [self addDependency:_startDependency];
    }
    return self;
}

- (void)provideKeySet:(CKKSCurrentKeySet *)keyset
{
    // Ensure that only one keyset is provided through each operation
    dispatch_sync(self.queue, ^{
        if(!self.keyset) {
            self.keyset = keyset;
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
