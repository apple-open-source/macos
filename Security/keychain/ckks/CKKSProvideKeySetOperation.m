
#if OCTAGON

#import "CKKSProvideKeySetOperation.h"

@interface CKKSProvideKeySetOperation ()
@property (nullable) CKKSCurrentKeySet* keyset;

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

        [self addDependency:_startDependency];
    }
    return self;
}

- (instancetype)initWithZoneName:(NSString*)zoneName keySet:(CKKSCurrentKeySet*)set
{
    if((self = [super init])) {
        _zoneName = zoneName;
        _keyset = set;
        _startDependency = nil;
    }
    return self;
}

- (void)provideKeySet:(CKKSCurrentKeySet *)keyset
{
    self.keyset = keyset;
    if(self.startDependency) {
        [[NSOperationQueue currentQueue] addOperation:self.startDependency];
        self.startDependency = nil;
    }
}
@end

#endif // OCTAGON
