
#if OCTAGON

#import "keychain/ot/OctagonFlags.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ckks/CKKSCondition.h"

@interface OctagonFlags ()
@property dispatch_queue_t queue;
@property NSMutableSet<OctagonFlag*>* flags;
@property (readonly) NSSet* allowableFlags;
@end

@implementation OctagonFlags

- (instancetype)initWithQueue:(dispatch_queue_t)queue
                       flags:(NSSet<OctagonFlag*>*)possibleFlags
{
    if((self = [super init])) {
        _queue = queue;
        _flags = [NSMutableSet set];
        _flagConditions = [[NSMutableDictionary alloc] init];
        _allowableFlags = possibleFlags;

        [possibleFlags enumerateObjectsUsingBlock:^(OctagonState * _Nonnull obj, BOOL * _Nonnull stop) {
            self.flagConditions[obj] = [[CKKSCondition alloc] init];
        }];
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

- (CKKSCondition*)conditionForFlag:(OctagonFlag*)flag {
    return self.flagConditions[flag];
}

- (void)setFlag:(nonnull OctagonFlag *)flag {
    dispatch_sync(self.queue, ^{
        [self _onqueueSetFlag:flag];
    });
}

- (void)_onqueueRemoveFlag:(nonnull OctagonFlag *)flag {
    dispatch_assert_queue(self.queue);

    NSAssert([self.allowableFlags containsObject:flag], @"state machine tried to handle unknown flag %@", flag);

    [self.flags removeObject:flag];
    [self.flagConditions[flag] fulfill];
    self.flagConditions[flag] = [[CKKSCondition alloc]init];
}

@end

#endif
