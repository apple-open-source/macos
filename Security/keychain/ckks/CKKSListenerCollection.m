
#if OCTAGON

#import "keychain/ckks/CKKSListenerCollection.h"
#import "keychain/ot/ObjCImprovements.h"

@interface CKKSListenerCollection ()
@property NSString* name;
@property NSMapTable<dispatch_queue_t, id>* listeners;
@end

@implementation CKKSListenerCollection

- (instancetype)initWithName:(NSString*)name
{
    if((self = [super init])) {
        _name = name;
        // Backwards from how we'd like, but it's the best way to have weak pointers to ListenerTypes.
        _listeners = [NSMapTable strongToWeakObjectsMapTable];
    }
    return self;
}

- (NSString*)description
{
    @synchronized(self.listeners) {
        return [NSString stringWithFormat:@"<CKKSListenerCollection(%@): %@>", self.name, [[self.listeners objectEnumerator] allObjects]];
    }
}

- (void)registerListener:(id)listener
{
    @synchronized(self.listeners) {
        bool alreadyRegisteredListener = false;
        NSEnumerator *enumerator = [self.listeners objectEnumerator];
        id value;

        while ((value = [enumerator nextObject])) {
            // actually use pointer comparison
            alreadyRegisteredListener |= (value == listener);
        }

        if(listener && !alreadyRegisteredListener) {
            NSString* queueName = [NSString stringWithFormat: @"%@-%@", self.name, listener];

            dispatch_queue_t objQueue = dispatch_queue_create([queueName UTF8String], DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
            [self.listeners setObject: listener forKey: objQueue];
        }
    }
}

- (void)iterateListeners:(void (^)(id))block
{
    @synchronized(self.listeners) {
        NSEnumerator *enumerator = [self.listeners keyEnumerator];
        dispatch_queue_t dq;

        // Queue up the changes for each listener.
        while ((dq = [enumerator nextObject])) {
            id listener = [self.listeners objectForKey: dq];
            WEAKIFY(listener);

            if(listener) {
                dispatch_async(dq, ^{
                        STRONGIFY(listener);
                        block(listener);
                });
            }
        }
    }
}

@end

#endif // OCTAGON
