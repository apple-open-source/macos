
#if OCTAGON

#import "keychain/ot/OctagonFlags.h"

@interface OctagonFlags ()
@property dispatch_queue_t queue;
@property NSMutableSet<OctagonFlag*>* flags;
@end

@implementation OctagonFlags

- (instancetype)initWithQueue:(dispatch_queue_t)queue
{
    if((self = [super init])) {
        _queue = queue;
        _flags = [NSMutableSet set];
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OctagonFlags: %@>", [self contentsAsString]];
}

- (NSString*)contentsAsString
{
    if(self.flags.count == 0) {
        return @"none";
    }
    return [[self.flags allObjects] componentsJoinedByString:@","];
}

- (NSArray<NSString*>*)dumpFlags
{
    return [self.flags allObjects];
}

- (BOOL)_onqueueContains:(nonnull OctagonFlag *)flag {
    dispatch_assert_queue(self.queue);
    return [self.flags containsObject:flag];
}

- (void)_onqueueSetFlag:(nonnull OctagonFlag *)flag {
    dispatch_assert_queue(self.queue);
    [self.flags addObject:flag];
}

- (void)setFlag:(nonnull OctagonFlag *)flag {
    dispatch_sync(self.queue, ^{
        [self _onqueueSetFlag:flag];
    });
}

- (void)_onqueueRemoveFlag:(nonnull OctagonFlag *)flag {
    dispatch_assert_queue(self.queue);
    [self.flags removeObject:flag];
}

@end

#endif
